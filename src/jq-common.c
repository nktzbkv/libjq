#include "jq-private.h"

#define OBJECT_MAGIC 0xFADEDFAC

void jq_object_init( jq_object* obj, jq_object_vtable* vtable ) {
  obj->magic = OBJECT_MAGIC;
  obj->refs = 1;
  obj->vtable = vtable;
}

static inline jq_object* jq_object_get( void* ptr ) {
  jq_object* obj = (jq_object*)ptr;
  return (obj && obj->magic == OBJECT_MAGIC) ? obj : NULL;
}

void jq_retain( void* ptr ) {
  jq_object* obj = jq_object_get( ptr );
  if( !obj ) return;
  
  jq_atomic_add( &obj->refs, 1 );
}

void jq_release( void* ptr ) {
  jq_object* obj = jq_object_get( ptr );
  if( !obj ) return;
  
  if( jq_atomic_sub( &obj->refs, 1 ) == 1 ) {
    obj->vtable->destroy( ptr );
  }
}

/*-----------------------------------------------------------------------------
  Once.
-----------------------------------------------------------------------------*/

int jq_once( jq_once_t* once ) {
  return jq_atomic_cas( once, 0, 1 ) == 0;
}

/*-----------------------------------------------------------------------------
  pthread_spinlock emulation if platform don't support it.
-----------------------------------------------------------------------------*/

#ifdef JQ_PTHREAD_SPINLOCK_EMULATED

#include <errno.h>
#include <sched.h>

int pthread_spin_init( pthread_spinlock_t* lock, int pshared ) {
  if( !lock ) return EINVAL;
  *lock = 0;
  return 0;
}

int pthread_spin_destroy( pthread_spinlock_t* lock ) {
  return 0;
}

int pthread_spin_lock( pthread_spinlock_t* lock ) {
  while( 1 ) {
    if( jq_atomic_cas( lock, 0, 1 ) == 0 )
      break;
    sched_yield();
  }
  return 0;
}

int pthread_spin_trylock( pthread_spinlock_t* lock ) {
  return (jq_atomic_cas( lock, 0, 1 ) == 0) ? 0 : EBUSY;
}

int pthread_spin_unlock( pthread_spinlock_t* lock ) {
  jq_atomic_cas( lock, 1, 0 );
  return 0;
}

#endif
