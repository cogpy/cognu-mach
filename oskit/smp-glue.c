#include <cpus.h>

#if MULTIPROCESSOR

#include <oskit/smp.h>
#include <machine/mp_desc.h>
#include <kern/cpu_number.h>
#include <mach/kern_return.h>
#include <mach/machine.h>

extern void slave_main (void) __attribute__ ((noreturn)); /* kern/startup.c */


void
interrupt_processor (int which_cpu)
{
  smp_message_pass (which_cpu);
}

static void
secondary_cpu (void *arg)
{
  const int mycpu = (int) arg;

  assert (mycpu == smp_find_cur_cpu ());

  mp_desc_load (mp_desc_init (mycpu));
  setup_machine_slot (mycpu);

  smp_message_pass_enable[mycpu] = 1;

  slave_main ();
  /* NOTREACHED */
}

void
start_other_cpus (void)
{
  int cpu;

  smp_message_pass_enable[master_cpu] = 1;

  cpu = 0;
  while ((cpu = smp_find_cpu (cpu)) >= 0)
    if (cpu != master_cpu)
      smp_start_cpu (cpu,
		     &secondary_cpu, (void *) cpu,
		     (void *) int_stack_top[cpu]);
}

kern_return_t
cpu_control(int cpu, int *info, int count)
{
printf("cpu_control %d\n", cpu);
	return KERN_FAILURE;
}

kern_return_t
cpu_start(int cpu)
{
  if (machine_slot[cpu].running)
    return KERN_FAILURE;

  smp_start_cpu (cpu,
		 &secondary_cpu, (void *) cpu,
		 (void *) int_stack_top[cpu]);

  return KERN_SUCCESS;		/* not synchronous */
}

#endif
