/*
 * Copyright (c) 1996, 1998, 1999, 2000 University of Utah and the Flux Group.
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
 * oskit IRQ control object for Mach.
 */

#include <oskit/error.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/osenv_irq.h>
#include <oskit/machine/base_trap.h>
#include <oskit/machine/pio.h>
#include <oskit/machine/proc_reg.h>
#include <oskit/machine/base_irq.h>

#include <string.h>

#include <machine/spl.h>
extern int intnull();

#include "ds_oskit.h"

#include "kalloc.h"
#include "assert.h"


/* linked list of functions for an IRQ */
struct int_handler {
	void	(*func)(void *);
	void	*data;
	struct int_handler *next;
};

/* array of pointers to lists of interrupt handlers */
static struct int_handler *handlers[BASE_IRQ_COUNT];

/* flag = 1 if sharing allowed; set to 0 if not */
static char shareable[BASE_IRQ_COUNT];

/* An oskit interrupt handler call osenv_intr_enable to "reenable
   interrupts" during the handler (it must return with them disabled
   again).  When it does this, what it really means is "reenable OTHER
   interrupts"--it does it before actually handling the interrupt!
   We must leave the irq line whose handler is running disabled at the PIC
   until that handler returns, only enabling other interrupts.  */

unsigned int nested_pic_mask;

static int
irq_handler(iunit, old_ipl, ret_addr, regs)
        int     iunit;          /* 'unit' number */
	int	old_ipl;	/* old interrupt level */
	void *ret_addr __attribute__((unused)); /* in interrupt handler */
	void *regs __attribute__((unused)); /* saved registers */
{
	const unsigned int irq = iunit;
	struct int_handler *current = handlers[irq];
	unsigned int omask = nested_pic_mask & (1 << irq);

	nested_pic_mask |= 1 << irq;

	while (current) {
		osenv_assert(current->func);
		current->func(current->data);
		current = current->next;
	}

	nested_pic_mask &= ~(1 << irq);
	nested_pic_mask |= omask;

	return 0;
}

int intpri_oskit = /* SPL0; */ SPLIO; /* XXX */

/* The one and only.  */
static struct oskit_osenv_irq_ops	osenv_irq_ops;
static struct oskit_osenv_irq		osenv_irq_object = {&osenv_irq_ops};

static OSKIT_COMDECL
irq_query(oskit_osenv_irq_t *s, const oskit_iid_t *iid, void **out_ihandle)
{
        if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
            memcmp(iid, &oskit_osenv_irq_iid, sizeof(*iid)) == 0) {
                *out_ihandle = s;
                return 0;
        }

        *out_ihandle = 0;
        return OSKIT_E_NOINTERFACE;
};

static OSKIT_COMDECL_U
irq_addref(oskit_osenv_irq_t *s)
{
	/* Only one object */
	return 1;
}

static OSKIT_COMDECL_U
irq_release(oskit_osenv_irq_t *s)
{
	/* Only one object */
	return 1;
}

static OSKIT_COMDECL_U
irq_alloc(oskit_osenv_irq_t *o, int irq,
	  void (*handler)(void *), void *data, int flags)
{
	struct int_handler *temp, **p;
	int first_time;

	if (irq < 0 || irq >= BASE_IRQ_COUNT)
		return OSKIT_EINVAL;

	first_time = (handlers[irq] == NULL);

	if (first_time && ivect[irq] != intnull)
		return OSKIT_EBUSY;

	assert(intpri_oskit != SPL0);

	if (!first_time && intpri[irq] != intpri_oskit) {
		osenv_log(OSENV_LOG_ERR, "oskit wants irq %d at pri %d vs %d",
			  irq, intpri_oskit, intpri[irq]);
		return OSKIT_EBUSY;
	}

	/*
	 * This is a blocking operation,
	 * so to avoid races we need to do it
	 * before we start mucking with data structures.
	 */
	temp = (void *) kalloc(sizeof(struct int_handler));
	if (temp == NULL)
		return OSKIT_ENOMEM;
	temp->func = handler;
	temp->data = data;
	temp->next = NULL;

	/*
	 * Fail if the irq is already in use
	 * and either the new handler or the existing handler
	 * is not shareable.
	 */
	if (!first_time &&
	    (!shareable[irq] || !(flags & OSENV_IRQ_SHAREABLE))) {
		kfree(temp, sizeof(struct int_handler));
	    	return OSKIT_EBUSY;
	}

	/*
	 * Note that we only hook in the new handler
	 * after its structure has been fully initialized;
	 * this way we don't have to disable interrupts,
	 * because interrupt-level code only scans the list.
	 */
	for (p = &handlers[irq]; *p != NULL; p = &(*p)->next);
	*p = temp;

	if (first_time)  {
		int x = splhigh ();
		shareable[irq] = (flags & OSENV_IRQ_SHAREABLE) != 0;
		intpri[irq] = intpri_oskit;
		ivect[irq] = irq_handler;
		iunit[irq] = irq;
		form_pic_mask();
		splx(x);
	}

	return 0;
}


static OSKIT_COMDECL_V
irq_free(oskit_osenv_irq_t *o, int irq,
	 void (*handler)(void *), void *data)
{
	struct int_handler *temp, **p;

	osenv_assert(irq >= 0 && irq < BASE_IRQ_COUNT);
	assert(ivect[irq] == irq_handler);

	/*
	 * If this is the only handler for this IRQ,
	 * then disable the IRQ before unregistering it,
	 * to avoid a possible infinite interrupt loop
	 * if the interrupt comes at just the wrong time.
	 * Not that this is ever likely to happen,
	 * but you never know with PC hardware...
	 */
	temp = handlers[irq];
	if (temp != NULL &&
	    temp->func == handler && temp->data == data &&
	    temp->next == NULL) {
		osenv_irq_disable(irq);
		handlers[irq] = NULL;
		intpri[irq] = SPL0;
		ivect[irq] = intnull;
		iunit[irq] = irq;
		kfree(temp, sizeof(struct int_handler));
		return;
	}

	/*
	 * Find and unlink the handler from the list.
	 * Interrupt handlers may safely scan the list during this time,
	 * but we know no one else will concurrently modify it
	 * because only process-level code modifies the list.
	 */
	p = &handlers[irq];
	while (((temp = *p) != NULL) &&
	       (temp->func != handler || temp->data != data))
		p = &temp->next;

	/* not found? */
	if (temp == NULL) {
		osenv_log(OSENV_LOG_WARNING,
			"osenv_irq_free: interrupt handler not registered!\n");
		return;
	}

	/* remove it! */
	*p = temp->next;

	kfree(temp, sizeof(struct int_handler));
}

static OSKIT_COMDECL_V
irq_disable(oskit_osenv_irq_t *o, int irq)
{
  spl_t s = splhigh ();		/* XXX make sure it's a change */
  nested_pic_mask |= (1 << irq);
  splx (s);
}

static OSKIT_COMDECL_V
irq_enable(oskit_osenv_irq_t *o, int irq)
{
  spl_t s = splhigh ();
  nested_pic_mask &= ~(1 << irq);
  splx (s);
}

static OSKIT_COMDECL_U
irq_pending(oskit_osenv_irq_t *o, int irq)
{
	return osenv_irq_pending(irq);
}

static struct oskit_osenv_irq_ops osenv_irq_ops = {
	irq_query,
	irq_addref,
	irq_release,
	irq_alloc,
	irq_free,
	irq_disable,
	irq_enable,
	irq_pending,
};

/*
 * Return a reference to the one and only irq object.
 */
oskit_osenv_irq_t *
oskit_create_osenv_irq(void)
{
	return &osenv_irq_object;
}
