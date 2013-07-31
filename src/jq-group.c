#include "jq-private.h"
#include <string.h>
#include <stdio.h>

typedef struct jq_group jq_group;

struct jq_group {
  jq_object object;
  
  /** Mutex for condition variable. */
  pthread_mutex_t mutex;
  
  /** Condition to signal when new req was added to queue. */
  pthread_cond_t cond;
  
  /** Number of group members. */
  size_t members;
};

static void jq_group_vtable_destroy( void* object ) {
  jq_group* group = (jq_group*)object;
  pthread_cond_destroy( &group->cond );
  pthread_mutex_destroy( &group->mutex );
  //printf( "%p group destroyed!\n", group );
}

static jq_fsa group_allocator = JQ_FSA_INITIALIZER( sizeof(jq_group), 0 );

static jq_object_vtable group_vtable = {
  jq_group_vtable_destroy
};

jq_group_t jq_group_create() {
  jq_group_t group = (jq_group_t)jq_fsa_alloc( &group_allocator );
  
  if( group ) {
    memset( group, 0, sizeof(*group) );
    
    jq_object_init( &group->object, &group_vtable );
    
    if( pthread_mutex_init( &group->mutex, NULL ) != 0 )
      goto fail;
    
    if( pthread_cond_init( &group->cond, NULL ) != 0 )
      goto fail;
  }
  
  //printf( "%p group created!\n", group );
  
  return group;
  
fail:
  jq_fsa_free( &group_allocator, group );
  return NULL;
}

void jq_group_enter( jq_group_t group ) {
  if( !group ) return;
  
  //printf( "%p group entered!\n", group );
  pthread_mutex_lock( &group->mutex );
  group->members++;
  pthread_mutex_unlock( &group->mutex );
}

void jq_group_leave( jq_group_t group ) {
  if( !group ) return;
  
  //printf( "%p group leaved!\n", group );
  pthread_mutex_lock( &group->mutex );
  if( group->members-- == 1 ) {
    pthread_cond_signal( &group->cond );
  }
  pthread_mutex_unlock( &group->mutex );
}

void jq_group_wait( jq_group_t group ) {
  if( !group ) return;
  
  pthread_mutex_lock( &group->mutex );
  
  while( group->members != 0 ) {
    //printf( "%p group wating (%lu)...\n", group, group->members );
    pthread_cond_wait( &group->cond, &group->mutex );
  }
  
  pthread_mutex_unlock( &group->mutex );
}
