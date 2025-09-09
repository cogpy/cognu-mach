/* smp.c - Enhanced SMP controller for Mach with threading support
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

#include <kern/smp.h>
#include <machine/smp.h>
#include <stdint.h>
#include <kern/kalloc.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>

#ifndef NCPUS
#define NCPUS 1
#endif

struct smp_data {
    uint8_t num_cpus;
} smp_info;

/* Per-CPU work queues */
static struct smp_work_queue *cpu_work_queues = NULL;

/*
 * smp_set_numcpus: initialize the number of cpus in smp_info structure
 */
void smp_set_numcpus(uint8_t numcpus)
{
   smp_info.num_cpus = numcpus;
}

/*
 * smp_get_numcpus: returns the number of cpus existing in the machine
 */
uint8_t smp_get_numcpus(void)
{
   uint8_t numcpus = smp_info.num_cpus;

   if (numcpus == 0)
      return 1; /* Although SMP doesn't find cpus, always there are at least one. */
   else
      return numcpus;
}

/*
 * Initialize work queues for SMP
 */
void smp_work_queue_init(void)
{
    int cpu;
    int num_cpus = smp_get_numcpus();
    
    if (cpu_work_queues != NULL)
        return; /* Already initialized */
        
    cpu_work_queues = (struct smp_work_queue *)
        kalloc((size_t)num_cpus * sizeof(struct smp_work_queue));
        
    if (cpu_work_queues == NULL)
        return;
        
    for (cpu = 0; cpu < num_cpus; cpu++) {
        queue_init(&cpu_work_queues[cpu].work_items);
        simple_lock_init(&cpu_work_queues[cpu].lock);
        cpu_work_queues[cpu].worker_thread = NULL;
        cpu_work_queues[cpu].cpu_id = cpu;
        cpu_work_queues[cpu].active = TRUE;
    }
}

/*
 * Queue work on any available CPU
 */
kern_return_t smp_queue_work(int cpu, void (*func)(void *), void *arg)
{
    struct smp_work_item *work_item;
    struct smp_work_queue *wq;
    int target_cpu = cpu;
    
    if (cpu_work_queues == NULL)
        return KERN_FAILURE;
        
    /* If CPU is -1, use current CPU or CPU 0 */
    if (target_cpu < 0 || target_cpu >= smp_get_numcpus()) {
#ifdef NCPUS
        target_cpu = 0; /* Fallback to CPU 0 */
#else
        target_cpu = cpu_number();
#endif
    }
    
    wq = &cpu_work_queues[target_cpu];
    work_item = (struct smp_work_item *)kalloc(sizeof(struct smp_work_item));
    
    if (work_item == NULL)
        return KERN_RESOURCE_SHORTAGE;
        
    work_item->func = func;
    work_item->arg = arg;
    work_item->flags = 0;
    
    simple_lock(&wq->lock);
    queue_enter(&wq->work_items, work_item, struct smp_work_item *, chain);
    simple_unlock(&wq->lock);
    
    /* Wake up worker thread if it exists */
    if (wq->worker_thread != NULL) {
        thread_wakeup((event_t)wq);
    }
    
    return KERN_SUCCESS;
}

/*
 * Queue work on a specific CPU
 */
kern_return_t smp_queue_work_on(int cpu, void (*func)(void *), void *arg)
{
    if (cpu < 0 || cpu >= smp_get_numcpus())
        return KERN_INVALID_ARGUMENT;
        
    return smp_queue_work(cpu, func, arg);
}

/*
 * Work thread main loop - processes work items for a CPU
 */
void smp_work_thread(void)
{
    struct smp_work_queue *wq;
    struct smp_work_item *work_item;
    int cpu_id = 0; /* Default to CPU 0 for now */
    
#ifdef NCPUS
    if (NCPUS > 1) {
        /* In a real SMP system, get actual CPU ID */
        /* cpu_id = cpu_number(); */
    }
#endif
    
    if (cpu_work_queues == NULL || cpu_id >= smp_get_numcpus())
        return;
        
    wq = &cpu_work_queues[cpu_id];
    wq->worker_thread = current_thread();
    
    while (wq->active) {
        simple_lock(&wq->lock);
        
        if (queue_empty(&wq->work_items)) {
            simple_unlock(&wq->lock);
            /* Wait for work */
            assert_wait((event_t)wq, FALSE);
            thread_block((continuation_t)0);
            continue;
        }
        
        work_item = (struct smp_work_item *)queue_first(&wq->work_items);
        queue_remove(&wq->work_items, work_item, struct smp_work_item *, chain);
        simple_unlock(&wq->lock);
        
        /* Execute work item */
        if (work_item->func != NULL) {
            work_item->func(work_item->arg);
        }
        
        /* Free work item */
        kfree((vm_offset_t)work_item, sizeof(struct smp_work_item));
    }
}
