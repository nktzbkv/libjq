#ifndef _JQ_H_
#define _JQ_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
  Common.
-----------------------------------------------------------------------------*/

typedef void (*jq_handler_t)( void* );

void jq_retain( void* );
void jq_release( void* );

/*-----------------------------------------------------------------------------
  Group.
-----------------------------------------------------------------------------*/

typedef struct jq_group* jq_group_t;

jq_group_t jq_group_create();
void jq_group_enter( jq_group_t );
void jq_group_leave( jq_group_t );
void jq_group_wait( jq_group_t );

/*-----------------------------------------------------------------------------
  Queue.
-----------------------------------------------------------------------------*/

typedef struct jq_queue* jq_queue_t;

jq_queue_t jq_queue_create();
void jq_queue_empty( jq_queue_t queue );

void jq_queue_loop( jq_queue_t queue );
int jq_queue_poll( jq_queue_t queue );

int jq_queue_stop( jq_queue_t queue );

int jq_queue_submit(
  jq_queue_t queue,
  jq_group_t group,
  jq_handler_t handler,
  void* context );

size_t jq_queue_get_length( jq_queue_t queue );

/*-----------------------------------------------------------------------------
  Worker.
-----------------------------------------------------------------------------*/

typedef struct jq_worker* jq_worker_t;

jq_worker_t jq_worker_create( jq_queue_t queue, size_t threads );

void jq_worker_set_threads( jq_worker_t worker, size_t threads );

void jq_worker_async(
  jq_worker_t worker,
  jq_handler_t handler,
  void* context );

void jq_worker_async_group(
  jq_worker_t worker,
  jq_group_t group,
  jq_handler_t handler,
  void* context );

void jq_worker_sync(
  jq_worker_t worker,
  jq_handler_t handler,
  void* context );

/*-----------------------------------------------------------------------------
  Once.
-----------------------------------------------------------------------------*/

typedef int jq_once_t;
int jq_once( jq_once_t* );

#ifdef __cplusplus
}
#endif

#endif /* ndef _JQ_H_ */
