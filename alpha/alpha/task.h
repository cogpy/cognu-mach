/* task.h - Data types for machine specific parts of tasks on Alpha.
   Copyright (C) 2002 Free Software Foundation, Inc.

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

#ifndef	_ALPHA_TASK_H_
#define _ALPHA_TASK_H_

/* The machine specific data of a task.  */
struct machine_task
{
};
typedef struct machine_task machine_task_t;

#define machine_task_module_init() ((void) 0)
#define machine_task_init(task) ((void) (task))
#define machine_task_terminate(task) ((void) (task))
#define machine_task_collect(task) ((void) (task))

#endif	/* _ALPHA_TASK_H_ */
