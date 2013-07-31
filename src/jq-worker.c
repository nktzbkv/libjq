#include "jq-private.h"
#include <stdlib.h>

/*-----------------------------------------------------------------------------
  Internals.
-----------------------------------------------------------------------------*/

#if 0
#define LOG( a ) printf a
#else
#define LOG( a )
#endif

typedef struct jq_worker jq_worker;

struct jq_worker {
  jq_object object;
  
  /* Request queue. */
  jq_queue_t queue;
  
  /* Spinlock for counters. */
  pthread_spinlock_t lock;
  
  /**
    Number of threads requested by client.
  */
  size_t requested_threads;
  
  /**
    Number of threads should run now.
    This counter increments each time thread created and decrements each time thread-stop request sent.
  */
  size_t launched_threads;
  
  /**
    Number of threads currently working.
    This counter increments each time thread created and decrements when thread actually stopped.
  */
  size_t working_threads;
};

/* Destruct and dealloc worker. */
static void jq_worker_dealloc( jq_worker* worker ) {
  jq_release( worker->queue );
  pthread_spin_destroy( &worker->lock );
  free( worker );
}

/* Is is good to dealloc worker now? */
static int jq_worker_can_dealloc( jq_worker* worker ) {
  return worker->object.refs < 1 && worker->working_threads < 1;
}

/* Dealloc worker if it possible. */
static void jq_worker_dealloc_if_possible( jq_worker* worker ) {
  if( jq_worker_can_dealloc( worker ) )
    jq_worker_dealloc( worker );
}

/* Called by working thread when it's ready to work. */
static inline void jq_worker_thread_added( jq_worker* worker ) {
  LOG(( "jq_worker_thread_added\n" ));
}

/* Called by working thread right before it quits. */
static inline void jq_worker_thread_removed( jq_worker* worker ) {
  LOG(( "jq_worker_thread_removed\n" ));
  
  pthread_spin_lock( &worker->lock );
  worker->working_threads--;
  pthread_spin_unlock( &worker->lock );
  
  jq_worker_dealloc_if_possible( worker );
}

/* Working thread code. */
static void* jq_worker_thread_main( void* arg ) {
  jq_worker* worker = (jq_worker*)arg;
  
  jq_worker_thread_added( worker );
  jq_queue_loop( worker->queue );
  jq_worker_thread_removed( worker );
  
  return NULL;
}

/* Start one thread. */
static inline int jq_worker_start_thread( jq_worker* worker ) {
  pthread_t t;
  
  if( pthread_create( &t, NULL, jq_worker_thread_main, worker ) == 0 ) {
    worker->working_threads++;
    worker->launched_threads++;
    return 1;
  }
  
  return 0;
}

/* Stop one thread. Put quit req to request queue. */
static inline void jq_worker_stop_thread( jq_worker* worker ) {
  jq_queue_stop( worker->queue );
  worker->launched_threads--;
}

/* Start or stop threads to sync requested_threads and launched_threads. */
static void jq_worker_manage_threads( jq_worker* worker ) {
  size_t threads;
  
  pthread_spin_lock( &worker->lock );
  
  threads = worker->requested_threads > 1 ? worker->requested_threads : 1;
  
  while( worker->launched_threads > threads ) {
    jq_worker_stop_thread( worker );
  }
  
  while( worker->launched_threads < threads ) {
    if( !jq_worker_start_thread( worker ) )
      break;
  }
  
  pthread_spin_unlock( &worker->lock );
}

static void jq_worker_vtable_destroy( void* ptr ) {
  jq_worker_t worker = (jq_worker_t)ptr;
  
  pthread_spin_lock( &worker->lock );
  
  if( jq_worker_can_dealloc( worker ) ) {
    pthread_spin_unlock( &worker->lock );
    jq_worker_dealloc( worker );
  }
  else {
    /* Stop all working threads. */
    while( worker->launched_threads > 0 ) {
      worker->launched_threads--;
      jq_worker_stop_thread( worker );
    }
    
    pthread_spin_unlock( &worker->lock );
  }
}

static jq_object_vtable worker_vtable = {
  jq_worker_vtable_destroy
};

/*-----------------------------------------------------------------------------
  Public.
-----------------------------------------------------------------------------*/

jq_worker_t jq_worker_create( jq_queue_t queue, size_t threads ) {
  jq_worker_t worker = (jq_worker_t)malloc( sizeof(jq_worker) );
  
  if( worker ) {
    jq_object_init( &worker->object, &worker_vtable );
    
    worker->requested_threads = threads;
    worker->launched_threads = 0;
    worker->working_threads = 0;
    
    pthread_spin_init( &worker->lock, 0 );
    
    if( queue ) {
      jq_retain( queue );
      worker->queue = queue;
    }
    else {
      worker->queue = jq_queue_create();
      
      if( !worker->queue )
        goto fail;
    }
    
    /* Launch threads. */
    jq_worker_manage_threads( worker );
    
    /* If no threads running - we can't operate normally. */
    if( worker->launched_threads < 1 )
      goto fail;
  }
  
  return worker;
  
fail:
  jq_worker_dealloc( worker );
  return NULL;
}

void jq_worker_set_threads( jq_worker_t worker, size_t threads ) {
  worker->requested_threads = threads;
  jq_worker_manage_threads( worker );
}

void jq_worker_async(
  jq_worker_t worker,
  jq_handler_t handler,
  void* context )
{
  jq_queue_submit( worker->queue, NULL, handler, context );
}

void jq_worker_async_group(
  jq_worker_t worker,
  jq_group_t group,
  jq_handler_t handler,
  void* context )
{
  jq_queue_submit( worker->queue, group, handler, context );
}

void jq_worker_sync(
  jq_worker_t worker,
  jq_handler_t handler,
  void* context )
{
  jq_group_t group = jq_group_create();
  if( group ) {
    jq_queue_submit( worker->queue, group, handler, context );
    jq_group_wait( group );
  }
}
