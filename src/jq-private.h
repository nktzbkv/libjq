#ifndef _JQ_ATOMIC_H_
#define _JQ_ATOMIC_H_

#include "jq.h"
#include <pthread.h>

/*-----------------------------------------------------------------------------
  pthread_spinlock emulation if platform don't support it.
-----------------------------------------------------------------------------*/

#if !defined(PTHREAD_SPINLOCK_INITIALIZER)
#define JQ_PTHREAD_SPINLOCK_EMULATED

typedef volatile int pthread_spinlock_t;

int pthread_spin_init( pthread_spinlock_t* lock, int pshared );
int pthread_spin_destroy( pthread_spinlock_t* lock );
int pthread_spin_lock( pthread_spinlock_t* lock );
int pthread_spin_trylock( pthread_spinlock_t* lock );
int pthread_spin_unlock( pthread_spinlock_t* lock );

#define PTHREAD_SPINLOCK_INITIALIZER 0

#endif

/*-----------------------------------------------------------------------------
  Common.
-----------------------------------------------------------------------------*/

typedef struct jq_object_vtable jq_object_vtable;
typedef struct jq_object jq_object;

struct jq_object_vtable {
  void (*destroy)( void* );
};

struct jq_object {
  int magic;
  jq_object_vtable* vtable;
  size_t refs;
};

void jq_object_init( jq_object* obj, jq_object_vtable* table );

/*-----------------------------------------------------------------------------
  Fixed size allocator.
-----------------------------------------------------------------------------*/

typedef struct jq_fsa jq_fsa;

/**
  Fixed size allocator (FSA).
  Allocates blocks of fixed size using one malloc for multiple blocks.
*/
struct jq_fsa {
  /** Size of each allocated memory block */
  size_t size;
  
  /** Number of blocks per malloc */
  size_t blocks_per_chunk;
  
  /** Single linked list of allocated chunks */
  void *first_chunk;
  
  /** Single linked list of free blocks */
  void *first_free;
  
  /** Spinlock for concurrency. */
  pthread_spinlock_t lock;
};

/**
  Fixed size allocator static initializer.
*/
#define JQ_FSA_INITIALIZER( size, blocks_per_chunk ) { \
  size < sizeof(void*) ? sizeof(void*) : size, \
  blocks_per_chunk < 8 ? 8 : blocks_per_chunk, \
  NULL, NULL, \
  PTHREAD_SPINLOCK_INITIALIZER \
}

void jq_fsa_init( jq_fsa* fsa, size_t size, size_t blocks_per_chunk );
void jq_fsa_destroy( jq_fsa* fsa );
void* jq_fsa_alloc( jq_fsa* fsa );
void jq_fsa_free( jq_fsa* fsa, void* ptr );
void jq_fsa_free_all( jq_fsa* fsa );

/*-----------------------------------------------------------------------------
  Atomic.
-----------------------------------------------------------------------------*/

#if defined(__GNUC__) && (__GNUC__ >= 4)

  #define jq_atomic_cas( v, old, new ) \
    __sync_val_compare_and_swap( (v), (old), (new) )
  
  #define jq_atomic_add( v, value ) \
    __sync_fetch_and_add( (v), (value) )
  
  #define jq_atomic_sub( v, value ) \
    __sync_fetch_and_sub( (v), (value) )

/*
#elif defined(_MSC_VER)

#include <intrin.h>
#define jq_atomic_cas( v, old, new ) \
  _InterlockedCompareExchange( (v), (new), (old) )
*/

#else

#error "Unsupported compiler"

#endif

#endif /* ndef _JQ_ATOMIC_H_ */
