#include "ds_oskit.h"

#include <oskit/dev/dev.h>
#include <oskit/dev/linux.h>
#include <oskit/dev/osenv_irq.h>
#include <oskit/dev/osenv_intr.h>
#include <oskit/dev/osenv_sleep.h>
#include <oskit/c/stdlib.h>

#include <oskit/c/termios.h>
#include <oskit/tty.h>

oskit_osenv_t *mach_osenv;

extern int serial_console;	/* base_console_init.c */

oskit_stream_t *ds_console_stream;

extern int console_irq;		/* kludge in osenv_irq.c */


void
ds_osenv_init (void)
{
  oskit_error_t rc;

  mach_osenv = oskit_osenv_create_default ();

  kmsg_init ();

  oskit_dev_init (mach_osenv);

  if (getenv ("BOGUS_CONSOLE"))
    {
      extern oskit_stream_t *console;
      ds_console_stream = console;
      printf("using bogus console!\n");
      rc = 0;
    }
  else if (serial_console)
    {
      oskit_osenv_intr_t *intr = oskit_create_osenv_intr ();
      char *p = getenv ("COM_CONS");
      int port = p ? atoi(p) : 1;
      struct termios param = base_cooked_termios;
      param.c_lflag &= ~ECHO;
      param.c_iflag &= ~ICRNL;
      param.c_oflag &= ~OPOST;
      console_irq = -1;
      rc = cq_com_console_init (port, &param,
				oskit_create_osenv_irq (),
				intr,
				oskit_create_osenv_sleep (intr),
				&ds_console_stream);
    }
  else
    {
      oskit_osenv_intr_t *intr = oskit_create_osenv_intr ();
      console_irq = -1;
      rc = cq_direct_console_init (oskit_create_osenv_irq (),
				   intr,
				   oskit_create_osenv_sleep (intr),
				   &ds_console_stream);
    }
  if (rc)
    panic ("cannot create interrupt-driven console stream: %x\n", rc);

  oskit_linux_init_osenv (mach_osenv);
  oskit_linux_init_devs ();

#ifdef HAVE_I8042
  oskit_dev_init_i8042 ();
#endif
}
