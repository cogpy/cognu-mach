/* main entrypoint (from oskit multiboot_main) for Mach.
   It turns out this file is almost entirely machine-dependent.

   This file defines `main', which is the entrypoint from the oskit.
   It calls `setup_main' (kern/startup.c) after setting up the machine,
   interrupts, paging, and `machine_slot[0]'; setup_main never returns.
   This file defines `machine_init', which is called after VM is set up.
*/

#include <oskit/clientos.h>
#include <oskit/machine/base_multiboot.h>
#include <oskit/machine/base_stack.h>
#include <oskit/machine/physmem.h>
#include <oskit/x86/base_cpu.h>
#include <oskit/x86/debug_reg.h>
#include <oskit/lmm.h>
#include <oskit/machine/phys_lmm.h>
#include <oskit/machine/base_stack.h>

#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>

#include <oskit/x86/proc_reg.h>
#include <oskit/x86/paging.h>
#include <oskit/x86/base_vm.h>
#include <oskit/x86/base_gdt.h>
#include <oskit/c/unistd.h>
#include <oskit/dev/dev.h>

#include "vm_param.h"
#include <kern/time_out.h>
#include <sys/time.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <kern/zalloc.h>
#include <i386/machspl.h>
#include <i386/pmap.h>


static void my_exit (int), (*his_exit) (int);

/* XXX move to intel/pmap.h */
extern pt_entry_t *kernel_page_dir;


#include "assert.h"

extern char	version[];

vm_offset_t int_stack_top, int_stack_high; /* XXX */


char **kernel_argv;
char *kernel_cmdline;		/* XXX */

int
main (int argc, char **argv)
{
  oskit_clientos_init ();

  printf ("Welcome to %s!\r\n", version);

  /*
   * Initialize the PIC prior to any possible call to an spl.
   */
  picinit();

  /* Tell the oskit base_* code what virtual addresses we are using
     to map the linear address space.  It can't keep using the 1:1
     kvtolin mapping once we flush the direct mapping below.
     We must do this before pmap_bootstrap, because pmap_bootstrap
     uses kvtolin to decide where to put its mappings!  */
  linear_base_va = -LINEAR_MIN_KERNEL_ADDRESS;

  /* This allocates the kernel page tables and initializes kernel_pmap.  */
  pmap_bootstrap();

  /*
   * Turn paging on.
   * We'll have to temporarily install a direct mapping
   * between physical memory and low linear memory,
   * until we start using our new kernel segment descriptors.
   * One page table (4MB) should do the trick.
   * Also, set the WP bit so that on 486 or better processors
   * page-level write protection works in kernel mode.
   */
  kernel_page_dir[lin2pdenum(0)] =
    kernel_page_dir[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS)];
  paging_enable((oskit_addr_t) kernel_page_dir);
  set_cr0 (get_cr0 () | CR0_WP);

  if (base_cpuid.feature_flags & CPUF_PAGE_GLOBAL_EXT) {
    /*
     * The processor supports the "global" bit to avoid flushing kernel TLB
     * entries, if we turn it on.  pmap_bootstrap checks this feature flag
     * and begins use the global bit in page table entries.  But according
     * to the x86 specs we cannot set this bit before we do enable_paging
     * above; setting CR4_PGE first doesn't work on some processors, in fact.
     */
    set_cr4 (get_cr4 () | CR4_PGE);
  }

  /*
   * Initialize and activate the real i386 protected-mode structures.
   */
  base_gdt_init();	/* reinitialize with linear_base_va */
  gdt_init();
  idt_init();
  int_init();
  ldt_init();

  base_cpu_load();

  /* Arrange a callback to our special exit function below, so we can
     try to return to a state the generic oskit reboot code can cope with.  */
  his_exit = oskit_libc_exit;
  oskit_libc_exit = &my_exit;

  /* Get rid of the temporary direct mapping and flush it out of the TLB.  */
  kernel_page_dir[lin2pdenum(0)] = 0;
  inval_tlb();

  /* XXX We'll just use the initialization stack we're already running on
     as the interrupt stack for now.  Later this will have to change,
     because the init stack will get freed after bootup.  */
  int_stack_top = (int)&base_stack_end;

  /* Interrupt stacks are allocated in physical memory,
     while kernel stacks are allocated in kernel virtual memory,
  so phys_last_addr serves as a convenient dividing point.  */
  int_stack_high = phys_mem_max;

  /* Examine the CPU model information provided by the oskit,
     and set machine_slot[0] to describe the CPU to users who ask.  */
  switch (base_cpuid.family)
    {
    default:
    case CPU_FAMILY_386:
      machine_slot[0].cpu_type = CPU_TYPE_I386;
      break;
    case CPU_FAMILY_486:
      machine_slot[0].cpu_type = CPU_TYPE_I486;
      break;
    case CPU_FAMILY_PENTIUM:
      machine_slot[0].cpu_type = CPU_TYPE_PENTIUM;
      break;
    case CPU_FAMILY_PENTIUM_PRO:
      machine_slot[0].cpu_type = CPU_TYPE_PENTIUMPRO;
      break;
    }
  machine_slot[0].cpu_subtype = CPU_SUBTYPE_AT386;
  machine_slot[0].is_cpu = TRUE;
  machine_slot[0].running = TRUE;

  kernel_argv = argv;		/* Stash our args for user_bootstrap to use. */

  { /* XXX */
    static char cmdline[1024];
    int i;
    strcpy (cmdline, argv[0]);
    for (i = 1; i < argc; ++i) {
      strcat (cmdline, " ");
      strcat (cmdline, argv[i]);
    }
    assert (strlen (cmdline) < sizeof cmdline);
    kernel_cmdline = cmdline;
  }

  /* Start the system.  This function does not return.  */
  setup_main();
  return -1;
}

/* This is the function we install in `oskit_libc_exit' to be called
   by _exit, panic, et al.  The oskit provided an original function
   (now stored in `his_exit'), that will only work properly if we are
   using direct-mapped physical addresses.  So we provide here a
   replacement that switches to direct linear addressing and moves
   to a physical-addressed stack and PC to call the oskit's function.  */
static void
my_exit (int rc)
{
  /* Restore direct virtual->physical mapping and switch to
     direct linear addressing code segment, so the oskit
     can cope when it tries to turn paging off.  */

  kernel_page_dir[lin2pdenum(0)]
    = kernel_page_dir[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS)];
  set_cr4 (get_cr4 () &~ CR4_PGE);
  set_pdbr (kvtophys (kernel_page_dir));

  asm volatile ("	ljmp	%0,$1f	\n" /* Switch to LINEAR_CS,  */
		"1:	movw	%w1,%%ds\n" /* Switch %ds to LINEAR_DS.  */
		"	movw	%w1,%%es\n" /* Switch %es to LINEAR_DS.  */
		"	movw	%w1,%%ss\n" /* Switch %ss to LINEAR_DS.  */
		"	movl	%2,%%esp\n" /* and to phys-addr base_stack.  */
		"	pushl	%3	\n" /* Push argument (RC).  */
		"	pushl	%4	\n" /* Push bogus return address.  */
		"	jmp	%*%5" : : /* Jump to oskit, never return.  */
		"i" (LINEAR_CS), "r" (LINEAR_DS),
		"ir" (kvtophys (&base_stack_end)),
		"ir" (rc), "ir" (0),
		"r" (kvtophys (his_exit)));
  /* NOTREACHED */
}

boolean_t pmap_valid_page(x)
	vm_offset_t x;
{
	/* XXX is this OK?  What does it matter for?  */
	return (((phys_mem_min <= x) && (x < phys_mem_max)) &&
		!(((boot_info.mem_lower * 1024) <= x) && (x < 1024*1024)));
}



#include <mach/time_value.h>

startrtclock()
{
	clkstart();
}

static void
inittodr()
{
  oskit_timespec_t ts;
  oskit_error_t rc;
  spl_t s;

  rc = oskit_rtc_get (&ts);
  if (rc)
    panic (__FUNCTION__);

  s = splhigh();
#undef tv_sec			/* oy */
  time.seconds = ts.tv_sec;
  time.microseconds = (ts.tv_nsec + 999) / 1000;
  splx(s);
}

/* This is called from host_set_time at splhigh to reset the hardware clock
   to the new value of `time'.  */
void
resettodr()
{
  oskit_timespec_t ts = { time.seconds, time.microseconds * 1000 };
  oskit_rtc_set (&ts);
}



void
halt_cpu ()
{
  while (1)
    asm volatile ("cli; hlt");
}

void
halt_all_cpus(reboot)
	boolean_t	reboot;
{
  exit(reboot ? 0 : 1);
}


void machine_init()
{
	/*
	 * Set up to use floating point.
	 */
	init_fpu();

	/*
	 * Get the time
	 */
	inittodr();

	/*
	 * Unmap page 0 to trap NULL references.
	 * If there is real memory in the first physical page,
	 * then it will not be accessible through the normal
	 * direct mapping, so we need to take it out of the LMM.
	 */
	if (phys_mem_min < PAGE_SIZE)
	  {
	    void *block = lmm_alloc_gen (&malloc_lmm,
					 PAGE_SIZE - phys_mem_min, 0, 0, 0,
					 phys_mem_min,
					 PAGE_SIZE - phys_mem_min);
	    if ((oskit_addr_t) block != phys_mem_min)
	      panic ("cannot allocate first page [%#x,%#x) from physical LMM!",
		     phys_mem_min, PAGE_SIZE);
	    else
	      {
		/*
		 * So we have this partial page that we can't use where it is.
		 * Rather than waste it, let's map it in someplace else
		 * and donate it to someplace that can always use a little
		 * chunk of extra wired kernel virtual memory: the zone system.
		 */
		extern vm_map_t zone_map; /* zalloc.c */
		extern zone_t vm_page_zone; /* vm_resident.c */
		vm_offset_t kva;
		kern_return_t kr;
		kr = kmem_alloc_pageable (zone_map, &kva, PAGE_SIZE);
		if (kr != KERN_SUCCESS)
		  panic ("machine_init: kmem_alloc_pageable zone_map: %#x",
			 kr);
		pmap_enter (kernel_pmap, kva, 0,
			    VM_PROT_READ | VM_PROT_WRITE, TRUE);
		kva += phys_mem_min; /* Skip mapping below phys_mem_min.  */
		/*
		 * The vm_page_zone is always a needy soul early in life.
		 */
		zcram (vm_page_zone, kva, PAGE_SIZE - phys_mem_min);
	      }
	  }
	pmap_unmap_page_zero();

	/* Catch interrupt stack overflow.  */
	set_b0 (kvtolin (&base_stack_start), DR7_LEN_4, DR7_RW_DATA);
	base_gdt_load();	/* necessary after setting debug regs */
}

/* XXX temp stubs */
void iopb_init() {}
void iopb_destroy() {}

#include <device/dev_hdr.h>
#include <mach/mig_errors.h>

kern_return_t i386_io_port_add(thread_t thread, device_t device)
{ return MIG_BAD_ID; }
kern_return_t i386_io_port_remove(thread_t thread, device_t device)
{ return MIG_BAD_ID; }
kern_return_t
i386_io_port_list(thread_t thread, device_t **list, unsigned int *list_count)
{ return MIG_BAD_ID; }
