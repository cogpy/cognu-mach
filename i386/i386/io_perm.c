/* io_perm.c - Code to manipulate I/O permission bitmap objects.
   Copyright (C) 2002 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of GNU Mach.

   GNU Mach is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Mach is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

/*
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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

#include <mach/boolean.h>
#include <mach/kern_return.h>

#include <ipc/ipc_port.h>

#include <kern/zalloc.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/thread.h>

#include <device/dev_hdr.h>
#include <device/device_port.h>

#include <oskit/ds_oskit.h>

#include "io_perm.h"
#include "gdt.h"


/* XXX From oskit/ds_routines.c  */
device_t dev_open_alloc (void);
void setup_no_senders (device_t dev);


/* The outtran which allows MiG to convert an io_perm_t object to a port
   representing it.  */
ipc_port_t
convert_io_perm_to_port (io_perm_t io_perm)
{
  return convert_device_to_port ((device_t) io_perm);
}


/* The intran which allows MiG to convert a port representing an
   io_perm_t object to the object itself.  */
io_perm_t
convert_port_to_io_perm (ipc_port_t port)
{
  return (io_perm_t) dev_port_lookup (port);
}


/* The destructor which is called when the last send right to a port
   representing an io_perm_t object vanishes.  */
void
io_perm_deallocate (io_perm_t io_perm)
{
  device_deallocate ((device_t) io_perm);
}


/* Initialize bitmap by setting all bits to OFF == 1.  */
static inline void
io_bitmap_init (unsigned char *iopb)
{
  memset (iopb, ~0, IOPB_BYTES);
}


/* Set selected bits in bitmap to ON == 0.  */
static inline void
io_bitmap_set (unsigned char *iopb, io_port_t from, io_port_t to)
{
  do
    iopb[from >> 3] &= ~(1 << (from & 0x7));
  while (from++ != to);
}


/* Set selected bits in bitmap to OFF == 1.  */
static inline void
io_bitmap_clear (unsigned char *iopb, io_port_t from, io_port_t to)
{
  do
    iopb[from >> 3] |= (1 << (from & 0x7));
  while (from++ != to);
}


/* Request a new port IO_PERM that represents the capability to access
   the I/O ports [FROM; TO] directly.  MASTER_PORT is the master device port.

   The function returns KERN_INVALID_ARGUMENT if TARGET_TASK is not a task,
   or FROM is greater than TO.

   The function is exported.  */
kern_return_t
i386_io_perm_create (ipc_port_t master_port, io_port_t from, io_port_t to,
		     io_perm_t *new)
{
  if (master_port != master_device_port || from > to)
    return KERN_INVALID_ARGUMENT;

  *new = (io_perm_t) dev_open_alloc ();
  if (! *new)
    return KERN_RESOURCE_SHORTAGE;

  /* Set up the dummy device.  */
  (*new)->com_device = 0;
  (*new)->mode = 0;
  (*new)->ops = &no_device_ops;
  setup_no_senders ((device_t) *new);

  (*new)->com.io_perm.from = from;
  (*new)->com.io_perm.to = to;

  return KERN_SUCCESS;
}


/* From pcb.c.  */
extern void update_ktss_iopb (unsigned char *new_iopb, int last);


/* Modify the I/O permissions for TARGET_TASK.  If ENABLE is TRUE, the
   permission to acces the I/O ports specified by IO_PERM is granted,
   otherwise it is withdrawn.

   The function returns KERN_INVALID_ARGUMENT if TARGET_TASK is not a valid
   task or IO_PERM not a valid I/O permission port.

   The function is exported.  */
kern_return_t
i386_io_perm_modify (task_t target_task, io_perm_t io_perm, boolean_t enable)
{
  io_port_t from, to;
  unsigned char *iopb;
  io_port_t iopb_size;

  if (target_task == TASK_NULL || (device_t) io_perm == DEVICE_NULL)
    return KERN_INVALID_ARGUMENT;

  from = io_perm->com.io_perm.from;
  to = io_perm->com.io_perm.to;

  simple_lock (&target_task->machine.iopb_lock);
  iopb = target_task->machine.iopb;
  iopb_size = target_task->machine.iopb_size;

  if (!enable && !iopb_size)
    {
      simple_unlock (&target_task->machine.iopb_lock);
      return KERN_SUCCESS;
    }

  if (!iopb)
    {
      simple_unlock (&target_task->machine.iopb_lock);
      iopb = (unsigned char *) zalloc (machine_task_iopb_zone);
      simple_lock (&target_task->machine.iopb_lock);
      if (target_task->machine.iopb)
	{
	  if (iopb)
	    zfree (machine_task_iopb_zone, (vm_offset_t) iopb);
	  iopb = target_task->machine.iopb;
	  iopb_size = target_task->machine.iopb_size;
	}
      else if (iopb)
	{
	  target_task->machine.iopb = iopb;
	  io_bitmap_init (iopb);
	}
      else
	return KERN_RESOURCE_SHORTAGE;
    }

  if (enable)
    {
      io_bitmap_set (iopb, from, to);
      if ((to >> 3) + 1 > iopb_size)
	target_task->machine.iopb_size = (to >> 3) + 1;
    }
  else
    {
      if ((from >> 3) + 1 > iopb_size)
	{
	  simple_unlock (&target_task->machine.iopb_lock);
	  return KERN_SUCCESS;
	}

      io_bitmap_clear (iopb, io_perm->com.io_perm.from,
		       io_perm->com.io_perm.to);
      while (iopb_size > 0 && iopb[iopb_size - 1] == 0xff)
	iopb_size--;
      target_task->machine.iopb_size = iopb_size;
    }

#if NCPUS>1
#warning SMP support missing (notify all CPUs running threads in that of the I/O bitmap change).
#endif
  if (target_task == current_task())
    update_ktss_iopb (iopb, target_task->machine.iopb_size);

  simple_unlock (&target_task->machine.iopb_lock);
  return KERN_SUCCESS;
}
