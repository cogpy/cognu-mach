/*
 * Mach Operating System
 * Copyright (c) 1993,1991,1990,1989,1988 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include "ds_request.h"

#include <machine/spl.h>
#include <kern/counters.h>
#include <kern/queue.h>
#include <kern/zalloc.h>


queue_head_t		device_ready_queue;
decl_simple_lock_data(,	device_ready_queue_lock)

zone_t io_req_zone;


void io_done_thread_continue()
{
	for (;;) {
	    register spl_t	s;
	    device_t dev;

	    while (1)
	      {
		s = splio();
		simple_lock(&device_ready_queue_lock);

		if (queue_empty (&device_ready_queue))
		  break;
		queue_remove_first (&device_ready_queue, dev,
				    device_t, com.stream.ready_queue);

		/* Clear this link to indicate that the device is no
		   longer on the queue, so ds_device_ready (below)
		   can check and avoid trying to requeue a device
		   that is already waiting for us on the queue.  */
		dev->com.stream.ready_queue.next = 0;

		simple_unlock(&device_ready_queue_lock);
		(void) splx(s);

		/* We have a ready device.  */
		ds_asyncio_ready (dev);/* could be dev->ops->ready */
		device_deallocate (dev); /* place on ready queue held a ref */
	    }

	    assert_wait(&device_ready_queue, FALSE);
	    simple_unlock(&device_ready_queue_lock);
	    (void) splx(s);
	    counter(c_io_done_thread_block++);
	    thread_block(io_done_thread_continue);
	}
}

void io_done_thread()
{
	/*
	 * Set thread privileges and highest priority.
	 */
	current_thread()->vm_privilege = TRUE;
	stack_privilege(current_thread());
	thread_set_own_priority(0);

	io_done_thread_continue();
	/*NOTREACHED*/
}


void
ds_device_ready (device_t dev)
{
  spl_t s;
  int ref;

  s = splio ();
  simple_lock (&device_ready_queue_lock); /* locks all request queues! */

  ref = (dev->com.stream.ready_queue.next == 0);
  if (ref)			/* if not already on the queue */
    queue_enter (&device_ready_queue, dev, device_t, com.stream.ready_queue);

  simple_unlock (&device_ready_queue_lock);
  splx (s);

  /* The device's spot on the ready queue holds a reference that
     will be freed when it's dequeued.  */
  device_reference (dev);

  thread_wakeup_one (&device_ready_queue);
}

void
ds_request_init (void)
{
  io_req_zone = zinit (sizeof (struct pending_request),
		       1000 * sizeof (struct pending_request),
		       100 * sizeof (struct pending_request),
		       ZONE_EXHAUSTIBLE,
		       "io requests pending");

  queue_init (&device_ready_queue);
  simple_lock_init (&device_ready_queue_lock);
}
