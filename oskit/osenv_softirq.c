/*
 * Copyright (c) 2000 University of Utah and the Flux Group.
 * All rights reserved.
 *
 * This file is part of the Flux OSKit.  The OSKit is free software, also known
 * as "open source;" you can redistribute it and/or modify it under the terms
 * of the GNU General Public License (GPL), version 2, as published by the Free
 * Software Foundation (FSF).  To explore alternate licensing terms, contact
 * the University of Utah at csl-dist@cs.utah.edu or +1-801-585-3271.
 *
 * The OSKit is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GPL for more details.  You should have
 * received a copy of the GPL along with the OSKit; see the file COPYING.  If
 * not, write to the FSF, 59 Temple Place #330, Boston, MA 02111-1307, USA.
 */

/*
 * oskit Software Interrupt management for Mach.
 */

#include <oskit/error.h>
#include <oskit/debug.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/softirq.h>
#include <oskit/machine/base_irq.h>
#include <stdio.h>
#include <string.h> /* ffs */

#include <machine/spl.h>
#include "ds_oskit.h"


/* Linked list of functions for a software interrupt.  */
struct softint_handler {
  void   (*func)(void *);
  void   *data;
  struct softint_handler *next;
  int    flags;
};

/* Array of pointers to lists of interrupt handlers.  */
static struct softint_handler *softint_handlers[SOFT_IRQ_COUNT];

/* Mask of allocated vectors, for the benefit of osenv_softirq_request()  */
static unsigned int softint_allocated = ((1 << OSENV_SOFT_IRQ_SOFTWARE)
					 | (1 << OSENV_SOFT_IRQ_PTHREAD));

/* Pending softirqs that have been software scheduled.  */
static volatile unsigned int softint_pending;

static spl_t osenv_softintr_spl;
extern spl_t curr_ipl;

/* This is called from softclock_oskit, at splsoftclock.
   It's the hook to run any pending oskit software interrupt handlers.

   A software interrupt handler should obviously not allocate or deallocate
   a software interrupt.  It may schedule one of course.  */
void
oskit_softint (void)
{
  unsigned int pending;

  /* This must be an atomic instruction because hardware interrupts (splio)
     are enabled, and one could come in and call osenv_softirq_schedule
     while this frame is suspended by the interrupt.  */

  asm ("xchgl %0,%1"
       : "=&r" (pending), "=m" (softint_pending)
       : "0" (0), "1" (softint_pending));

  while (pending)
    {
      int i;
      struct softint_handler *current;

      i = ffs (pending) - 1;
      pending &= ~(1 << i);

      current = softint_handlers[i];
      while (current)
	{
	  current->func (current->data);
	  current = current->next;
	}
    }
}


/* Allocate a (well-known) software interrupt handler.  */
oskit_error_t
osenv_softirq_alloc (int irq, void (*handler)(void *), void *data, int flags)
{
  struct softint_handler *temp, **p;

  if (irq < 0 || irq >= SOFT_IRQ_COUNT)
    return OSKIT_EINVAL;

  /* Do this to ensure that the free vectors are explicitly allocated
     before having a handler assigned.  */
  if ((irq >= SOFT_IRQ_FREEBASE) && !(softint_allocated & (1 << irq)))
    return OSKIT_EINVAL;

  /* This is a blocking operation, so to avoid races we need to do it
     before we start mucking with data structures.  */
  temp = osenv_mem_alloc (sizeof(struct softint_handler), 0, 1);
  if (temp == NULL)
    return OSKIT_ENOMEM;
  temp->func = handler;
  temp->data = data;
  temp->next = NULL;
  temp->flags = flags;

  /* Note that we only hook in the new handler after its structure has been
     fully initialized;  this way we don't have to disable interrupts, because
     interrupt-level code only scans the list.  */
  for (p = &softint_handlers[irq]; *p != NULL; p = &(*p)->next)
    ;
  *p = temp;

  return 0;
}

/*  Request an unused software interrupt handler vector. This just reserves
    and returns the vector number for use with osenv_softirq_alloc() above. */
oskit_error_t
osenv_softirq_alloc_vector (int *out_irq)
{
  int i, enabled;

  /* Go ahead and block interrupts, cause its easy.  */
  enabled = osenv_softintr_enabled ();
  if (enabled)
    osenv_softintr_disable ();

  /*  Search for an empty slot above the well-known vectors.  */
  for (i = SOFT_IRQ_FREEBASE; i < SOFT_IRQ_COUNT; i++)
    {
      if (! (softint_allocated & (1 << i)))
	break;
    }
  if (i == SOFT_IRQ_COUNT)
    {
      if (enabled)
	osenv_softintr_enable ();
      return OSKIT_ERANGE;
    }

  softint_allocated |= (1 << i);
  *out_irq = i;

  if (enabled)
    osenv_softintr_enable ();
  return 0;
}

/* Free up a vector.  */
oskit_error_t
osenv_softirq_free_vector (int irq)
{
  /* Must be allocated */
  if (! (softint_allocated & (1 << irq)))
    return OSKIT_EINVAL;

  /* Must no longer be in use */
  if (softint_handlers[irq])
    return OSKIT_EINVAL;

  softint_allocated &= ~(1 << irq);

  return 0;
}

/* Deallocate a software interrupt.  Need a handle so know WHICH
   interrupt handler to remove.  */
void
osenv_softirq_free (int irq, void (*handler)(void *), void *data)
{
  struct softint_handler *temp, **p;

  osenv_assert (irq >= 0 && irq < SOFT_IRQ_COUNT);

  /* Find and unlink the handler from the list.  */
  p = &softint_handlers[irq];
  while (((temp = *p) != NULL)
	 && (temp->func != handler || temp->data != data))
    p = &temp->next;

  /* not found?  */
  if (temp == NULL)
    {
      osenv_log (OSENV_LOG_WARNING,
		 "osenv_softirq_free: handler not registered!\n");
      return;
    }

  /* remove it!  */
  *p = temp->next;
  osenv_softintr_disable ();
  osenv_mem_free (temp, 0, sizeof(struct softint_handler));
}

/* Schedule a software interrupt to be delivered next time it's appropriate.
   Note that this must be called with software interrupts disabled!  */
void
osenv_softirq_schedule (int irq)
{
  osenv_assert (softint_handlers[irq]);

  softint_pending |= (1 << irq);
  setsoftclock ();
}

void
osenv_softintr_enable (void)
{
  spl_t s = osenv_softintr_spl;
  osenv_softintr_spl = SPL1;
  splx (s);
}

void
osenv_softintr_disable (void)
{
  /* We can be called with interrupts already disabled!  */
  if (curr_ipl > SPL1)
    /* We are already at a higher priority, so return to it.
       XXX This presumes _enable will be called at the same spl.
       Perhaps we should instead just set a flag and have
       the corresponding enable just do nothing.  */
    osenv_softintr_spl = curr_ipl;
  else if (curr_ipl < SPL1)
    /* We are at a level where software interrupts are enabled, so we must
       go to spltsoftclock.  osenv_softintr_enable we will return to the
       current level.  */
    osenv_softintr_spl = splsoftclock ();
}

int
osenv_softintr_enabled (void)
{
  return curr_ipl < SPL1;
}
