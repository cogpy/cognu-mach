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

#include <cpus.h>
#include <kern/cpu_number.h>
#include <mach/machine.h>
#include <vm/vm_kern.h>

#include <i386/mp_desc.h>
#include <oskit/x86/base_stack.h>

/*
 * Addresses of bottom and top of interrupt stacks.
 * When NCPUS==1 these initialized values never change.
 */
vm_offset_t	interrupt_stack[NCPUS] = { (vm_offset_t) &base_stack_start, };
vm_offset_t	int_stack_top[NCPUS] = { (vm_offset_t) &base_stack_end, };

/*
 * Barrier address.
 */
vm_offset_t	int_stack_high;

#if	NCPUS > 1

#include <i386/lock.h>
#include "vm_param.h"

#include <oskit/x86/base_idt.h>
#include "gdt.h"
#include <oskit/x86/base_tss.h>


/*
 * The i386 needs an interrupt stack to keep the PCB stack from being
 * overrun by interrupts.  All interrupt stacks MUST lie at lower addresses
 * than any thread`s kernel stack.
 */

/*
 * We allocate interrupt stacks from physical memory.
 */
extern
vm_offset_t	avail_start;

/*
 * Multiprocessor i386/i486 systems use a separate copy of the
 * GDT, IDT, LDT, and kernel TSS per processor.  The first three
 * are separate to avoid lock contention: the i386 uses locked
 * memory cycles to access the descriptor tables.  The TSS is
 * separate since each processor needs its own kernel stack,
 * and since using a TSS marks it busy.
 */

/*
 * Allocated descriptor tables.
 */
struct mp_desc_table	*mp_desc_table[NCPUS];

/*
 * Pointer to TSS for access in load_context.
 */
struct x86_tss		*mp_ktss[NCPUS];

/*
 * Pointer to GDT to reset the KTSS busy bit.
 */
struct x86_desc	*mp_gdt[NCPUS];

/*
 * Allocate and initialize the per-processor descriptor tables.
 */

struct mp_desc_table *
mp_desc_init(mycpu)
	register int	mycpu;
{
	register struct mp_desc_table *mpt;

	if (mycpu == master_cpu) {
		/*
		 * Master CPU uses the tables built at boot time.
		 * Just set the TSS and GDT pointers.
		 */
		mp_ktss[mycpu] = &base_tss;
		mp_gdt[mycpu] = gdt;
		return 0;
	}
	else {
		/*
		 * Other CPUs allocate the table from the bottom of
		 * the interrupt stack.
		 */
		mpt = (struct mp_desc_table *) interrupt_stack[mycpu];

		mp_desc_table[mycpu] = mpt;
		mp_ktss[mycpu] = &mpt->ktss;
		mp_gdt[mycpu] = mpt->gdt;

		/*
		 * Copy the tables
		 */
		memcpy (mpt->idt, base_idt, sizeof mpt->idt);
		memcpy (mpt->gdt, base_gdt, sizeof mpt->gdt);
		memcpy (mpt->ldt, ldt, sizeof ldt);
		bzero (&mpt->ktss, sizeof mpt->ktss);

		/*
		 * Fix up the entries in the GDT to point to
		 * this LDT and this TSS.
		 */
		fill_descriptor(&mpt->gdt[sel_idx(KERNEL_LDT)],
			(unsigned)kvtolin(&mpt->ldt),
			LDTSZ * sizeof(struct x86_desc) - 1,
			ACC_P|ACC_PL_K|ACC_LDT, 0);
		fill_descriptor(&mpt->gdt[sel_idx(KERNEL_TSS)],
			(unsigned)kvtolin(&mpt->ktss),
			sizeof(struct x86_tss) - 1,
			ACC_P|ACC_PL_K|ACC_TSS, 0);

		/*
		 * Set the %gs segment register to point at
		 * a word containing the cpu number.
		 */
		mpt->cpu_number = mycpu;
		fill_descriptor(&mpt->gdt[sel_idx(KERNEL_GS)],
			(unsigned)kvtolin(&mpt->cpu_number), sizeof(int) - 1,
			ACC_P|ACC_PL_K|ACC_DATA, 0);

		mpt->ktss.ss0 = KERNEL_DS;
		mpt->ktss.io_bit_map_offset = 0x0FFF;	/* no IO bitmap */

		return mpt;
	}
}

/* Hacked from oskit's base_cpu_load.  */
void
mp_desc_load(struct mp_desc_table *mpt)
{
  struct pseudo_descriptor pdesc;

  /* Create a pseudo-descriptor describing the GDT.  */
  pdesc.limit = sizeof(mpt->gdt) - 1;
  pdesc.linear_base = kvtolin(mpt->gdt);

  /* Load it into the CPU.  */
  set_gdt(&pdesc);

  /*
   * Reload all the segment registers from the new GDT.
   */
  asm volatile("
ljmp	%0,$1f
1:
" : : "i" (KERNEL_CS));
  set_ds(KERNEL_DS);
  set_es(KERNEL_DS);
  set_ss(KERNEL_DS);

  /*
   * Clear out the FS and GS registers by default,
   * since they're not needed for normal execution of GCC code.
   */
  set_fs(0);
  set_gs(0);

  /* Create a pseudo-descriptor describing the IDT.  */
  pdesc.limit = sizeof(mpt->idt) - 1;
  pdesc.linear_base = kvtolin(mpt->idt);

  /* Load the IDT.  */
  set_idt(&pdesc);

  /* Make sure the TSS isn't marked busy.  */
  mpt->gdt[BASE_TSS / 8].access &= ~ACC_TSS_BUSY;

  /* Load the TSS.  */
  set_tr(BASE_TSS);
}



/*
 * Called after all CPUs have been found, but before the VM system
 * is running.  The machine array must show which CPUs exist.
 */
void
interrupt_stack_alloc()
{
	register int	i;
	vm_offset_t	stack_start;

	/*
	 * Allocate an interrupt stack for each CPU except for
	 * the master CPU (which uses the bootstrap stack)
	 *
	 * Set up pointers to the top of the interrupt stack.
	 */
	for (i = 0; i < NCPUS; i++) {
	    if (i == master_cpu) {
	      /* XXX We'll just use the initialization stack we're already
		 running on as the interrupt stack for now.  Later this will
		 have to change, because the init stack will get freed after
	         bootup.
	      */
		interrupt_stack[i] = (vm_offset_t) &base_stack_start;
		int_stack_top[i]   = (vm_offset_t) &base_stack_end;
	    }
	    else if (machine_slot[i].is_cpu) {
	      if (!init_alloc(INTSTACK_SIZE, &stack_start))
		panic("not enough memory for interrupt stacks");

		interrupt_stack[i] = stack_start;
		int_stack_top[i]   = stack_start + INTSTACK_SIZE;
	    }
	}
}

/* XXX should be adjusted per CPU speed */
int simple_lock_pause_loop = 100;

unsigned int simple_lock_pause_count = 0;	/* debugging */

void
simple_lock_pause()
{
	static volatile int dummy;
	int i;

	simple_lock_pause_count++;

	/*
	 * Used in loops that are trying to acquire locks out-of-order.
	 */

	for (i = 0; i < simple_lock_pause_loop; i++)
	    dummy++;	/* keep the compiler from optimizing the loop away */
}

#endif	/* NCPUS > 1 */
