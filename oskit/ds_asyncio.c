/* In keeping with Mach's old chario behavior, we just ignore RECNUM.  */

#include <stddef.h>

#include <machine/spl.h>
#include <mach/mig_errors.h>

#include "ds_oskit.h"
#include "ds_request.h"


static struct oskit_listener_ops listener_ops; /* forward decl */


static void
queue_request (device_t dev, struct pending_request *req,
	       oskit_s32_t rw, queue_t queue)
{
  spl_t s;

  s = splio ();
  simple_lock (&device_ready_queue_lock); /* locks all request queues! */

  queue_enter (queue, req, struct pending_request *, chain);

  simple_unlock (&device_ready_queue_lock);
  splx (s);

  /* The driver's asyncio interface is responsible for being interrupt-safe. */
  if ((dev->com.stream.listening & rw) == 0)
    {
      oskit_s32_t mask;

      if (dev->com.stream.listening != 0)
	/* There is an old listener installed for just the other direction,
	   but now we are interested in both directions.  */
	oskit_asyncio_remove_listener (dev->com.stream.aio,
				       &dev->com.stream.listener);

      dev->com.stream.listener.ops = &listener_ops;
      dev->com.stream.listening |= rw;
      mask = oskit_asyncio_add_listener (dev->com.stream.aio,
					 &dev->com.stream.listener,
					 dev->com.stream.listening);
      if (OSKIT_FAILED (mask))
	panic ("asyncio_add_listener: %x", mask);
      if (mask & rw)
	ds_device_ready (dev);
    }
}
#define requeue_read_request(dev, req) \
  queue_request(dev, req, OSKIT_ASYNCIO_READABLE, &dev->com.stream.read_queue)
#define requeue_write_request(dev, req) \
  queue_request(dev, req, OSKIT_ASYNCIO_WRITABLE, &dev->com.stream.write_queue)


static io_return_t
new_request (device_t dev, oskit_s32_t rw, queue_t queue,
	     void (*completer) (device_t, struct pending_request *),
	     ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     oskit_size_t count, oskit_size_t offset,
	     union device_data u)
{
  oskit_s32_t mask;
  struct pending_request *req;
  int s;

  req = request_allocate ();
  if (!req)
    return D_NO_MEMORY;

  req->completer = completer;
  req->reply_port = reply_port;
  req->reply_port_type = reply_port_type;
  req->count = count;
  req->offset = offset;
  req->data = u;

  queue_request (dev, req, rw, queue);

  /* We add a reference to the device for each live request.
     The reference lives as long as the request structure, even
     if it is momentarily removed and later requeued.  */
  device_reference (dev);

  return 0;
}

static void
request_done (device_t dev, struct pending_request *req)
{
  request_free (req);
  device_deallocate (dev);
}

static io_return_t
new_request_inband_buf (device_t dev, oskit_s32_t rw, queue_t queue,
			void (*completer_inband) (device_t,
						  struct pending_request *),
			void (*completer_small) (device_t,
						 struct pending_request *),
			ipc_port_t reply_port,
			mach_msg_type_name_t reply_port_type,
			oskit_size_t count, oskit_size_t offset,
			const char *data, oskit_size_t copycnt)
{
  union device_data u;
  io_return_t err;

  if (count <= IO_SMALL_MAX)
    {
      unsigned int i;
      for (i = 0; i < copycnt; ++i)
	u.small[i] = data[i];
      err = new_request (dev, rw, queue, completer_small,
			 reply_port, reply_port_type,
			 count, offset, u);
    }
  else
    {
      u.inbando = zalloc (io_inband_zone);
      if (u.inbando == 0)
	return D_NO_MEMORY;
      assert (count <= IO_INBAND_MAX);
      memcpy (u.inband, data, copycnt);
      err = new_request (dev, rw, queue, completer_inband,
			 reply_port, reply_port_type,
			 count, offset, u);
      if (err)
	zfree (io_inband_zone, u.inbando);
    }

  return err;
}


/* Ascertain if this request needs a reply message.  Returns nonzero iff
   there is a live reply port.  If there is no reply port or the reply port
   has died, returns zero after cleaning up the reply port.  */
static int
need_reply (struct pending_request *req)
{
  if (!IP_VALID (req->reply_port))
    return 0;

  ip_lock (req->reply_port);
  if (ip_active (req->reply_port))
    {
      ip_unlock (req->reply_port);
      return 1;
    }

  ip_release (req->reply_port);
  ip_check_unlock (req->reply_port);
  return 0;
}


static int
ds_asyncio_complete_read_inband_1 (device_t dev, struct pending_request *req,
				   char *data)
{
  inline void error (oskit_error_t rc)
    {
      if (req->offset == 0)
	ds_device_read_inband_error_reply (req->reply_port,
					   req->reply_port_type,
					   oskit_to_mach_error (rc));
      else
	ds_device_read_reply_inband (req->reply_port,
				     req->reply_port_type,
				     0, data, req->offset);
    }

  if (need_reply (req))
    {
      oskit_s32_t n = oskit_asyncio_readable (dev->com.stream.aio);
      if (OSKIT_FAILED (n))
	error (n);
      else
	{
	  oskit_error_t rc;
	  oskit_u32_t nread;

	  if (n > req->count - req->offset)
	    n = req->count - req->offset;

	  rc = oskit_stream_read (dev->com.stream.io,
				  &data[req->offset], n, &nread);
	  if (rc == OSKIT_EWOULDBLOCK)
	    {
	      requeue_read_request (dev, req);
	      return 0;
	    }

	  if (rc)
	    error (rc);
	  else
	    {
	      req->offset += nread;
#if 0				/* XXX */
	      if (req->offset < req->count)
		{
		  requeue_read_request (dev, req);
		  return 0;
		}
#endif

	      ds_device_read_reply_inband (req->reply_port,
					   req->reply_port_type,
					   0, data, req->offset);
	    }
	}
    }

  request_done (dev, req);
  return 1;
}

static void
ds_asyncio_complete_read_inband_small (device_t dev,
				       struct pending_request *req)
{
  ds_asyncio_complete_read_inband_1 (dev, req, req->data.small);
}

static void
ds_asyncio_complete_read_inband (device_t dev, struct pending_request *req)
{
  vm_offset_t ptr = req->data.inbando;
  if (ds_asyncio_complete_read_inband_1 (dev, req, req->data.inband))
    zfree (io_inband_zone, ptr);
}

io_return_t
ds_asyncio_read_inband (device_t dev, ipc_port_t reply_port,
			mach_msg_type_name_t reply_port_type, dev_mode_t mode,
			recnum_t recnum, int count, char *data,
			unsigned *bytes_read)
{
  oskit_error_t rc;
  io_return_t err;
  oskit_s32_t n;

  n = oskit_asyncio_readable (dev->com.stream.aio);
  if (OSKIT_FAILED (n))
    return oskit_to_mach_error (n);
  else if (n > 0)
    {
      if (n > count)
	n = count;
      rc = oskit_stream_read (dev->com.stream.io, data, n, bytes_read);
      if (OSKIT_FAILED (rc) && (rc != OSKIT_EWOULDBLOCK /*|| (mode & D_NOWAIT)*/))
	return oskit_to_mach_error (rc);
if(rc==0)
      if (mode & D_NOWAIT)	/* return just what we got */
	return D_SUCCESS;
    }
  else
    {
#if 0
      if (mode & D_NOWAIT)	/* Old Mach chario doesn't do this.  */
	return D_WOULD_BLOCK;
#endif

      *bytes_read = 0;
    }

  err = new_request_inband_buf (dev, OSKIT_ASYNCIO_READABLE,
				&dev->com.stream.read_queue,
				ds_asyncio_complete_read_inband,
				ds_asyncio_complete_read_inband_small,
				reply_port, reply_port_type,
				count, *bytes_read, data, *bytes_read);

  return err ?: MIG_NO_REPLY;
}



static int
ds_asyncio_complete_write_inband_1 (device_t dev,
				    struct pending_request *req, char *data)
{
  oskit_u32_t n, wrote;
  oskit_error_t rc;

  inline void error (oskit_error_t rc)
    {
      if (req->offset == 0)
	ds_device_write_inband_error_reply (req->reply_port,
					    req->reply_port_type,
					    oskit_to_mach_error (rc));
      else
	ds_device_write_reply_inband (req->reply_port,
				      req->reply_port_type,
				      0, req->offset);
    }

  rc = oskit_stream_write (dev->com.stream.io, &data[req->offset],
			   req->count - req->offset, &wrote);
  if (rc == OSKIT_EWOULDBLOCK)
    {
      requeue_write_request (dev, req);
      return 0;
    }

  if (rc)
    error (rc);
  else
    {
      req->offset += wrote;
      if (req->offset < req->count)
	{
	  requeue_write_request (dev, req);
	  return 0;
	}

      ds_device_write_reply_inband (req->reply_port,
				    req->reply_port_type,
				    0, req->offset);
    }

  request_done (dev, req);
  return 1;
}

static void
ds_asyncio_complete_write_inband_small (device_t dev,
					struct pending_request *req)
{
  ds_asyncio_complete_write_inband_1 (dev, req, req->data.small);
}

static void
ds_asyncio_complete_write_inband (device_t dev, struct pending_request *req)
{
  vm_offset_t ptr = req->data.inbando;
  if (ds_asyncio_complete_write_inband_1 (dev, req, req->data.inband))
    zfree (io_inband_zone, ptr);
}

io_return_t
ds_asyncio_write_inband (device_t dev, ipc_port_t reply_port,
			 mach_msg_type_name_t reply_port_type, dev_mode_t mode,
			 recnum_t recnum,
			 io_buf_ptr_t data, unsigned int count,
			 int *bytes_written)
{
  oskit_error_t rc;
  io_return_t err;
  oskit_u32_t wrote;
  union device_data u;

  rc = oskit_stream_write (dev->com.stream.io, data, count, &wrote);
  if (rc && (rc != OSKIT_EWOULDBLOCK || (mode & D_NOWAIT)))
    return oskit_to_mach_error (rc);
  if (rc == 0)
    {
      if (wrote == count || (mode & D_NOWAIT))
	{
	  *bytes_written = wrote;
	  return D_SUCCESS;
	}

      data += wrote;
      count -= wrote;
    }

  err = new_request_inband_buf (dev, OSKIT_ASYNCIO_WRITABLE,
				&dev->com.stream.write_queue,
				ds_asyncio_complete_write_inband,
				ds_asyncio_complete_write_inband_small,
				reply_port, reply_port_type,
				count, 0, data, count);

  return err ?: MIG_NO_REPLY;
}

/* Kludge just for kmsg.  */
void
ds_asyncio_close (device_t dev)
{
  if ((void *) dev->com_device == kmsg_stream && (dev->mode & D_READ))
    --kmsg_readers;
}

const struct device_ops asyncio_device_ops =
{
  write_inband: ds_asyncio_write_inband,
  read_inband: ds_asyncio_read_inband,
  close: ds_asyncio_close
};


static device_t
listener_device (oskit_listener_t *listener)
{
  return (device_t) ((char *) listener
		     - offsetof (struct device, com.stream.listener));
}

static OSKIT_COMDECL
listener_query(oskit_listener_t *io, const oskit_iid_t *iid,
	       void **out_ihandle)
{
  device_t dev = listener_device (io);

  if (memcmp (iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
      memcmp (iid, &oskit_listener_iid, sizeof(*iid)) == 0)
    {
      *out_ihandle = &dev->com.stream.listener;
      device_reference (dev);
      return 0;
    }

  *out_ihandle = NULL;
  return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
listener_addref(oskit_listener_t *io)
{
  device_t dev = listener_device (io);
  device_reference (dev);
  return dev->ref_count;
}

static OSKIT_COMDECL_U
listener_release(oskit_listener_t *io)
{
  device_t dev = listener_device (io);
  oskit_u32_t n = dev->ref_count - 1;
  device_deallocate (dev);
  return n;
}

static OSKIT_COMDECL
listener_notify (oskit_listener_t *io, oskit_iunknown_t *obj)
{
  device_t dev = listener_device (io);

  ds_device_ready (dev);
}

static struct oskit_listener_ops listener_ops =
{ listener_query, listener_addref, listener_release, listener_notify };



static struct pending_request *
dequeue_request (device_t dev, queue_t queue)
{
  struct pending_request *req;

  spl_t s = splio ();
  simple_lock (&device_ready_queue_lock); /* locks all request queues! */

  if (queue_empty (queue))
    req = 0;
  else
    queue_remove_first (queue, req, struct pending_request *, chain);

  simple_unlock (&device_ready_queue_lock);
  splx (s);

  return req;
}


/* This gets called (at spl0) by the io_done_thread when one of our
   devices was put on the device_ready_queue by a listener firing.  */

void
ds_asyncio_ready (device_t dev)
{
  oskit_s32_t avail;
  struct pending_request *req;

  do
    {
      avail = oskit_asyncio_poll (dev->com.stream.aio);
      if (OSKIT_FAILED (avail))
	panic ("oskit_asyncio_poll: %x", avail);
    poll_results:
      if (avail & OSKIT_ASYNCIO_READABLE)
	{
	  /* Pluck off a read request and call its completer.
	     This will dispatch the request and either free
	     or requeue the structure.  */
	  req = dequeue_request (dev, &dev->com.stream.read_queue);
	  if (req)
	    (*req->completer) (dev, req);
	  else
	    avail &= ~OSKIT_ASYNCIO_READABLE; /* ignore availability */
	}
      if (avail & OSKIT_ASYNCIO_WRITABLE)
	{
	  req = dequeue_request (dev, &dev->com.stream.write_queue);
	  if (req)
	    (*req->completer) (dev, req);
	  else
	    avail &= ~OSKIT_ASYNCIO_WRITABLE;
	}
    } while (avail & (OSKIT_ASYNCIO_READABLE|OSKIT_ASYNCIO_WRITABLE));

  avail = dev->com.stream.listening;
  if (queue_empty (&dev->com.stream.read_queue))
    avail &= ~OSKIT_ASYNCIO_READABLE;
  if (queue_empty (&dev->com.stream.write_queue))
    avail &= ~OSKIT_ASYNCIO_WRITABLE;
  if (avail != dev->com.stream.listening)
    {
      /* We don't need the same listener any more.  */
      oskit_asyncio_remove_listener (dev->com.stream.aio,
				     &dev->com.stream.listener);

      dev->com.stream.listening = avail;
      if (avail != 0)
	{
	  avail = oskit_asyncio_add_listener (dev->com.stream.aio,
					      &dev->com.stream.listener,
					      dev->com.stream.listening);
	  if (OSKIT_FAILED (avail))
	    panic ("asyncio_add_listener: %x", avail);
	  if (avail)
	    goto poll_results;
	}
    }
}
