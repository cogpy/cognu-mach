/* device server routines for disk partitions within oskit block devices
 */

#include "ds_oskit.h"

#include <oskit/io/blkio.h>
#include <oskit/diskpart/diskpart.h>

static io_return_t
adjust_recnum (device_t dev, recnum_t *recnum, unsigned int count)
{
  recnum_t countrecs = count / dev->com.blkpart.blk->com.blk.size;
  if (*recnum + countrecs > dev->com.blkpart.part->size)
    INVALREC;
  *recnum += dev->com.blkpart.part->start;
  return 0;
}

io_return_t
ds_blkpart_write (device_t dev, ipc_port_t reply_port,
		  mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		  recnum_t recnum, io_buf_ptr_t data, unsigned int count,
		  int *bytes_written)
{
  return (adjust_recnum (dev, &recnum, count) ?:
	  ds_device_write (dev->com.blkpart.blk,
			   reply_port, reply_port_type,
			   mode, recnum, data, count, bytes_written));
}

io_return_t
ds_blkpart_write_inband (device_t dev, ipc_port_t reply_port,
			 mach_msg_type_name_t reply_port_type, dev_mode_t mode,
			 recnum_t recnum,
			 io_buf_ptr_t data, unsigned int count,
			 int *bytes_written)
{
  return (adjust_recnum (dev, &recnum, count) ?:
	  ds_device_write_inband (dev->com.blkpart.blk,
				  reply_port, reply_port_type,
				  mode, recnum, data, count, bytes_written));
}


io_return_t
ds_blkpart_read (device_t dev, ipc_port_t reply_port,
		 mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		 recnum_t recnum, int count, io_buf_ptr_t *data,
		 unsigned *bytes_read)
{
  return (adjust_recnum (dev, &recnum, count) ?:
	  ds_device_read (dev->com.blkpart.blk,
			  reply_port, reply_port_type,
			  mode, recnum, count, data, bytes_read));
}

io_return_t
ds_blkpart_read_inband (device_t dev, ipc_port_t reply_port,
			mach_msg_type_name_t reply_port_type, dev_mode_t mode,
			recnum_t recnum, int count, char *data,
			unsigned *bytes_read)
{
  return (adjust_recnum (dev, &recnum, count) ?:
	  ds_device_read_inband (dev->com.blkpart.blk,
				 reply_port, reply_port_type,
				 mode, recnum, count, data, bytes_read));
}

io_return_t
ds_blkpart_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		       mach_msg_type_number_t *status_count)
{
  switch (flavor)
    {
    case DEV_GET_SIZE:
      {
 	status[DEV_GET_SIZE_RECORD_SIZE] = dev->com.blkpart.blk->com.blk.size;
	status[DEV_GET_SIZE_DEVICE_SIZE] = (dev->com.blkpart.blk->com.blk.size
					    * dev->com.blkpart.part->size);
	*status_count = 2;
	return D_SUCCESS;
      }
    }
  INVALOP;
}

void
ds_blkpart_close (device_t dev)
{
  /* We store the partition pointer instead of a COM object here, so we'd
     better clear this field before returning to device_deallocate, which
     will attempt to call the COM release method.  */
  dev->com_device = 0;

  /* Release our ref on the underlying device.  */
  device_deallocate (dev->com.blkpart.blk);
}

const struct device_ops block_partition_device_ops =
{
  write_inband: ds_blkpart_write_inband, write: ds_blkpart_write,
  read_inband: ds_blkpart_read_inband, read: ds_blkpart_read,
  get_status: ds_blkpart_get_status,
  close: ds_blkpart_close
};
