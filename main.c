#include "jq.h"
#include <stdio.h>

void test( void* p ) {
  printf( "Hi! %lu\n", (size_t)p );
  // sleep( 1 );
}

int main() {
  jq_worker_t worker = jq_worker_create( NULL, 100 );
  jq_group_t group = jq_group_create();
  
  size_t i;
  for( i = 0; i < 500000; ++i )
    jq_worker_async_group( worker, group, test, (void*)i );
  
  jq_group_wait( group );
  
  jq_release( group );
  jq_release( worker );
  
  sleep( 1 );
  return 0;
}
