#include "jq-private.h"
#include <string.h>
#include <stdio.h>

/*-----------------------------------------------------------------------------
  Request object.
-----------------------------------------------------------------------------*/

typedef struct jq_req jq_req;

/** Request. */
struct jq_req {
  /** Next req in queue. */
  jq_req* next;
  
  /** Group this request assigned to. */
  jq_group_t group;
  
  /** Pointer to function that do the work. */
  jq_handler_t handler;
  
  /** */
  void* context;
};

static jq_fsa req_allocator = JQ_FSA_INITIALIZER( sizeof(jq_req), 0 );

jq_req* jq_req_create( jq_group_t group, jq_handler_t handler, void* context ) {
  jq_req* req = jq_fsa_alloc( &req_allocator );
  
  if( req ) {
    jq_retain( group );
    jq_group_enter( group );
    
    req->group = group;
    req->handler = handler;
    req->context = context;
  }
  
  return req;
}

void jq_req_destroy( jq_req* req ) {
  if( req->group ) {
    jq_group_leave( req->group );
    jq_release( req->group );
  }
  
  jq_fsa_free( &req_allocator, req );
}

/*-----------------------------------------------------------------------------
  Internals.
-----------------------------------------------------------------------------*/

typedef struct jq_queue jq_queue;

/** Request queue. */
struct jq_queue {
  jq_object object;
  
  /** First req in queue. */
  jq_req* first;
  
  /** Last req in queue. */
  jq_req* last;
  
  /** Concurrency spinlock. */
  pthread_spinlock_t lock;
  
  /** Mutex for condition variable. */
  pthread_mutex_t mutex;
  
  /** Condition to signal when new req was added to queue. */
  pthread_cond_t cond;
  
  /** Number of reqs in queue. */
  volatile size_t count;
};

static inline void jq_queue_lockless_empty( jq_queue* queue ) {
  jq_req* next;
  jq_req* req = queue->first;
  
  while( req != NULL ) {
    next = req->next;
    jq_req_destroy( req );
    req = next;
  }
  
  queue->first = NULL;
  queue->last = NULL;
}

static inline void jq_queue_lockless_put_last( jq_queue* queue, jq_req* req ) {
  req->next = NULL;
  
  if( queue->last ) {
    queue->last->next = req;
  }
  else {
    queue->first = req;
  }
  
  queue->last = req;
  queue->count++;
}

static inline void jq_queue_lockless_put_first( jq_queue* queue, jq_req* req ) {
  if( queue->first ) {
    req->next = queue->first;
  }
  else {
    req->next = NULL;
    queue->last = req;
  }
  
  queue->first = req;
  queue->count++;
}

static inline jq_req* jq_queue_lockless_get( jq_queue* queue ) {
  jq_req* req = queue->first;
  
  if( req ) {
    if( !(queue->first = req->next) )
       queue->last = NULL;
    
    queue->count--;
  }
  
  return req;
}

static void jq_queue_put_last( jq_queue* queue, jq_req* req ) {
  pthread_spin_lock( &queue->lock );
  jq_queue_lockless_put_last( queue, req );
  pthread_spin_unlock( &queue->lock );
  pthread_cond_signal( &queue->cond );
}

static void jq_queue_put_first( jq_queue* queue, jq_req* req ) {
  pthread_spin_lock( &queue->lock );
  jq_queue_lockless_put_first( queue, req );
  pthread_spin_unlock( &queue->lock );
  pthread_cond_signal( &queue->cond );
}

static jq_req* jq_queue_get( jq_queue* queue ) {
  jq_req* req;
  pthread_spin_lock( &queue->lock );
  req = jq_queue_lockless_get( queue );
  pthread_spin_unlock( &queue->lock );
  return req;
}

static jq_req* jq_queue_wait( jq_queue* queue ) {
  jq_req* req;
  
  if( !(req = jq_queue_get( queue )) ) {
    pthread_mutex_lock( &queue->mutex );
    
    while( !(req = jq_queue_get( queue )) ) {
      pthread_cond_wait( &queue->cond, &queue->mutex );
    }
    
    pthread_mutex_unlock( &queue->mutex );
  }
  
  return req;
}

static void jq_queue_quit_proc( void* context ) {
}

static void jq_queue_vtable_destroy( void* object ) {
  jq_queue* queue = (jq_queue*)object;
  jq_queue_empty( queue );
  pthread_cond_destroy( &queue->cond );
  pthread_mutex_destroy( &queue->mutex );
  pthread_spin_destroy( &queue->lock );
  
  //printf( "%p queue destroyed!\n", queue );
}

static jq_fsa queue_allocator = JQ_FSA_INITIALIZER( sizeof(jq_queue), 0 );

static jq_object_vtable queue_vtable = {
  jq_queue_vtable_destroy
};

/*-----------------------------------------------------------------------------
  Public.
-----------------------------------------------------------------------------*/

jq_queue_t jq_queue_create() {
  jq_queue* queue = (jq_queue*)jq_fsa_alloc( &queue_allocator );
  
  if( queue ) {
    memset( queue, 0, sizeof(*queue) );
    
    jq_object_init( &queue->object, &queue_vtable );
    
    if( pthread_spin_init( &queue->lock, 0 ) != 0 )
      goto fail;
    
    if( pthread_mutex_init( &queue->mutex, NULL ) != 0 )
      goto fail;
    
    if( pthread_cond_init( &queue->cond, NULL ) != 0 )
      goto fail;
  }
  
  //printf( "%p queue created!\n", queue );
  
  return queue;
  
fail:
  jq_fsa_free( &queue_allocator, queue );
  return NULL;
}

void jq_queue_empty( jq_queue_t queue ) {
  pthread_spin_lock( &queue->lock );
  jq_queue_lockless_empty( queue );
  pthread_spin_unlock( &queue->lock );
}

int jq_queue_submit( jq_queue_t queue, jq_group_t group, jq_handler_t handler, void* context ) {
  jq_req* req = jq_req_create( group, handler, context );
  if( !req ) return 0;
  
  jq_queue_put_last( queue, req );
  return 1;
}

int jq_queue_stop( jq_queue_t queue ) {
  jq_req* req = jq_req_create( NULL, jq_queue_quit_proc, NULL );
  if( !req ) return 0;
  
  jq_queue_put_first( queue, req );
  return 1;
}

int jq_queue_poll( jq_queue_t queue ) {
  while( 1 ) {
    jq_req* req = jq_queue_get( queue );
    if( !req ) return 1;
    
    if( req->handler == jq_queue_quit_proc ) {
      jq_req_destroy( req );
      return 0;
    }
    
    if( req->handler )
      req->handler( req->context );
    
    jq_req_destroy( req );
  }
}

void jq_queue_loop( jq_queue_t queue ) {
  while( 1 ) {
    jq_req* req = jq_queue_wait( queue );
    
    if( req->handler == jq_queue_quit_proc ) {
      jq_req_destroy( req );
      break;
    }
    
    if( req->handler )
      req->handler( req->context );
    
    jq_req_destroy( req );
  }
}

size_t jq_queue_get_length( jq_queue_t queue ) {
  size_t length;
  
  pthread_spin_lock( &queue->lock );
  length = queue->count;
  pthread_spin_unlock( &queue->lock );
  
  return length;
}
