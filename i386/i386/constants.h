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

#ifndef _I386_CONSTANTS_H_
#define _I386_CONSTANTS_H_

/*
 * Common i386 architecture constants
 * Consolidates frequently used magic numbers throughout the codebase
 */

/* Bit masks for common data sizes */
#define BYTE_MASK               0xff        /* 8-bit mask */
#define WORD_MASK               0xffff      /* 16-bit mask */
#define LIMIT_20BIT_MASK        0xfffff     /* 20-bit limit mask for descriptors */

/* Segment register masks */
#define SEGMENT_SELECTOR_MASK   WORD_MASK   /* Segment selector is 16 bits */

/* Trap/exception error code masks */
#define ERROR_CODE_MASK         WORD_MASK   /* Error code lower 16 bits */

/* i386 instruction opcodes */
#define I386_BREAKPOINT_OPCODE  0xcc        /* INT 3 breakpoint instruction */

#endif /* _I386_CONSTANTS_H_ */