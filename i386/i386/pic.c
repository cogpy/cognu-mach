/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
/*
Copyright (c) 1988,1989 Prime Computer, Inc.  Natick, MA 01760
All Rights Reserved.

Permission to use, copy, modify, and distribute this
software and its documentation for any purpose and
without fee is hereby granted, provided that the above
copyright notice appears in all copies and that both the
copyright notice and this permission notice appear in
supporting documentation, and that the name of Prime
Computer, Inc. not be used in advertising or publicity
pertaining to distribution of the software without
specific, written prior permission.

THIS SOFTWARE IS PROVIDED "AS IS", AND PRIME COMPUTER,
INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN
NO EVENT SHALL PRIME COMPUTER, INC.  BE LIABLE FOR ANY
SPECIAL, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN ACTION OF CONTRACT, NEGLIGENCE, OR
OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/types.h>
#include <i386/ipl.h>
#include <i386/machspl.h>

#include <oskit/x86/pc/pic.h>
#include <oskit/x86/proc_reg.h>

spl_t	curr_ipl;
int	pic_mask[NSPL];
int	curr_pic_mask;

int	iunit[NINTR] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

int 	nintr = NINTR;
int	npics = NPICS;

picinit()
{
  cli();

	form_pic_mask();

	/*
	** 1a. Select current SPL.
	*/

	curr_ipl = SPLHI;
	curr_pic_mask = pic_mask[SPLHI];

	pic_set_irqmask(curr_pic_mask);
}


/*
** form_pic_mask(int_lvl)
**
**	For a given interrupt priority level (int_lvl), this routine goes out
** and scans through the interrupt level table, and forms a mask based on the
** entries it finds there that have the same or lower interrupt priority level
** as (int_lvl). It returns a 16-bit mask which will have to be split up between
** the 2 pics.
**
*/

#if	defined(AT386) || defined(PS2)
#define SLAVEMASK       (0xFFFF ^ SLAVE_ON_IR2)
#endif	/* defined(AT386) || defined(PS2) */
#ifdef	iPSC386
#define SLAVEMASK       (0xFFFF ^ SLAVE_ON_IR7)
#endif	iPSC386

#define SLAVEACTV	0xFF00

form_pic_mask()
{
	unsigned int i, j, bit, mask;

	for (i=SPL0; i < NSPL; i++) {
	 	for (j=0x00, bit=0x01, mask = 0; j < NINTR; j++, bit<<=1)
			if (intpri[j] <= i)
				mask |= bit;

	 	if ((mask & SLAVEACTV) != SLAVEACTV )
			mask &= SLAVEMASK;

		pic_mask[i] = mask;
	}
}

intnull(unit_dev)
{
	printf("intnull(%d)\n", unit_dev);
}
