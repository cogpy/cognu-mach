#ifndef	_DS_REQUEST_H_
#define	_DS_REQUEST_H_

#include "ds_oskit.h"
#include <kern/zalloc.h>
#include <kern/queue.h>
#include <kern/lock.h>

/* This structure describes a pending asynchronous device request.

   That is, an i/o that is in progress but with no thread waiting for it.
   We store here all the information we need to complete the i/o and send
   the reply message to complete the device RPC.

   This is currently only used for oskit asyncio devices, but can be
   extended for other types e.g. when the oskit gives us a way not to
   block the device request thread doing disk i/o.

   These structures are queued on the device's read_queue or write_queue.
   Notifications from the device put the device itself onto the
   device_ready_queue and wake the io_done_thread.  The io_done_thread
   takes devices off the ready queue, polls them for i/o availability
   and then dequeues requests from their read_queue and/or write_queue
   as long as there is i/o available.

   All of these queues are accessed at splio and locked by
   device_ready_queue_lock.  */

struct pending_request {
  queue_chain_t chain;		/* on device's read_queue or write_queue */

  void (*completer) (device_t, struct pending_request *);

  struct ipc_port *reply_port;
  mach_msg_type_name_t reply_port_type;

  oskit_size_t count, offset;

  union device_data {
    vm_map_copy_t outofband;
    char *inband;		/* from io_inband_zone */
    vm_offset_t inbando;	/* rather than casting */
#define IO_SMALL_MAX	(sizeof (vm_map_copy_t))
    char small[IO_SMALL_MAX];
  } data;
};

extern zone_t io_req_zone;
#define request_allocate()	((void *) zalloc (io_req_zone))
#define request_free(req)	(zfree (io_req_zone, (vm_offset_t) (req)))


extern queue_head_t device_ready_queue;
decl_simple_lock_data(extern, device_ready_queue_lock)

extern void ds_request_init (void);
extern void ds_device_ready (device_t);
extern void ds_asyncio_ready (device_t);



#endif
