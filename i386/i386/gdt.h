/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.
 *
 * CARNEGIE MELLON, IBM, AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON, IBM, AND CSL DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#ifndef _I386_GDT_
#define _I386_GDT_

#include <oskit/x86/base_gdt.h>

/* The layout of the GDT is as follows.  Each four entries fill one cache line.
   We need to match the oskit layout if we call into any oskit code that uses
   the literal selectors.  I don't think we do, except for base_gdt_load
   itself.  So we make sure the oskit's KERNEL_CS and KERNEL_DS selectors work
   ok, but reuse the other base_gdt slots differently.  It's presumed a Good
   Thing to have all the commonly-used kernel-mode descriptors on one cache
   line and all the user-mode descriptors on another single cache line.

   idx	sel	what				oskit base_gdt usage

   0	0x00	reserved (null selector)
   1	0x08	initial TSS			oskit BASE_TSS
   2	0x10	initial kernel code		oskit KERNEL_CS
   3	0x18	initial kernel data		oskit KERNEL_DS

   4	0x20	KERNEL_CS			oskit KERNEL_16_CS
   5	0x28	KERNEL_DS			oskit KERNEL_16_CS
   6	0x30	KERNEL_TSS			oskit LINEAR_CS
   7	0x38	KERNEL_LDT			oskit LINEAR_DS

   8	0x43	user-mode code			oskit USER_CS
   9	0x4b	user-mode code			oskit USER_DS
   10	0x50	i386_set_gdt #1
   11	0x58	i386_set_gdt #2

   12	0x60	USER_LDT
   13	0x68
   14	0x70	FPE_CS
   15	0x78	USER_FPREGS

   For more, can no longer use oskit base_gdt defn since GDTSZ is 16 entries.
*/

#define	BASE_KERNEL_CS	0x10	/* <oskit/x86/base_gdt.h> value of KERNEL_CS */
#define	BASE_KERNEL_DS	0x18	/* <oskit/x86/base_gdt.h> value of KERNEL_DS */
#undef	KERNEL_CS
#undef	KERNEL_DS

#define KERNEL_CS	0x20
#define KERNEL_DS	0x28
#define KERNEL_TSS	0x30
#define KERNEL_LDT	0x38

#undef	USER_CS
#undef	USER_DS
#define	USER_LDT	0x60
#define	FPE_CS		0x70
#define	USER_FPREGS	0x78

#define USER_GDT	0x50
#define USER_GDT_SLOTS	2

#define gdt base_gdt

/* Fill a segment descriptor in the GDT.  */
#define fill_gdt_descriptor(segment, base, limit, access, sizebits) \
	fill_descriptor(&gdt[segment/8], base, limit, access, sizebits)

#endif /* _I386_GDT_ */
