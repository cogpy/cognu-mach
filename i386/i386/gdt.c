/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation
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
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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
/*
 * Global descriptor table.
 */
#include <mach/machine/vm_types.h>

#include "vm_param.h"
#include "gdt.h"

extern void etext();

void
gdt_init()
{
        /* Initialize kernel code segment descriptor, allowing
	   only kernel text to be executed in kernel mode.
	   This will have to change to allow loaded modules and such.
	   A limit of (LINEAR_MAX_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS)
	   allows any kernel virtual address to be executed, but not user-mode
	   addresses accessible from kernel mode.  */
	fill_gdt_descriptor(KERNEL_CS,
			    LINEAR_MIN_KERNEL_ADDRESS,
			    (oskit_addr_t)etext - 1,
			    ACC_PL_K|ACC_CODE_R, SZ_32);

	/* Initialize kernel data segment descriptor to allow any offset
	   so that any linear address is accessible, including wrapping
	   around to zero and covering the user-mode range.  */
	fill_gdt_descriptor(KERNEL_DS,
			    LINEAR_MIN_KERNEL_ADDRESS, 0xffffffff,
			    ACC_PL_K|ACC_DATA_W, SZ_32);
}
