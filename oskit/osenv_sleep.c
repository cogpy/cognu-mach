#include <oskit/dev/dev.h>

#include "sched_prim.h"


void
osenv_sleep_init(osenv_sleeprec_t *sr)
{
  sr->data[0] = (void *) OSENV_SLEEP_WAKEUP;
}

int
osenv_sleep(osenv_sleeprec_t *sr)
{
  assert_wait (sr, FALSE);
  thread_block (0);
  return (int) sr->data[0];
}

void
osenv_wakeup(osenv_sleeprec_t *sr, int wakeup_status)
{
  sr->data[0] = (void *) wakeup_status;
  thread_wakeup (sr);
}
