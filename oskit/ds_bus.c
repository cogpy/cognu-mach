/* This is a total crock of shit.  */

#include "ds_oskit.h"

#include <oskit/error.h>
#include <oskit/dev/bus.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <string.h>


oskit_error_t
populate_bus (device_t dev, oskit_bus_t *bus)
{
  kern_return_t kr;
  oskit_error_t rc;
  unsigned int i;
  char *p;

  /* First, we probe the bus so it knows what devices are on it.  */
  rc = oskit_bus_probe (bus);
  if (OSKIT_FAILED (rc) && rc != OSKIT_E_NOTIMPL)
    return rc;

  /* Next, we will get a page of kernel memory to hold our ridiculous
     text representation of the bus's device list.  */

  kr = kmem_alloc_pageable (kernel_map, (vm_offset_t *) &dev->com.bus.contents,
			    PAGE_SIZE);
  if (kr)
    return OSKIT_E_OUTOFMEMORY;

  p = dev->com.bus.contents;
  i = 0;
  do
    {
      inline void append (char sep, const char *str)
	{
	  extern char *stpcpy (char *, const char *);
	  if (str)
	    {
	      *p++ = sep;
	      p = stpcpy (p, str);
	    }
	}

      char pos[OSKIT_BUS_POS_MAX];
      oskit_device_t *child;
      oskit_devinfo_t info;

      rc = oskit_bus_getchild (bus, i++, &child, pos);
      if (rc == OSKIT_E_DEV_NOMORE_CHILDREN)
	break;
      if (OSKIT_SUCCEEDED (rc))
	{
	  rc = oskit_device_getinfo (child, &info);
	  oskit_device_release (child);
	}
      if (OSKIT_FAILED (rc))
	{
	  kmem_free (kernel_map,
		     (vm_offset_t) dev->com.bus.contents, PAGE_SIZE);
	  return rc;
	}

      append ('@', pos);
      append (':', info.name);
      append ('=', info.description);
      append (';', info.author);
      append ('/', info.version);
      *p++ = '\n';

      assert (p < dev->com.bus.contents + PAGE_SIZE);
    } while (rc == 0);

  dev->com.bus.size = p - dev->com.bus.contents;
  memset (p, 0, PAGE_SIZE - dev->com.bus.size);

  return 0;
}

void
ds_bus_close (device_t dev)
{
  kmem_free (kernel_map, (vm_offset_t) dev->com.bus.contents, PAGE_SIZE);
}

io_return_t
ds_bus_read (device_t dev, ipc_port_t reply_port,
	     mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	     recnum_t recnum, int count, io_buf_ptr_t *data,
	     unsigned *bytes_read)
{
  if (recnum % PAGE_SIZE != 0)
    return D_INVALID_OPERATION;	/* let generic code use inband read */

  if (recnum + count > dev->com.bus.size)
    INVALREC;

  if (count == 0)
    {
      *bytes_read = 0;
      return D_SUCCESS;
    }

  *bytes_read = count;
  return vm_map_copyin_page_list (kernel_map,
				  (vm_offset_t) dev->com.bus.contents + recnum,
				  round_page (count),
				  FALSE, FALSE, (vm_map_copy_t *) data,
				  FALSE);
}

io_return_t
ds_bus_read_inband (device_t dev, ipc_port_t reply_port,
		    mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		    recnum_t recnum, int count, char *data,
		    unsigned *bytes_read)
{
  if (recnum + count > dev->com.bus.size)
    INVALREC;
  memcpy (data, dev->com.bus.contents + recnum, count);
  *bytes_read = count;
  return D_SUCCESS;
}

io_return_t
ds_bus_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *status_count)
{
  switch (flavor)
    {
    case DEV_GET_SIZE:
      status[DEV_GET_SIZE_RECORD_SIZE] = 1;
      status[DEV_GET_SIZE_DEVICE_SIZE] = dev->com.bus.size;
      *status_count = 2;
      return D_SUCCESS;
    }
  INVALOP;
}


const struct device_ops bus_device_ops =
{
  read_inband: ds_bus_read_inband,
  read: ds_bus_read,
  get_status: ds_bus_get_status,
  close: ds_bus_close
};
