/* An oskit COM stream-based incarnation of the "kmsg" device.

   This is an oskit-based kernel logging device in the style of Linux's
   /proc/kmsg magical file.  It is the backend used for all logging output
   from oskit components.  */

#include "ds_oskit.h"

#include <oskit/com.h>
#include <oskit/com/charqueue.h>

#define KMSG_BUFSIZE	8192

oskit_stream_t *kmsg_stream;
unsigned int kmsg_readers;

void
kmsg_init (void)
{
  kmsg_stream = oskit_charqueue_create (KMSG_BUFSIZE,
					OSKIT_CHARQUEUE_FULL_REPLACE);
  assert (kmsg_stream);
}
