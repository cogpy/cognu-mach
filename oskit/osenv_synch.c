#include <oskit/dev/dev.h>

#include <machine/spl.h>
#include "ds_oskit.h"

extern spl_t curr_ipl;

/*
 * Enable/disable interrupts.
 */

static spl_t osenv_intr_spl;

void
osenv_intr_disable(void)
{
  /* We can be called with interrupts already disabled! */
  if (curr_ipl > SPLIO)
    /* We are already at higher priority than oskit code normally runs.
       I think this only happens in the calls from oskit_rtc_{get,set}.
       On the assumption that osenv_intr_enable will be called in
       parity from the same interrupt level, we will want to stay at the
       same high interrupt level.  */
    osenv_intr_spl = curr_ipl;
  else if (curr_ipl < SPLIO)
    /* We are at a level where oskit interrupts are enabled, so we must go
       to splio.  osenv_intr_enable we will return to the current level.  */
    osenv_intr_spl = splio ();
}

void
osenv_intr_enable(void)
{
  /* We assume we are at splio or higher.  */
  spl_t s = osenv_intr_spl;
  osenv_intr_spl = SPL0;
  splx (s);
}

/*
 * Return the current interrupt enable flag.
 */
int
osenv_intr_enabled(void)
{
  return curr_ipl < SPLIO;
}
