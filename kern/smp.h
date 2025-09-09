/* smp.h - Enhanced SMP controller for Mach with threading support
   Copyright (C) 2020 Free Software Foundation, Inc.
   Written by Almudena Garcia Jurado-Centurion
   Enhanced for SMP threading by GNU Copilot

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

#ifndef _KERN_SMP_H_
#define _KERN_SMP_H_

#include <stdint.h>
#include <kern/queue.h>
#include <kern/lock.h>
#include <mach/kern_return.h>		/* for kern_return_t */

/* Forward declaration */
struct thread;

/* Work queue item structure */
struct smp_work_item {
    queue_chain_t  chain;
    void (*func)(void *arg);
    void *arg;
    int flags;
};

/* Work queue structure */
struct smp_work_queue {
    queue_head_t work_items;
    decl_simple_lock_data(, lock)
    struct thread *worker_thread;
    int cpu_id;
    boolean_t active;
};

/* CPU affinity mask */
typedef uint32_t cpu_mask_t;

/* SMP functions */
void smp_set_numcpus(uint8_t numcpus);
uint8_t smp_get_numcpus(void);

/* Work queue management */
void smp_work_queue_init(void);
kern_return_t smp_queue_work(int cpu, void (*func)(void *), void *arg);
kern_return_t smp_queue_work_on(int cpu, void (*func)(void *), void *arg);
void smp_work_thread(void);

/* CPU affinity support */
#define CPU_AFFINITY_ANY    ((cpu_mask_t)-1)
#define CPU_AFFINITY_NONE   ((cpu_mask_t)0)

static inline boolean_t cpu_affinity_test(cpu_mask_t mask, int cpu) {
    return (mask & (1U << cpu)) != 0;
}

static inline cpu_mask_t cpu_affinity_set(cpu_mask_t mask, int cpu) {
    return mask | (1U << cpu);
}

static inline cpu_mask_t cpu_affinity_clear(cpu_mask_t mask, int cpu) {
    return mask & ~(1U << cpu);
}

#endif /* _KERN_SMP_H_ */
