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

#undef	USER_CS
#undef	USER_DS
#define	KERNEL_TSS	BASE_TSS
#define	KERNEL_LDT	0x58
#define	USER_LDT	0x60
#define	USER_TSS	0x68
#define	FPE_CS		0x70
#define	USER_FPREGS	0x78

#define gdt base_gdt

/* Fill a segment descriptor in the GDT.  */
#define fill_gdt_descriptor(segment, base, limit, access, sizebits) \
	fill_descriptor(&gdt[segment/8], base, limit, access, sizebits)

#endif _I386_GDT_
