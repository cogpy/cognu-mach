/* io_perm.h - Data types for I/O permission bitmap objects.
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

#ifndef _I386_IO_PERM_H_
#define _I386_IO_PERM_H_

/* The highest possible I/O port.  ISA bus allows ports 0..3ff, but
   accelerator cards are funky.  */
#define	IOPB_MAX	0xFFFF	

/* The number of bytes needed to hold all permission bits.  */
#define	IOPB_BYTES	(((IOPB_MAX + 1) + 7) / 8)

/* An offset that points outside of the permission bitmap, used to
   disable all permission.  */
#define IOPB_INVAL	0x2FFF


/* The type of an I/O port address.  */
typedef unsigned short io_port_t;


/* struct device is defined in <oskit/ds_oskit.h>, which includes this
   file for the io_port_t type definition above.  */
typedef struct device *io_perm_t;

extern io_perm_t convert_port_to_io_perm (/* struct ipc_port * */);
extern struct ipc_port *convert_io_perm_to_port(/* io_perm_t */);
extern void io_perm_deallocate(/* io_perm_t */);

#endif
