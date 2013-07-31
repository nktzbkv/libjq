#include "jq.h"
#include "jq-test.h"

static void proc( void* p ) {
  
}

testing() {
  jq_worker_t worker = jq_worker_create( NULL, 4 );
  assert( worker != NULL );
  
  jq_group_t group = jq_group_create();
  
  int i;
  for( i = 0; i < 1000000; ++i ) {
    jq_worker_async_group( worker, group, proc, NULL );
  }
  
  jq_group_wait( group );
  
  jq_release( worker );
}
