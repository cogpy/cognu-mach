/* Support for minimal console's bell using PC speaker.
   This replaces the default oskit function that busy-waits.  */

#include <oskit/machine/pc/direct_cons.h>
#include <oskit/machine/pc/speaker.h>

#include "time_out.h"

#define BELL_FREQUENCY	750	/* Hz */
#define	BELL_DURATION	(hz>>3)	/* 1/8th second duration */

/* This is the private timer used to turn off the speaker.  */
static timer_elt_data_t bell_timer = { fcn: (int (*)()) &pc_speaker_off };

void
direct_cons_bell (void)
{
  /* Cancel the timeout for a bell we are already ringing.  */
  if (reset_timeout (&bell_timer))
    /* Turn the speaker off for an instant to distinguish this bell
       from the next.  */
    pc_speaker_off ();

  /* Turn the speaker on and set the timeout to turn it off.  */
  pc_speaker_on (BELL_FREQUENCY);
  set_timeout (&bell_timer, BELL_DURATION);
}
