/*
 * Copyright (c) 1996, 1998, 1999 University of Utah and the Flux Group.
 * All rights reserved.
 *
 * This file is part of the Flux OSKit.  The OSKit is free software, also known
 * as "open source;" you can redistribute it and/or modify it under the terms
 * of the GNU General Public License (GPL), version 2, as published by the Free
 * Software Foundation (FSF).  To explore alternate licensing terms, contact
 * the University of Utah at csl-dist@cs.utah.edu or +1-801-585-3271.
 *
 * The OSKit is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GPL for more details.  You should have
 * received a copy of the GPL along with the OSKit; see the file COPYING.  If
 * not, write to the FSF, 59 Temple Place #330, Boston, MA 02111-1307, USA.
 */

#include "ds_oskit.h"

#include <stdarg.h>
#include <string.h>

#include <oskit/com.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/osenv_log.h>
#include <oskit/c/stdio.h>
#include <oskit/base_critical.h>

#include <oskit/com/stream.h>
#include <oskit/com/trivial_stream.h>


/*
 * There is one and only one log/panic interface in this implementation.
 */
static struct oskit_osenv_log_ops	osenv_log_ops;
static struct oskit_osenv_log		osenv_log_object = {&osenv_log_ops};

static OSKIT_COMDECL
log_query(oskit_osenv_log_t *s, const oskit_iid_t *iid, void **out_ihandle)
{
        if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
            memcmp(iid, &oskit_osenv_log_iid, sizeof(*iid)) == 0) {
                *out_ihandle = s;
                return 0;
        }

        *out_ihandle = 0;
        return OSKIT_E_NOINTERFACE;
};

static OSKIT_COMDECL_U
log_addref(oskit_osenv_log_t *s)
{
	return 1;
}

static OSKIT_COMDECL_U
log_release(oskit_osenv_log_t *s)
{
	return 1;
}


static const char *const log_prio_names[] =
{
  [OSENV_LOG_EMERG] = "EMERGENCY",
  [OSENV_LOG_ALERT] = "ALERT",
  [OSENV_LOG_CRIT] = "CRITICAL ERROR",
  [OSENV_LOG_ERR] = "ERROR",
  [OSENV_LOG_WARNING] = "WARNING",
  [OSENV_LOG_NOTICE] = "NOTICE",
  [OSENV_LOG_INFO] = "INFO",
  [OSENV_LOG_DEBUG] = "DEBUG",
};

static int midline, last_prio = -1;
static int
cooked_putchar (int c)
{
  oskit_error_t rc;
  oskit_u32_t actual;

  if (!midline)
    {
      com_printf(kmsg_stream, "<%u>", last_prio);
      if (last_prio < OSENV_LOG_DEBUG && kmsg_readers == 0)
	{
	  if (last_prio == OSENV_LOG_INFO)
	    com_printf (ds_console_stream, "(device driver): ");
	  else
	    com_printf (ds_console_stream, "(device driver) %s: ",
			log_prio_names[last_prio]);
	}
    }

  midline = (c != '\n');

  if (last_prio >= OSENV_LOG_DEBUG || kmsg_readers)
    return (unsigned char)c;

  if (c == '\n')
    rc = oskit_stream_write (ds_console_stream, "\r\n", 2, &actual);
  else
    {
      unsigned char cc = c;
      rc = oskit_stream_write (ds_console_stream, &cc, 1, &actual);
    }

  return OSKIT_FAILED(rc) ? -1 : (unsigned char)c;
}
static struct oskit_trivial_stream log_impl =
{ { &oskit_trivial_stream_ops }, putchar: cooked_putchar };


static OSKIT_COMDECL_V
log_vlog(oskit_osenv_log_t *o, int priority, const char *fmt, void *args)
{
  base_critical_enter();

  if (priority != last_prio)
    {
      midline = 0;
      last_prio = priority;
    }

  /* This run prints to the console if that's enabled,
     and it maintains the "midline" state and prints a
     priority token to the kmsg stream for each new line.
     We assume there will be at most one new line per call.  */
  com_vprintf(&log_impl.streami, fmt, args);

  /* Now this run writes the message to the kmsg stream,
     after any priority token written by above.  */
  com_vprintf(kmsg_stream, fmt, args);

  base_critical_leave();
}

static OSKIT_COMDECL_V
log_log(oskit_osenv_log_t *o, int priority, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	log_vlog(o, priority, fmt, args);
	va_end(args);
}

static OSKIT_COMDECL_V
log_vpanic(oskit_osenv_log_t *o, const char *fmt, void *args)
{
	log_vlog(o, OSENV_LOG_EMERG, fmt, args);

	vprintf(fmt, args);
	panic("\r\npanic in device driver!");
}

static OSKIT_COMDECL_V
log_panic(oskit_osenv_log_t *o, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	log_vpanic(o, fmt, args);
	va_end(args);
}

static struct oskit_osenv_log_ops osenv_log_ops = {
	log_query,
	log_addref,
	log_release,
	log_log,
	log_vlog,
	log_panic,
	log_vpanic,
};


/*
 * Return a reference to the one and only interrupt object.
 */
oskit_osenv_log_t *
oskit_create_osenv_log(void)
{
	return &osenv_log_object;
}
