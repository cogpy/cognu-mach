/*
 * Mach device server routines (oskit version).
 *
 * Copyright (c) 1999 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 */

#include "ds_oskit.h"

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mig_errors.h>
#include <mach/port.h>
#include <mach/notify.h>
#include <mach/time_value.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include "pmap.h"

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <oskit/dev/dev.h>
#include <oskit/error.h>
#include <oskit/unsupp/bus_walk.h>
#include <oskit/dev/bus.h>
#include <oskit/dev/blk.h>
#include <oskit/dev/net.h>
#include <oskit/dev/linux.h>
#include <oskit/com/stream.h>
#include <oskit/c/stdlib.h>

#include <oskit/machine/pc/direct_cons.h> /* XXX direct_cons_set_flags */

#include <device/device_port.h>

#include <string.h>


#define NDEVICES		256
#define	DEVICE_IO_MAP_SIZE	(4 * 1024 * 1024)

zone_t dev_hdr_zone;
zone_t io_inband_zone; /* for inband reads */

queue_head_t		io_done_list;
decl_simple_lock_data(,	io_done_list_lock)

/* We maintain a hash table mapping COM oskit_device_t pointers to
   open device ports (device_t).   */

#define	NDEVHASH		7
#define	DEV_PTR_HASH(com)	((unsigned int) (com) % NDEVHASH)
queue_head_t dev_hash_table[NDEVHASH];

/*
 * Lock for device-number to device lookup.
 * Must be held before device-ref_count lock.
 */
decl_simple_lock_data(,dev_hash_lock)

static void
dev_hash_enter (device_t device)
{
  queue_enter (&dev_hash_table[DEV_PTR_HASH(device->com_device)],
	       device, device_t, hash_chain);
}

static void
dev_hash_remove (device_t device)
{
  queue_remove (&dev_hash_table[DEV_PTR_HASH(device->com_device)],
		device, device_t, hash_chain);
}


static device_t
dev_hash_lookup (oskit_device_t *com_device, dev_mode_t mode)
{
  queue_t q;
  device_t device;

  q = &dev_hash_table[DEV_PTR_HASH (com_device)];
  queue_iterate (q, device, device_t, hash_chain)
    if (device->com_device == com_device && (mode &~ device->mode) == 0)
      return device;

  return DEVICE_NULL;
}

/*
 * Add a reference to the device.
 */
void
device_reference(device_t device)
{
  simple_lock(&device->ref_lock);
  device->ref_count++;
  simple_unlock(&device->ref_lock);
}

/*
 * Remove a reference to the device, and deallocate the
 * structure if no references are left.
 */
void
device_deallocate (device_t device)
{
  simple_lock (&device->ref_lock);
  if (device->ref_count > 1) {
    --device->ref_count;
    simple_unlock (&device->ref_lock);
    return;
  }
  simple_unlock (&device->ref_lock);

  assert (device->com_device);

  simple_lock(&dev_hash_lock);
  simple_lock(&device->ref_lock);
  if (--device->ref_count > 0) {
    simple_unlock (&device->ref_lock);
    simple_unlock (&dev_hash_lock);
    return;
  }

  dev_hash_remove (device);
  simple_unlock(&device->ref_lock);
  simple_unlock(&dev_hash_lock);

  /* Destroy the port.  */
  ipc_kobject_set (device->port, IKO_NULL, IKOT_NONE);
  ipc_port_dealloc_kernel (device->port);
  device->port = IP_NULL;

  DEV_LOCK (device);
  if (device->ops && device->ops->close)
    (*device->ops->close) (device);
  if (device->com_device)	/* close hook might have cleared it */
    oskit_device_release (device->com_device);
  DEV_UNLOCK (device);

  zfree(dev_hdr_zone, (vm_offset_t)device);
}

const struct device_ops no_device_ops;

void ds_init()
{
	vm_offset_t	device_io_min, device_io_max;
	unsigned int i;

	queue_init(&io_done_list);
	simple_lock_init(&io_done_list_lock);

	device_io_map = kmem_suballoc(kernel_map,
				      &device_io_min,
				      &device_io_max,
				      DEVICE_IO_MAP_SIZE,
				      FALSE);
	/*
	 *	If the kernel receives many device_write requests, the
	 *	device_io_map might run out of space.  To prevent
	 *	device_write_get from failing in this case, we enable
	 *	wait_for_space on the map.  This causes kmem_io_map_copyout
	 *	to block until there is sufficient space.
	 *	(XXX Large writes may be starved by small writes.)
	 *
	 *	There is a potential deadlock problem with this solution,
	 *	if a device_write from the default pager has to wait
	 *	for the completion of a device_write which needs to wait
	 *	for memory allocation.  Hence, once device_write_get
	 *	allocates space in device_io_map, no blocking memory
	 *	allocations should happen until device_write_dealloc
	 *	frees the space.  (XXX A large write might starve
	 *	a small write from the default pager.)
	 */
	device_io_map->wait_for_space = TRUE;

	dev_hdr_zone = zinit(sizeof(struct device),
			     sizeof(struct device) * NDEVICES,
			     PAGE_SIZE,
			     FALSE,
			     "open device entry");

	io_inband_zone = zinit(sizeof(io_buf_ptr_inband_t),
			    1000 * sizeof(io_buf_ptr_inband_t),
			    10 * sizeof(io_buf_ptr_inband_t),
			    FALSE,
			    "io inband read buffers");

	simple_lock_init(&dev_hash_lock);
	for (i = 0; i < NDEVHASH; i++)
	    queue_init(&dev_hash_table[i]);

	ds_osenv_init();
	ds_request_init();

	DEV_LOCK_INIT;
}


boolean_t
ds_notify (mach_msg_header_t *msg)
{
  if (msg->msgh_id == MACH_NOTIFY_NO_SENDERS)
    {
      device_t dev;
      mach_no_senders_notification_t *ns;

      ns = (mach_no_senders_notification_t *) msg;
      dev = dev_port_lookup ((ipc_port_t) ns->not_header.msgh_remote_port);
      assert (dev);

      /* Extant send rights held one ref on the device object.  */
      device_deallocate (dev);

      return TRUE;
    }

  printf ("ds_notify: strange notification %d\n", msg->msgh_id);
  return FALSE;
}


ipc_port_t
convert_device_to_port (device_t device)
{
  ipc_port_t port;

  if (! device)
    return IP_NULL;
  device_lock(device);
  port = ipc_port_make_send(device->port);
  device_unlock(device);
  device_deallocate(device);
  return port;
}


/*
 * Lookup a device by its port.
 * Doesn't consume the naked send right; produces a device reference.
 */
device_t
dev_port_lookup(port)
	ipc_port_t	port;
{
	register device_t	device;

	if (!IP_VALID(port))
	    return (DEVICE_NULL);

	ip_lock(port);
	if (ip_active(port) && (ip_kotype(port) == IKOT_DEVICE)) {
	    device = (device_t) port->ip_kobject;
	    device_reference(device);
	}
	else
	    device = DEVICE_NULL;

	ip_unlock(port);
	return (device);
}


/*** Opening devices.  ***/


static device_t
dev_open_alloc (void)
{
  device_t dev = (device_t) zalloc (dev_hdr_zone);
  simple_lock_init(&dev->ref_lock);
  dev->ref_count = 1;
  simple_lock_init(&dev->lock);

  /*
   * Allocate port, keeping a reference for it.
   */
  dev->port = ipc_port_alloc_kernel ();
  if (dev->port == IP_NULL)
    {
      zfree (dev_hdr_zone, (vm_offset_t) dev);
      return DEVICE_NULL;
    }

  /* Associate the port with the device.  */
  ipc_kobject_set (dev->port, (ipc_kobject_t) dev, IKOT_DEVICE);

  return dev;
}


static void
setup_no_senders (device_t dev)
{
  ipc_port_t notify;

  /*
   * Request no-senders notifications on device port.
   * One ref in DEV->ref_count is held by the existence of send rights;
   * the no-senders notification will release that ref.
   */
  notify = ipc_port_make_sonce(dev->port);
  ip_lock(dev->port);
  ipc_port_nsrequest(dev->port, 1, notify, &notify);
  assert(notify == IP_NULL);

  device_reference (dev);
}


/* Given a COM device object and an i/o mode, produce an open device.
   Always consumes a ref on COM_DEVICE.  */
static io_return_t
dev_open_com (oskit_device_t *com_device, dev_mode_t mode, device_t *devp,
	      const char *diskpart)
{
  device_t dev;
  const struct device_ops *ops;

  /* First we look for an existing device port already open.  */
 lookup:
  simple_lock (&dev_hash_lock);
  dev = dev_hash_lookup (com_device, mode);
  if (dev != DEVICE_NULL)
    {
      device_lock (dev);
      ++dev->ref_count;
      simple_unlock (&dev_hash_lock);

      DEV_LOCK (dev);
      oskit_device_release (com_device); /* consume ref passed in */
      DEV_UNLOCK (dev);
      if (dev->ops == 0)
	{
	  /* Somebody else is blocked in the oskit getting this device open.
	     We will block until they wake us up.  */
	  dev->mode |= D_NODELAY;	/* mark for opener */
	  thread_sleep ((event_t) dev, simple_lock_addr (dev->lock), TRUE);
	  /* Repeat the lookup in case DEV got killed in a failed open.  */
	  goto lookup;
	}
      else
	device_unlock (dev);

      goto got_device;
    }
  simple_unlock (&dev_hash_lock);

  /* Nope!  We will need a new device port.  */

  dev = dev_open_alloc ();
  if (!dev)
      return KERN_RESOURCE_SHORTAGE;

  dev->com_device = com_device;
  dev->mode = mode;
  dev->ops = 0;

  /* Put the device in the hash table under its COM device.
     After this point we need to use device_lock.  */
  simple_lock (&dev_hash_lock);
  dev_hash_enter (dev);
  simple_unlock (&dev_hash_lock);

  /* Now we must open the oskit device, first determining which flavor it is.
   */
  {
    oskit_bus_t *bus;
    oskit_blkdev_t *blkdev;
    oskit_netdev_t *netdev;
    oskit_error_t rc;
    inline void asyncio_init (device_t dev)
      {
	dev->com.stream.listening = 0;
	queue_init (&dev->com.stream.read_queue);
	queue_init (&dev->com.stream.write_queue);
	dev->com.stream.ready_queue.next = 0;
      }

    if (oskit_device_query (com_device, &oskit_stream_iid,
			    (void **) &dev->com.stream.io) == 0)
      {
	/* Special case for minimal console stream, not really a device.  */
	if (oskit_stream_query (dev->com.stream.io, &oskit_asyncio_iid,
				(void **) &dev->com.stream.aio) == 0)
	  {
	    asyncio_init (dev);
	    ops = &asyncio_device_ops;
	  }
	else
	  ops = &stream_device_ops;
	rc = 0;

	/* Kludge for kmsg.  */
	if ((void *) com_device == kmsg_stream && (mode & D_READ))
	  ++kmsg_readers;
      }
    else if (oskit_device_query (com_device, &oskit_bus_iid,
				 (void **) &bus) == 0)
      {
	if (mode & D_WRITE)
	  {
	    device_lock (dev);
	    mode = dev->mode & D_NODELAY;
	    device_unlock (dev);
	    if (mode & D_NODELAY)
	      /* Someone was waiting for us to finish opening.  */
	      thread_wakeup ((event_t)dev);
	    device_deallocate (dev);
	    return D_READ_ONLY;
	  }
	ops = &bus_device_ops;
	rc = populate_bus (dev, bus);
	oskit_bus_release (bus);
      }
    else if (oskit_device_query (com_device, &oskit_blkdev_iid,
				 (void **) &blkdev) == 0)
      {
	ops = &block_device_ops;
	rc = oskit_blkdev_open (blkdev,
				((mode & D_READ) ? OSKIT_DEV_OPEN_READ : 0) |
				((mode & D_WRITE) ? OSKIT_DEV_OPEN_WRITE : 0),
				&dev->com.blk.io);
	oskit_blkdev_release (blkdev);
	if (OSKIT_SUCCEEDED (rc))
	  dev->com.blk.size = oskit_blkio_getblocksize (dev->com.blk.io);
	dev->com.blk.parts = 0;
      }
    else if (oskit_device_query (com_device, &oskit_netdev_iid,
				 (void **) &netdev) == 0)
      {
	ops = &net_device_ops;
	rc = ds_netdev_open (dev, netdev);
      }

    if (OSKIT_FAILED (rc))
      {
	if (dev->mode & D_NODELAY)
	  /* Someone was waiting for us to finish opening.  */
	  thread_wakeup ((event_t)dev);
	device_deallocate (dev);
	return oskit_to_mach_error (rc);
      }
  }

  device_lock (dev);
  dev->ops = ops;
  mode = dev->mode & D_NODELAY;
  dev->mode &= ~D_NODELAY;
  device_unlock (dev);
  if (mode)
    /* Someone was waiting for us to finish opening.  */
    thread_wakeup ((event_t)dev);

  setup_no_senders (dev);

  /* Return the open device.  */

 got_device:

  if (diskpart)
    {
      diskpart_t *part;
      device_t subdev;

      /* Kludge for partitioning.  */
      if (dev->ops != &block_device_ops)
	{
	  device_deallocate (dev);
	  return D_NO_SUCH_DEVICE;
	}

      device_lock (dev);
      if (dev->com.blk.parts == 0)
	{
	  diskpart_t *parts = (void *) kalloc (MAX_PARTS * sizeof *parts);
	  int n = diskpart_blkio_get_partition (dev->com.blk.io,
						parts, MAX_PARTS);
	  if (n <= 0)
	    {
	      device_unlock (dev);
	      kfree ((vm_offset_t) parts, MAX_PARTS * sizeof *parts);
	      device_deallocate (dev);
	      return D_NO_SUCH_DEVICE;
	    }
	  dev->com.blk.parts = parts;
	}
      device_unlock (dev);

      part = diskpart_lookup_bsd_string (dev->com.blk.parts, diskpart);
      if (part == 0)
	{
	  device_deallocate (dev);
	  return D_NO_SUCH_DEVICE;
	}

      /* Now we have the pointer to the partition we want.  For partition
	 pseudo-devices, we use this pointer in lieu of the COM object
	 pointer as the key in the device hash table.  */
      simple_lock (&dev_hash_lock);
      subdev = dev_hash_lookup ((void *) part, mode);
      if (subdev != DEVICE_NULL)
	{
	  device_lock (subdev);
	  ++subdev->ref_count;
	  simple_unlock (&dev_hash_lock);

	  device_deallocate (dev);
	  dev = subdev;
	}
      else
	{
	  simple_unlock (&dev_hash_lock);

	  subdev = dev_open_alloc ();
	  if (!subdev)
	    {
	      device_deallocate (dev);
	      return KERN_RESOURCE_SHORTAGE;
	    }

	  mode = dev->mode;
	  subdev->com.blkpart.blk = dev; /* consumes our ref */
	  subdev->com.blkpart.part = part;
	  dev = subdev;

	  dev->com_device = (void *) part;
	  dev->mode = mode;
	  dev->ops = &block_partition_device_ops;

	  simple_lock (&dev_hash_lock);
	  dev_hash_enter (dev);
	  simple_unlock (&dev_hash_lock);

	  setup_no_senders (dev);
	}
    }

  *devp = dev;
  return D_SUCCESS;
}

/* Create a special pseudo-device that maps contiguous physical memory
   starting with the physical memory backing the wired kernel virtual
   address KVA.  See ds_mem.c for the device ops.  */
static io_return_t
special_mem_device (device_t *loc, vm_offset_t kva,
		    vm_size_t size, vm_size_t recsize,
		    device_t *out_dev)
{
  if (*loc == DEVICE_NULL)
    {
      device_t dev = dev_open_alloc ();
      if (dev == DEVICE_NULL)
	return KERN_RESOURCE_SHORTAGE;
      dev->mode = D_READ;
      dev->com_device = 0;
      dev->com.mem.pa = pmap_extract (kernel_pmap, kva);
      dev->com.mem.size = size;
      dev->com.mem.recsize = recsize;
      dev->ops = &mem_device_ops;
      *loc = dev;
    }

  device_reference (*loc);
  *out_dev = *loc;
  return D_SUCCESS;
}


io_return_t
ds_device_open (ipc_port_t open_port, ipc_port_t reply_port,
		mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		char *name, device_t *devp)
{
  char constructed[200];
  oskit_error_t rc;
  oskit_device_t *com_device;
  const char *subpart = 0;

  /* Open must be called on the master device port.  */
  if (open_port != master_device_port)
    INVALOP;

  /* There must be a reply port.  */
  if (! IP_VALID (reply_port))
    return MIG_NO_REPLY;

  if (name[0] == '@')		/* Bus tree location.  */
    {
      rc = oskit_bus_walk_lookup (name, &com_device, &subpart);
      if (OSKIT_FAILED (rc))
	return oskit_to_mach_error (rc);
    }
  else if (!strcmp (name, "console")) /* Special case.  */
    {
      com_device = (oskit_device_t *) ds_console_stream; /* not a device */
      oskit_device_addref (com_device);

      /* Kludge.  We don't do this at bootup so that the printing of any
	 kernel lossage or panics will be easier to read.  But we need it
	 set for real use of the console device.  It's harmless to call
	 this more than once, since it just sets a variable.  */
      direct_cons_set_flags (DC_NO_ONLCR);
    }
  else if (!strcmp (name, "kmsg")) /* Special case.  */
    {
      com_device = (oskit_device_t *) kmsg_stream; /* not a device */
      oskit_device_addref (com_device);
    }
  else if (!strcmp (name, "time")) /* Special case.  */
    {
      extern time_value_t *mtime;
      static device_t mapped_time_device;
      if (mode & D_WRITE)	/* No writing of "time" device allowed;  */
	return D_READ_ONLY;	/* users must do host_set_time instead.  */
      return special_mem_device (&mapped_time_device,
				 (vm_offset_t) mtime,
				 sizeof *mtime, sizeof mtime->seconds,
				 devp);
    }
  else if (!strcmp (name, "mem")) /* Special case.  */
    {
      static device_t phys_mem_device;
      return special_mem_device (&phys_mem_device,
				 phystokv (0),
				 /* We bogusly claim the granularity is
				    four bytes when in fact we can do one,
				    so that the size stays well below
				    overflowing the signed dev_status_t.  */
				 ~(vm_offset_t)0 - 4, 1,
				 devp);
    }
  else if (bus_walk_from_compat_string (name, constructed)) /* compat hack */
    {
      /* The name was recognized as a compatibility syntax, and CONSTRUCTED
	 now contains a name in bus walk syntax (starting with an '@').  */
      rc = oskit_bus_walk_lookup (constructed, &com_device, &subpart);
      if (OSKIT_FAILED (rc))
	return oskit_to_mach_error (rc);
    }
  else				/* I got no clue.  */
    return D_NO_SUCH_DEVICE;

  return dev_open_com (com_device, mode & (D_READ | D_WRITE), devp, subpart);
}

io_return_t
ds_device_close (device_t dev)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;

  /* The device_close RPC never does anything.
     The device gets closed when the last send right to its port dies.  */

  return D_SUCCESS;
}



/* Server functions for RPCs to open devices.

   Note: These functions expect a migworthy return value from the dev->ops
   functions, i.e. MIG_NO_REPLY and not D_IO_QUEUED.
*/

io_return_t
ds_device_write (device_t dev, ipc_port_t reply_port,
		 mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		 recnum_t recnum, io_buf_ptr_t data, unsigned int count,
		 int *bytes_written)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (!(dev->mode & D_WRITE))
    INVALOP;
  if (! data)
    INVALSZ;
  if (! dev->ops->write)
    {
      if (!dev->ops->write_inband)
	return D_READ_ONLY;
      else
	{
	  /*
	   * Copy out-of-line data into kernel address space.
	   * Since data is copied as page list, it will be
	   * accessible.
	   */
	  vm_offset_t addr;
	  kern_return_t kr = vm_map_copyout (device_io_map, &addr,
					     (vm_map_copy_t) data);
	  if (kr == KERN_SUCCESS)
	    {
	      /* Note: we assume that write_inband can take a large count.  */
	      kr = (*dev->ops->write_inband) (dev, reply_port,
					      reply_port_type, mode, recnum,
					      (char *) addr, count,
					      bytes_written);
	      (void) vm_deallocate (device_io_map, addr, count);
	    }
	  return kr;
	}
    }
  return (*dev->ops->write) (dev, reply_port,
			     reply_port_type, mode, recnum,
			     data, count, bytes_written);
}

io_return_t
ds_device_write_inband (device_t dev, ipc_port_t reply_port,
			mach_msg_type_name_t reply_port_type,
			dev_mode_t mode, recnum_t recnum,
			io_buf_ptr_inband_t data, unsigned count,
			int *bytes_written)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (!(dev->mode & D_WRITE))
    INVALOP;
  if (! data)
    INVALSZ;
  assert (count <= IO_INBAND_MAX);
  if (! dev->ops->write_inband)
    {
      if (! dev->ops->write)
	INVALOP;
      else
	{
	  vm_offset_t addr;
	  kern_return_t kr;
	  vm_map_copy_t copy;

	  assert (IO_INBAND_MAX <= PAGE_SIZE);
	  kr = kmem_alloc (kernel_map, &addr, PAGE_SIZE);
	  if (kr != KERN_SUCCESS)
	    return kr;

	  memcpy ((char *) addr, data, count);
	  kr = vm_map_copyin_page_list (kernel_map, addr, PAGE_SIZE,
					TRUE, TRUE, &copy, FALSE);
	  assert (kr == KERN_SUCCESS);

	  kr = (*dev->ops->write) (dev, reply_port,
				   reply_port_type, mode, recnum,
				   (char *) copy, count, bytes_written);
	  if (kr != D_SUCCESS)
	    vm_map_copy_discard (copy);
	  return kr;
	}
    }
  return (*dev->ops->write_inband) (dev, reply_port,
				    reply_port_type, mode, recnum,
				    data, count, bytes_written);
}

io_return_t
ds_device_read (device_t dev, ipc_port_t reply_port,
		mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		recnum_t recnum, int count, io_buf_ptr_t *data,
		unsigned *bytes_read)
{
  io_return_t err;

  /* There must be a reply port.  */
  if (! IP_VALID (reply_port))
    return MIG_NO_REPLY;
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (!(dev->mode & D_READ))
    INVALOP;

  err = (dev->ops->read ? (*dev->ops->read) (dev, reply_port,
					     reply_port_type, mode, recnum,
					     count, data, bytes_read)
	 : D_INVALID_OPERATION);
  while (err == D_INVALID_OPERATION) asm volatile ("int $3");

  /* The ops->read function can return D_INVALID_OPERATION to tell us that
     it doesn't want to deal with making a copy object for this read.  We
     fall back to the generic code below using the inband read function.  */
  if (err != D_INVALID_OPERATION)
    return err;

  if (! dev->ops->read_inband)
    INVALOP;
  else
    {
      vm_offset_t addr;
      vm_size_t size;
      kern_return_t kr;

      size = round_page (count);
      kr = kmem_alloc (kernel_map, &addr, size);
      if (kr != KERN_SUCCESS)
	return kr;
      kr = (*dev->ops->read_inband) (dev, reply_port,
				     reply_port_type, mode, recnum,
				     count, (char *) addr, bytes_read);
  while (kr == D_INVALID_OPERATION) asm volatile ("int $3");
      if (kr == D_SUCCESS)
	{
	  vm_size_t result_size = round_page (*bytes_read);

	  /* Free any excess pages we allocated beyond what was used.  */
	  size -= result_size;
	  if (size > 0)
	    vm_deallocate (kernel_map, addr + result_size, size);

	  /* Zero memory that the device did not fill.  */
	  memset ((char *) addr + *bytes_read, 0,
		  result_size - *bytes_read);

	  kr = vm_map_copyin_page_list (kernel_map, addr, result_size,
					TRUE, TRUE, (vm_map_copy_t *) data,
					FALSE);
	  if (kr != KERN_SUCCESS)
	    panic ("vm_map_copyin_page_list failed");
	}
      else
	{
	  /* Deallocate the pages since the read failed completely.  */
	  vm_deallocate (kernel_map, addr, size);
	}
      return kr;
    }
}

io_return_t
ds_device_read_inband (device_t dev, ipc_port_t reply_port,
		       mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		       recnum_t recnum, int count, char *data,
		       unsigned *bytes_read)
{
  /* There must be a reply port.  */
  if (! IP_VALID (reply_port))
    return MIG_NO_REPLY;
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (!(dev->mode & D_READ))
    INVALOP;
  if (count > IO_INBAND_MAX)
    count = IO_INBAND_MAX;

  if (dev->ops->read_inband)
    return (*dev->ops->read_inband) (dev, reply_port,
				     reply_port_type, mode, recnum,
				     count, data, bytes_read);
  if (dev->ops->read)
    {
      vm_map_copy_t copy;
      io_return_t err = (*dev->ops->read) (dev, reply_port,
					   reply_port_type, mode, recnum,
					   count,
					   (io_buf_ptr_t *) &copy,
					   bytes_read);
      if (!err)
	{
	  vm_offset_t addr;
	  err = vm_map_copyout (kernel_map, &addr, copy);
	  assert (!err);
	  memcpy (data, (char *) addr, *bytes_read);
	  vm_deallocate (kernel_map, addr, round_page (*bytes_read));
	}
      return err;
    }

  INVALOP;
}

io_return_t
ds_device_set_status (device_t dev, dev_flavor_t flavor,
		      dev_status_t status, mach_msg_type_number_t status_count)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (! dev->ops->set_status)
    INVALOP;

  return (*dev->ops->set_status) (dev, flavor, status,
				       status_count);
}

io_return_t
ds_device_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		      mach_msg_type_number_t *status_count)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (! dev->ops->get_status)
    INVALOP;

  return (*dev->ops->get_status) (dev, flavor, status,
				       status_count);
}

io_return_t
ds_device_set_filter (device_t dev, ipc_port_t receive_port, int priority,
		      filter_t *filter, unsigned filter_count)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (! dev->ops->set_filter)
    INVALOP;
  return (*dev->ops->set_filter) (dev, receive_port,
				       priority, filter, filter_count);
}

io_return_t
ds_device_map (device_t dev, vm_prot_t prot, vm_offset_t offset,
	       vm_size_t size, ipc_port_t *pager, boolean_t unmap)
{
  kern_return_t kr;

  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (! dev->ops->map)
    INVALOP;

  if (dev->mode != (D_READ | ((prot & VM_PROT_WRITE) ? D_WRITE : 0)))
    INVALOP;

  kr = (*dev->ops->map) (dev, prot, offset, size, 0);
  if (!kr)
    kr = device_pager_setup (dev, prot, offset, size, pager);
  return kr;
}


io_return_t
ds_device_write_trap (device_t dev, dev_mode_t mode,
		      recnum_t recnum, vm_offset_t data, vm_size_t count)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (!(dev->mode & D_WRITE))
    return D_INVALID_OPERATION;
  if (! dev->ops->write_trap)
    return D_INVALID_OPERATION;
  return (*dev->ops->write_trap) (dev, mode, recnum, data, count);
}

io_return_t
ds_device_writev_trap (device_t dev, dev_mode_t mode,
		       recnum_t recnum, io_buf_vec_t *iovec, vm_size_t count)
{
  if (dev == DEVICE_NULL)
    return D_NO_SUCH_DEVICE;
  if (!(dev->mode & D_WRITE))
    return D_INVALID_OPERATION;
  if (! dev->ops->writev_trap)
    return D_INVALID_OPERATION;
  return (*dev->ops->writev_trap) (dev, mode, recnum, iovec, count);
}
