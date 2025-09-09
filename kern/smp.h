/* smp.h - Template for generic SMP controller for Mach. Header file
   Copyright (C) 2020 Free Software Foundation, Inc.
   Written by Almudena Garcia Jurado-Centurion

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

/*
 * smp.h - Template for generic SMP controller for Mach. Header file
 * Copyright (C) 2020 Free Software Foundation, Inc.
 * Written by Almudena Garcia Jurado-Centurion
 * Enhanced for improved SMP support by GNU Mach contributors
 *
 * This file is part of GNU Mach.
 *
 * GNU Mach is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * GNU Mach is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#ifndef _KERN_SMP_H_
#define _KERN_SMP_H_

#include <stdint.h>
#include <mach/boolean.h>
#include <kern/lock.h>

/* Forward declarations */
struct thread;
struct processor;
struct processor_set;
typedef struct thread *thread_t;
typedef struct processor *processor_t;
typedef struct processor_set *processor_set_t;

/* SMP synchronization and data structures */
struct smp_cpu_info {
    uint8_t cpu_id;
    boolean_t is_online;
    boolean_t is_active;
    uint32_t load_average;
    uint64_t idle_time;
    uint64_t busy_time;
};

/* Core SMP functions */
void smp_set_numcpus(uint8_t numcpus);
uint8_t smp_get_numcpus(void);

/* SMP-aware scheduling support */
boolean_t smp_cpu_is_online(uint8_t cpu);
void smp_cpu_set_online(uint8_t cpu, boolean_t online);
uint8_t smp_find_idle_cpu(void);
uint8_t smp_select_cpu_for_thread(void);

/* SMP load balancing */
void smp_balance_load(void);
uint32_t smp_get_cpu_load(uint8_t cpu);
void smp_update_cpu_load(uint8_t cpu);

/* SMP synchronization barriers */
void smp_synchronize_cpus(void);
void smp_cpu_barrier(void);

/* Enhanced SMP scheduler functions */
void smp_sched_init(void);
processor_t smp_choose_processor(thread_t thread, processor_set_t pset);
boolean_t smp_should_migrate_thread(thread_t thread, processor_t from_proc, processor_t to_proc);
void smp_balance_pset_load(processor_set_t pset);
boolean_t smp_steal_work(processor_t idle_proc);
void smp_enhanced_thread_setrun(thread_t th, boolean_t may_preempt);
int processor_to_cpu_id(processor_t processor);

/* CPU state management */
extern struct smp_cpu_info smp_cpu_info[NCPUS];
extern simple_lock_data_t smp_cpu_info_lock;

#endif /* _KERN_SMP_H_ */
