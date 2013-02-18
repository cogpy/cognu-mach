/*
 * Mach Operating System
 * Copyright (c) 2012 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 *	File:	mach/vm_advice.h
 *	Author:	Maksym Planeta
 *
 *	Virtual memory map advice definitions.
 *
 */

#ifndef	_MACH_VM_ADVICE_H_
#define	_MACH_VM_ADVICE_H_

/*
 *	Types defined:
 *
 *	vm_advice_t	Enumeration of valid advice codes.
 *	
 */

typedef enum {
	VM_ADVICE_DEFAULT,
	VM_ADVICE_RANDOM,
	VM_ADVICE_SEQUENTIAL,
	VM_ADVICE_NORMAL,
} vm_advice_t;

#define VM_ADVICE_MAX_READAHEAD 2

struct vm_advice_entry {
	vm_offset_t	read_before;
	vm_offset_t	read_after;
};

#endif	/* _MACH_VM_ADVICE_H_ */
