/* device server routines for oskit stream devices.

   This is a very simple version really not fit for general use at all.  It
   blocks the thread making sending the device RPC *request* until the read
   or write completes.  This is only fit for kernel debugging when the
   "real" minimal console code is not available.  (I ain't written it yet!)
*/

#include "ds_oskit.h"


/* In keeping with Mach's old chario behavior, we just ignore RECNUM.  */

io_return_t
ds_stream_write_inband (device_t dev, ipc_port_t reply_port,
			mach_msg_type_name_t reply_port_type, dev_mode_t mode,
			recnum_t recnum, io_buf_ptr_t data, unsigned int count,
			int *bytes_written)
{
  oskit_error_t rc;

  *bytes_written = count;	/* oskit console is buggy */
  rc = oskit_stream_write (dev->com.stream.io, data, count, bytes_written);

  return oskit_to_mach_error (rc);
}

io_return_t
ds_stream_read_inband (device_t dev, ipc_port_t reply_port,
		       mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		       recnum_t recnum, int count, char *data,
		       unsigned *bytes_read)
{
  oskit_error_t rc;

  if (count == 0)
    {
      *bytes_read = 0;
      return D_SUCCESS;
    }

  /* XXX bad blocking behavior: user thread doing read_request_inband blocks
   */

  rc = oskit_stream_read (dev->com.stream.io, data, count, bytes_read);
  return oskit_to_mach_error (rc);
}

const struct device_ops stream_device_ops =
{
  write_inband: ds_stream_write_inband, read_inband: ds_stream_read_inband
};
