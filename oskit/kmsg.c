/*
 * An oskit COM stream-based incarnation of the "kmsg" device.
 */

#include "ds_oskit.h"

#include <oskit/com.h>
#include <oskit/com/charqueue.h>

#define KMSG_BUFSIZE	8192

oskit_stream_t *kmsg_stream;
unsigned int kmsg_readers;

kmsg_init()
{
  kmsg_stream = oskit_charqueue_create (KMSG_BUFSIZE,
					OSKIT_CHARQUEUE_FULL_REPLACE);
  assert (kmsg_stream);
}
