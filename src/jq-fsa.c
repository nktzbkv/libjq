#include "jq-private.h"
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------------------------
  Internals.
-----------------------------------------------------------------------------*/

typedef unsigned char byte_t;
#define next(p) (*(void**)(p))

static void jq_fsa_alloc_chunk( jq_fsa* fsa ) {
  byte_t* p;
  byte_t* last;
  byte_t *chunk = (byte_t*)malloc( sizeof(void*) + fsa->blocks_per_chunk * fsa->size );
  
  next( chunk ) = fsa->first_chunk;
  fsa->first_chunk = chunk;
  
  p = chunk + sizeof(void*);
  last = p + fsa->size * (fsa->blocks_per_chunk - 1);
  
  for( ; p < last; p += fsa->size )
    next( p ) = p + fsa->size;
  
  next( last ) = NULL;
  
  fsa->first_free = chunk + sizeof(void*);
}

static void jq_fsa_dealloc_chunks( jq_fsa* fsa ) {
  void* chunk = fsa->first_chunk;
  
  while( chunk ) {
    void* next_chunk = next( chunk );
    free( chunk );
    chunk = next_chunk;
  }
}

/*-----------------------------------------------------------------------------
  Public.
-----------------------------------------------------------------------------*/

void jq_fsa_init( jq_fsa* fsa, size_t size, size_t blocks_per_chunk ) {
  memset( fsa, 0, sizeof(jq_fsa) );
  
  fsa->size = size < sizeof(void*) ? sizeof(void*) : size;
  fsa->blocks_per_chunk = blocks_per_chunk < 8 ? 8 : blocks_per_chunk;
  
  pthread_spin_init( &fsa->lock, 0 );
}

void jq_fsa_destroy( jq_fsa* fsa ) {
  jq_fsa_dealloc_chunks( fsa );
  pthread_spin_destroy( &fsa->lock );
}

void* jq_fsa_alloc( jq_fsa* fsa ) {
  void* ptr;
  
  pthread_spin_lock( &fsa->lock );
  
  if( !(ptr = fsa->first_free) ) {
    jq_fsa_alloc_chunk( fsa );
    
    if( !(ptr = fsa->first_free) ) {
      return NULL;
    }
  }
  
  fsa->first_free = next( fsa->first_free );
  
  pthread_spin_unlock( &fsa->lock );
  
  return ptr;
}

void jq_fsa_free( jq_fsa* fsa, void* ptr ) {
  if( !ptr ) return;
  
  pthread_spin_lock( &fsa->lock );
  
  next( ptr ) = fsa->first_free;
  fsa->first_free = ptr;
  
  pthread_spin_unlock( &fsa->lock );
}

void jq_fsa_free_all( jq_fsa* fsa ) {
  pthread_spin_lock( &fsa->lock );
  
  jq_fsa_dealloc_chunks( fsa );
  fsa->first_free = NULL;
  fsa->first_chunk = NULL;
  
  pthread_spin_unlock( &fsa->lock );
}
