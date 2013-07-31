#include "jq.h"
#include "jq-test.h"

int counter = 0;
static void inc( void* c ) { counter++; }

testing() {
  jq_queue_t queue = jq_queue_create();
  assert( queue != NULL );
  
  ok( jq_queue_get_length( queue ) == 0 );
  
  jq_queue_submit( queue, NULL, inc, NULL );
  ok( jq_queue_get_length( queue ) == 1 );
  
  jq_queue_submit( queue, NULL, inc, NULL );
  ok( jq_queue_get_length( queue ) == 2 );
  
  jq_queue_poll( queue );
  ok( jq_queue_get_length( queue ) == 0 );
  ok( counter == 2 );
  
  jq_queue_stop( queue );
  jq_queue_submit( queue, NULL, inc, NULL );
  jq_queue_submit( queue, NULL, inc, NULL );
  
  ok( jq_queue_get_length( queue ) == 3 );
  
  jq_queue_loop( queue );
  ok( jq_queue_get_length( queue ) == 2 );
  ok( counter == 2 );
  
  jq_queue_poll( queue );
  ok( jq_queue_get_length( queue ) == 0 );
  ok( counter == 4 );
  
  jq_release( queue );
}
