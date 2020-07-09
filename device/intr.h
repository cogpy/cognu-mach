/*
 * Copyright (c) 2010, 2011, 2019 Free Software Foundation, Inc.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE FREE SOFTWARE FOUNDATIONALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE FREE SOFTWARE FOUNDATION DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#ifndef __INTR_H__
#define __INTR_H__

#include <device/device_types.h>
#include <kern/queue.h>
#include <device/notify.h>

typedef struct intr_entry
{
  queue_chain_t chain;
  ipc_port_t dest;
  int line;
  int interrupts;		/* The number of interrupts occur since last run of intr_thread. */
  int unacked_interrupts;	/* Number of times irqs were disabled for this */
} user_intr_t;

#define DEVICE_NOTIFY_MSGH_SEQNO 0

int install_user_intr_handler (unsigned int line,
					unsigned long flags,
					user_intr_t *user_intr);

/* Returns 0 if action should be removed */
int deliver_user_intr (int line, user_intr_t *intr);

user_intr_t *insert_intr_entry (int line, ipc_port_t dest);

/* TODO: should rather take delivery port */
kern_return_t user_intr_enable (int line, char status);

void intr_thread (void);

void __disable_irq(unsigned int);
void __enable_irq(unsigned int);

#endif
