#include "jq.h"
#include "jq-test.h"
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define fail_exit( str ) (printf( str ), exit( -1 ))

volatile sig_atomic_t alarm_enabled = 0;

void catch_alarm( int sig ) {
  if( alarm_enabled )
    fail_exit( "Alarm!\n" );
  
  signal( sig, catch_alarm );
}

void check_enter_leave( jq_group_t group, int num ) {
  int i;
  
  for( i = 0; i < num; ++i )
    jq_group_enter( group );
  
  for( i = 0; i < num; ++i )
    jq_group_leave( group );
  
  alarm_enabled = 1;
  alarm( 1 );
  jq_group_wait( group );
  alarm_enabled = 0;
}

int main() {
  int i;
  
  jq_group_t group = jq_group_create();
  assert( group != NULL );
  
  signal( SIGALRM, catch_alarm );
  
  for( i = 0; i < 1000; ++i ) {
    check_enter_leave( group, i );
  }
  
  jq_release( group );
  
  return 0;
}
