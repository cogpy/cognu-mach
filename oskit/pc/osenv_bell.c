/* Support for minimal console's bell using PC speaker.
   This replaces the default oskit function that busy-waits.  */

#include <oskit/machine/pc/direct_cons.h>
#include <oskit/machine/pc/speaker.h>

#include "time_out.h"


#define BELL_FREQUENCY	750	/* Hz */

void
direct_cons_bell(void)
{
  pc_speaker_on (BELL_FREQUENCY);
  timeout (&pc_speaker_off, 0, hz / 8);	/* 1/8th second duration */
}
