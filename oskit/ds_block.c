/* device server routines for oskit block devices
 *
 * Some code here stolen from linux/dev/glue/block.c by Shantanu Goel
 *
 * Copyright (C) 1996, 1999 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ds_oskit.h"

#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <oskit/io/blkio.h>


#define MAX_COPY	(VM_MAP_COPY_PAGE_LIST_MAX << PAGE_SHIFT)

#if 0
/* XXX no point in getting fancy since the oskit (linux/dev) code
   will allocate its own wired memory and copy the data we pass into it
   regardless.
 */
io_return_t
ds_blkio_write (device_t dev, ipc_port_t reply_port,
		mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		recnum_t recnum, io_buf_ptr_t data, unsigned int count,
		int *bytes_written)
{
  io_return_t err;
  oskit_error_t rc;
  vm_map_copy_t copy = (vm_map_copy_t) data;
  oskit_size_t resid, amt, i;
  vm_offset_t addr, uaddr;
  vm_size_t len, size;

  if (count % dev->com.blk.size)
    INVALSZ;

  resid = count;
  uaddr = copy->offset;

  /* Allocate a kernel buffer.  */
  size = round_page (uaddr + count) - trunc_page (uaddr);
  if (size > MAX_COPY)
    size = MAX_COPY;
  addr = vm_map_min (device_io_map);
  err = vm_map_enter (device_io_map, &addr, size, 0, TRUE,
		      0, 0, FALSE, VM_PROT_READ|VM_PROT_WRITE,
		      VM_PROT_READ|VM_PROT_WRITE, VM_INHERIT_NONE);
  if (err)
    {
      vm_map_copy_discard (copy);
      return err;
    }

  /* Determine size of I/O this time around.  */
  len = size - (uaddr & PAGE_MASK);
  if (len > resid)
    len = resid;

  offset = (oskit_off_t)recnum * dev->com.blk.size;

  do
    {
      /* Map user pages.  */
      for (i = 0; i < copy->cpy_npages; i++)
	pmap_enter (vm_map_pmap (device_io_map),
		    addr + (i << PAGE_SHIFT),
		    copy->cpy_page_list[i]->phys_addr,
		    VM_PROT_READ /* |VM_PROT_WRITE ? */, TRUE);

      /* Do the write.  */
      rc = oskit_blkio_write (dev->com.blk.io,
			      (const char *) addr + (uaddr & PAGE_MASK),
			      offset, len, &amt);

      /* Unmap pages and deallocate copy.  */
      pmap_remove (vm_map_pmap (device_io_map),
		   addr, addr + (copy->cpy_npages << PAGE_SHIFT));
      vm_map_copy_discard (copy);

      /* Check result of write.  */
      if (rc)
	{
	  err = oskit_to_mach_error (rc);
	  break;
	}
      else
	{
	  resid -= amt;
	  if (resid == 0)
	    break;
	  uaddr += amt;
	  offset += amt;
	}

      /* Determine size of I/O this time around and copy in pages.  */
      len = round_page (uaddr + resid) - trunc_page (uaddr);
      if (len > MAX_COPY)
	len = MAX_COPY;
      len -= uaddr & PAGE_MASK;
      if (len > resid)
	len = resid;
      err = vm_map_copyin_page_list (current_map (), uaddr, len,
				     FALSE, FALSE, &copy, FALSE);
    } while (!err);

  /* Delete kernel buffer.  */
  vm_map_remove (device_io_map, addr, addr + size);

  return err;
}
#endif

io_return_t
ds_blkio_write_inband (device_t dev, ipc_port_t reply_port,
		       mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		       recnum_t recnum, io_buf_ptr_t data, unsigned int count,
		       int *bytes_written)
{
  oskit_error_t rc;

  /* Note that IO_INBAND_MAX is 128, so it is impossible to pass this
     test for usual block sizes (>= 512) with any device_write_inband RPC.
     But this function gets called by the generic ds_routines.c code for
     out-of-band device_write too.  */

  if (count % dev->com.blk.size)
    INVALSZ;

  rc = oskit_blkio_write (dev->com.blk.io, data,
			  (oskit_off_t)recnum * dev->com.blk.size,
			  count, (oskit_size_t *) bytes_written);

  return oskit_to_mach_error (rc);
}


io_return_t
ds_blkio_read (device_t dev, ipc_port_t reply_port,
	       mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	       recnum_t recnum, int count, io_buf_ptr_t *data,
	       unsigned *bytes_read)
{
  io_return_t err;
  oskit_error_t rc;
  boolean_t dirty;
  oskit_size_t resid, amt;
  queue_head_t pages;
  vm_map_copy_t copy;
  vm_offset_t addr, offset, alloc_offset, o;
  vm_object_t object;
  vm_page_t m;
  vm_size_t len, size;
  oskit_off_t readloc;

  if (count % dev->com.blk.size)
    INVALSZ;

  *data = 0;
  *bytes_read = 0;

  /* Allocate an object to hold the data.  */
  size = round_page (count);
  object = vm_object_allocate (size);
  if (! object)
    return D_NO_MEMORY;

  alloc_offset = offset = 0;
  resid = count;

  /* Allocate a kernel buffer.  */
  addr = vm_map_min (device_io_map);
  if (size > MAX_COPY)
    size = MAX_COPY;
  err = vm_map_enter (device_io_map, &addr, size, 0, TRUE, 0,
		      0, FALSE, VM_PROT_READ|VM_PROT_WRITE,
		      VM_PROT_READ|VM_PROT_WRITE, VM_INHERIT_NONE);
  if (err)
    goto out;

  queue_init (&pages);

  readloc = (oskit_off_t)recnum * dev->com.blk.size;

  while (resid)
    {
      /* Determine size of I/O this time around.  */
      len = round_page (offset + resid) - trunc_page (offset);
      if (len > MAX_COPY)
	len = MAX_COPY;

      /* Map any pages left from previous operation.  */
      o = trunc_page (offset);
      queue_iterate (&pages, m, vm_page_t, pageq)
	{
	  pmap_enter (vm_map_pmap (device_io_map),
		      addr + o - trunc_page (offset),
		      m->phys_addr, VM_PROT_READ|VM_PROT_WRITE, TRUE);
	  o += PAGE_SIZE;
	}
      assert (o == alloc_offset);

      /* Allocate and map pages.  */
      while (alloc_offset < trunc_page (offset) + len)
	{
	  while ((m = vm_page_grab (FALSE)) == 0)
	    VM_PAGE_WAIT (0);
	  assert (! m->active && ! m->inactive);
	  m->busy = TRUE;
	  queue_enter (&pages, m, vm_page_t, pageq);
	  pmap_enter (vm_map_pmap (device_io_map),
		      addr + alloc_offset - trunc_page (offset),
		      m->phys_addr, VM_PROT_READ|VM_PROT_WRITE, TRUE);
	  alloc_offset += PAGE_SIZE;
	}

      /* Do the read.  */
      amt = len - (offset & PAGE_MASK);
      if (amt > resid)
	amt = resid;
      rc = oskit_blkio_read (dev->com.blk.io,
			     (char *) addr + (offset & PAGE_MASK),
			     readloc, amt, &amt);

      /* Compute number of pages to insert in object.  */
      o = trunc_page (offset);
      if (rc == 0)
	{
	  dirty = TRUE;
	  resid -= amt;
	  if (resid == 0)
	    {
	      /* Zero any unused space.  */
	      if (offset + amt < o + len)
		memset ((void *) (addr + offset - o + amt),
			0, o + len - offset - amt);
	      offset = o + len;
	    }
	  else
	    offset += amt;
	}
      else
	{
	  dirty = FALSE;
	  offset = o + len;
	}

      /* Unmap pages and add them to the object.  */
      pmap_remove (vm_map_pmap (device_io_map), addr, addr + len);
      vm_object_lock (object);
      while (o < trunc_page (offset))
	{
	  m = (vm_page_t) queue_first (&pages);
	  assert (! queue_end (&pages, (queue_entry_t) m));
	  queue_remove (&pages, m, vm_page_t, pageq);
	  assert (m->busy);
	  vm_page_lock_queues ();
	  if (dirty)
	    {
	      PAGE_WAKEUP_DONE (m);
	      m->dirty = TRUE;
	      vm_page_insert (m, object, o);
	    }
	  else
	    vm_page_free (m);
	  vm_page_unlock_queues ();
	  o += PAGE_SIZE;
	}
      vm_object_unlock (object);

      if (rc)
	{
	  err = oskit_to_mach_error (rc);
	  break;
	}
      else if (amt == 0)	/* XXX ? */
	break;
      else
	readloc += amt;
    }

  /* Delete kernel buffer.  */
  vm_map_remove (device_io_map, addr, addr + size);

  assert (queue_empty (&pages));

out:
  if (! err)
    err = vm_map_copyin_object (object, 0, round_page (count), &copy);
  if (! err)
    {
      *data = (io_buf_ptr_t) copy;
      *bytes_read = count - resid;
    }
  else
    vm_object_deallocate (object);

  return err;
}

io_return_t
ds_blkio_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		     mach_msg_type_number_t *status_count)
{
  oskit_error_t rc;
  switch (flavor)
    {
    case DEV_GET_SIZE:
      {
	oskit_off_t size;

	/* DEV->com.blk.size was initialized on open.  */
 	status[DEV_GET_SIZE_RECORD_SIZE] = dev->com.blk.size;

	rc = oskit_blkio_getsize (dev->com.blk.io, &size);
	if (rc == OSKIT_E_NOTIMPL)
	  status[DEV_GET_SIZE_DEVICE_SIZE] = 0;
	else if (rc)
	  return oskit_to_mach_error (rc);
	else
	  status[DEV_GET_SIZE_DEVICE_SIZE] = size;

	*status_count = 2;
	return D_SUCCESS;
      }
    case DEV_GET_RECORDS:
      {
	oskit_off_t size;

	/* DEV->com.blk.size was initialized on open.  */
 	status[DEV_GET_RECORDS_RECORD_SIZE] = dev->com.blk.size;
	assert (dev->com.blk.size > 0);

	rc = oskit_blkio_getsize (dev->com.blk.io, &size);
	if (rc == OSKIT_E_NOTIMPL)
	  size = 0;
	else if (rc)
	  return oskit_to_mach_error (rc);

	status[DEV_GET_RECORDS_DEVICE_RECORDS]
	  = (recnum_t) (size / (oskit_off_t) dev->com.blk.size);

	/* Always return DEV_GET_RECORDS_COUNT.  This is what all native
	   Mach drivers do, and makes it possible to detect the absence
	   of the call by setting it to a different value on input.  MiG
	   makes sure that we will never return more integers than the
	   user asked for.  */
	*status_count = DEV_GET_RECORDS_COUNT;
	return D_SUCCESS;
      }
    }
  INVALOP;
}

void
ds_blkio_close (device_t dev)
{
  /* Free any partition info we cached about this disk.  */
  if (dev->com.blk.parts)
    kfree ((vm_offset_t) dev->com.blk.parts,
	   MAX_PARTS * sizeof dev->com.blk.parts[0]);
}

const struct device_ops block_device_ops =
{
  write_inband: ds_blkio_write_inband,
  read: ds_blkio_read,
  get_status: ds_blkio_get_status,
  close: ds_blkio_close
};
