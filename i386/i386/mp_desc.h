/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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

#ifndef	_I386_MP_DESC_H_
#define	_I386_MP_DESC_H_

#include <cpus.h>

#if MULTIPROCESSOR

/*
 * Multiprocessor i386/i486 systems use a separate copy of the
 * GDT, IDT, LDT, and kernel TSS per processor.  The first three
 * are separate to avoid lock contention: the i386 uses locked
 * memory cycles to access the descriptor tables.  The TSS is
 * separate since each processor needs its own kernel stack,
 * and since using a TSS marks it busy.
 */

#include <mach/std_types.h>

#include <oskit/x86/base_idt.h>	/* IDTSZ */
#include <machine/tss.h>

#include "gdt.h"
#include "ldt.h"

/*
 * The descriptor tables are together in a structure
 * allocated one per processor (except for the boot processor).
 */
struct mp_desc_table {
  	int cpu_number;		/* to be fetched via %gs */
	struct x86_gate	idt[IDTSZ];	/* IDT */
	struct x86_desc	gdt[GDTSZ];	/* GDT */
	struct x86_desc	ldt[LDTSZ];	/* LDT */
	struct task_tss ktss;
};

/*
 * They are pointed to by a per-processor array.
 */
extern struct mp_desc_table	*mp_desc_table[NCPUS];

/*
 * The kernel TSS gets its own pointer.
 */
extern struct task_tss	*mp_ktss[NCPUS];

/*
 * So does the GDT.
 */
extern struct x86_desc	*mp_gdt[NCPUS];


/*
 * Each CPU calls this routine to set up its descriptor tables.
 */
extern struct mp_desc_table *	mp_desc_init (int cpu);

/* This one loads the tables into the running CPU.  */
void	mp_desc_load (struct mp_desc_table *);

#endif /* MULTIPROCESSOR */


/*
 * Addresses of bottom and top of interrupt stacks.
 */
extern vm_offset_t	interrupt_stack[NCPUS];
extern vm_offset_t	int_stack_top[NCPUS];

/*
 * Barrier address.
 */
extern vm_offset_t	int_stack_high;


#endif	/* _I386_MP_DESC_H_ */
