#include "ds_oskit.h"

#include <oskit/machine/physmem.h>
#include <vm/vm_map.h>


/* For device_get_status, we look like a block device.  */
io_return_t
ds_mem_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *status_count)
{
  switch (flavor)
    {
    case DEV_GET_SIZE:
      status[DEV_GET_SIZE_RECORD_SIZE] = dev->com.mem.recsize;
      status[DEV_GET_SIZE_DEVICE_SIZE] = dev->com.mem.size;
      *status_count = 2;
      return D_SUCCESS;
    }
  INVALOP;
}

/* This is the main use for these pseudo-devices, to map physical pages
   into user tasks.  This is called from the device pager to get a physical
   page address that will be inserted into a task's page tables.  So we can
   safely use any physical address at all, regardless of whether it is
   mapped into the kernel address space.  */
io_return_t
ds_mem_map (device_t dev, vm_prot_t prot,
	    vm_offset_t offset, vm_size_t size, oskit_addr_t *pa)
{
  if (offset + size > round_page (dev->com.mem.size))
    INVALREC;
  if (offset % dev->com.mem.recsize || !page_aligned (offset))
    INVALREC;
  if (round_page (size) % dev->com.mem.recsize)
    INVALSZ;

  if (pa)
    *pa = dev->com.mem.pa + offset;
  return 0;
}


/* Real physical memory is already direct-mapped into the kernel address
   space, but i/o memory may not be.  To copy into or out of i/o memory
   addresses, we need to map pages into the kernel address space
   temporarily.  */
static int
direct_mapped (vm_offset_t pa)
{
  return (trunc_page (pa) >= round_page (phys_mem_min) &&
	  round_page (pa) <= trunc_page (phys_mem_max));
}

static int
map_phys (vm_offset_t pa, unsigned *count, vm_prot_t prot,
	  vm_offset_t *kva, vm_size_t *mapsz, vm_offset_t *ofs)
{
  vm_offset_t start = trunc_page (pa), end = round_page (pa + *count);
  kern_return_t kr;
  vm_offset_t addr;

  /* Find some address space to use.
     If we can't get it all, get a smaller amount.  */
  addr = vm_map_min (device_io_map);
  while ((kr = vm_map_enter (device_io_map, &addr, end - start, 0, TRUE,
			     0, 0, FALSE, prot, prot, VM_INHERIT_NONE))
	 == KERN_NO_SPACE)
    end = start + (end - start) / 2;
  if (kr != KERN_SUCCESS)
    return kr;

  *kva = addr;
  *ofs = pa - start;
  if (end < pa + *count)
    *count = end - pa;
  *mapsz = end - start;

  /* Now map those physical pages in.  */
  do
    {
      pmap_enter (vm_map_pmap (device_io_map), addr, start, prot, TRUE);
      start += PAGE_SIZE;
      addr += PAGE_SIZE;
    } while (start < end);

  return 0;
}

static void
unmap_phys (vm_offset_t addr, vm_size_t size)
{
  vm_map_remove (device_io_map, addr, addr + size);
}


io_return_t
ds_mem_read_inband (device_t dev, ipc_port_t reply_port,
		    mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		    recnum_t recnum, int count, char *data,
		    unsigned *bytes_read)
{
  if (count == 0)
    {
      *bytes_read = 0;
      return 0;
    }
  recnum *= dev->com.mem.recsize;
  if ((oskit_size_t) recnum + count > dev->com.mem.size)
    INVALREC;

  *bytes_read = count;
  if (direct_mapped (dev->com.mem.pa))
    memcpy (data, (char *) phystokv (dev->com.mem.pa) + recnum, count);
  else
    {
      vm_offset_t kva, ofs;
      vm_size_t mapsz;
      if (! map_phys (dev->com.mem.pa + recnum, bytes_read,
		      VM_PROT_READ,
		      &kva, &mapsz, &ofs))
	return D_NO_MEMORY;
      memcpy (data, (char *) kva + ofs, *bytes_read);
      unmap_phys (kva, mapsz);
    }

  return 0;
}

io_return_t
ds_mem_write_inband (device_t dev, ipc_port_t reply_port,
		     mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		     recnum_t recnum,
		     io_buf_ptr_t data, unsigned int count,
		     int *bytes_written)
{
  if (count == 0)
    {
      *bytes_written = 0;
      return 0;
    }
  recnum *= dev->com.mem.recsize;
  if ((oskit_size_t) recnum + count > dev->com.mem.size)
    INVALREC;

  *bytes_written = count;
  if (direct_mapped (dev->com.mem.pa))
    memcpy ((char *) phystokv (dev->com.mem.pa) + recnum, data, count);
  else
    {
      vm_offset_t kva, ofs;
      vm_size_t mapsz;
      if (! map_phys (dev->com.mem.pa + recnum,
		      (unsigned *) bytes_written,
		      VM_PROT_READ | VM_PROT_WRITE,
		      &kva, &mapsz, &ofs))
	return D_NO_MEMORY;
      memcpy ((char *) kva + ofs, data, *bytes_written);
      unmap_phys (kva, mapsz);
    }

  return 0;
}


const struct device_ops mem_device_ops =
{
  read_inband: ds_mem_read_inband,
  write_inband: ds_mem_write_inband,
  get_status: ds_mem_get_status,
  map: ds_mem_map,
};
