#include "ds_oskit.h"

#include <device/net_io.h>
#include <device/if_ether.h>

#include <stddef.h>
#include <string.h>
#include <oskit/dev/net.h>
#include <oskit/dev/ethernet.h>


io_return_t
ds_net_get_status (device_t dev, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *status_count)
{
  oskit_error_t rc;
  oskit_etherdev_t *eth;

  rc = oskit_device_query (dev->com_device, &oskit_etherdev_iid,
			   (void **) &eth);
  if (OSKIT_FAILED (rc))
    return D_INVALID_OPERATION;	/* XXX we only support ethernet devices */

  switch (flavor)
    {
    case NET_ADDRESS:
      {
#define WORDS ((OSKIT_ETHERDEV_ADDR_SIZE + sizeof (int) - 1) / sizeof (int))

	if (*status_count < WORDS)
	  {
	    oskit_etherdev_release (eth);
	    return D_INVALID_OPERATION;
	  }
	*status_count = WORDS;

	bzero (status, OSKIT_ETHERDEV_ADDR_SIZE);
	oskit_etherdev_getaddr (eth, (void *) status);
	oskit_etherdev_release (eth);

	status[0] = ntohl (status[0]);
	status[1] = ntohl (status[1]);

	return D_SUCCESS;
      }

    case NET_STATUS:
      {
	struct net_status *ns = (void *) status;

	oskit_etherdev_release (eth);

	if (*status_count < NET_STATUS_COUNT)
	  return D_INVALID_OPERATION;
	*status_count = NET_STATUS_COUNT;

	ns->header_format = HDR_ETHERNET;
	ns->min_packet_size = ns->header_size = sizeof (struct ether_header);
	ns->max_packet_size = sizeof (struct ether_header) + ETHERMTU;
	ns->address_size = OSKIT_ETHERDEV_ADDR_SIZE;
	ns->flags = IFF_UP|IFF_RUNNING|IFF_BROADCAST;
	ns->mapped_size = 0;

	return D_SUCCESS;
      }
    }

  oskit_etherdev_release (eth);
  return D_INVALID_OPERATION;
}

io_return_t
ds_net_set_filter (device_t dev, ipc_port_t port, int priority,
		   filter_t *filter, unsigned filter_count)
{
  return net_set_filter (&dev->com.net.ifnet,
			 port, priority, filter, filter_count);
}


#if 0
io_return_t
ds_net_write (device_t dev, ipc_port_t reply_port,
	      mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	      recnum_t recnum, io_buf_ptr_t data, unsigned int count,
	      int *bytes_written)
{
  oskit_error_t rc;
  io_return_t err;
  oskit_u32_t wrote;
  vm_offset_t addr;
  oskit_bufio_t *bio;

  if (count < sizeof (struct ether_header) ||
      count > sizeof (struct ether_header) + ETHERMTU)
    return D_INVALID_SIZE;

  err = vm_map_copyout (device_io_map, &addr, (vm_map_copy_t) data);
  if (err != KERN_SUCCESS)
    return err;


}
#endif

/* XXX
   The oskit network drivers want to call our bufio object from interrupt
   handlers.  This makes it impractical for us to do anything fancy in
   there, like map the VM on demand--we can't do that from interrupt level!
   So we rely on the outbound netio_push to see bufio_map fail and copy out
   the data without attempting to save a reference to the bufio COM object.  */

struct bufio_impl
{
  oskit_bufio_t ioi;
  char *buf;
  oskit_size_t size;
};

static OSKIT_COMDECL
bufio_query(oskit_bufio_t *io, const oskit_iid_t *iid, void **out_ihandle)
{ panic(__FUNCTION__); }

static OSKIT_COMDECL_U
bufio_addref(oskit_bufio_t *io)
{ panic(__FUNCTION__); }

static OSKIT_COMDECL_U
bufio_release(oskit_bufio_t *io)
{ panic(__FUNCTION__); }

static OSKIT_COMDECL_U
bufio_getblocksize(oskit_bufio_t *io)
{
  return 1;
}

static OSKIT_COMDECL
bufio_read(oskit_bufio_t *io, void *dest, oskit_off_t offset,
	   oskit_size_t count, oskit_size_t *out_actual)
{
  struct bufio_impl *b = (struct bufio_impl *) io;

  if (offset >= b->size)
    return OSKIT_EINVAL;
  if (offset + count > b->size)
    count = b->size - offset;

  memcpy (dest, b->buf + offset, count);
  *out_actual = count;
  return 0;
}

static OSKIT_COMDECL
bufio_write(oskit_bufio_t *io, const void *src, oskit_off_t offset,
	    oskit_size_t count, oskit_size_t *out_actual)
{ panic(__FUNCTION__); }

static OSKIT_COMDECL
bufio_getsize(oskit_bufio_t *io, oskit_off_t *out_size)
{
  struct bufio_impl *b = (struct bufio_impl *) io;

  *out_size = b->size;
  return 0;
}

static OSKIT_COMDECL
bufio_setsize(oskit_bufio_t *io, oskit_off_t size)
{ return OSKIT_E_NOTIMPL; }


static OSKIT_COMDECL
bufio_map(oskit_bufio_t *io, void **out_addr,
	  oskit_off_t offset, oskit_size_t count)
{ return OSKIT_E_NOTIMPL; }

static OSKIT_COMDECL
bufio_unmap(oskit_bufio_t *io, void *addr, oskit_off_t offset, oskit_size_t count)
{ return OSKIT_E_NOTIMPL; }

static OSKIT_COMDECL
bufio_wire(oskit_bufio_t *io, oskit_addr_t *out_physaddr,
	   oskit_off_t offset, oskit_size_t count)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
bufio_unwire(oskit_bufio_t *io, oskit_addr_t phys_addr,
	     oskit_off_t offset, oskit_size_t count)
{
	return OSKIT_E_NOTIMPL;
}

static OSKIT_COMDECL
bufio_copy(oskit_bufio_t *io, oskit_off_t offset, oskit_size_t count,
	   oskit_bufio_t **out_io)
{
	return OSKIT_E_NOTIMPL;
}


static struct oskit_bufio_ops bio_ops = {
	bufio_query, bufio_addref, bufio_release,
	bufio_getblocksize,
	bufio_read, bufio_write,
	bufio_getsize, bufio_setsize,
	bufio_map, bufio_unmap,
	bufio_wire, bufio_unwire,
	bufio_copy
};


io_return_t
ds_net_write_inband (device_t dev, ipc_port_t reply_port,
		     mach_msg_type_name_t reply_port_type, dev_mode_t mode,
		     recnum_t recnum, io_buf_ptr_t data, unsigned int count,
		     int *bytes_written)
{
  oskit_error_t rc;
  struct bufio_impl bio = { { &bio_ops }, data, count };

  if (count < sizeof (struct ether_header) ||
      count > sizeof (struct ether_header) + ETHERMTU)
    return D_INVALID_SIZE;

  rc = oskit_netio_push (dev->com.net.sendi, &bio.ioi, count);
  *bytes_written = count;

  return oskit_to_mach_error (rc);
}


const struct device_ops net_device_ops =
{
  write_inband: ds_net_write_inband,
  set_filter: ds_net_set_filter,
  get_status: ds_net_get_status,
};


static device_t
recv_netio_device (oskit_netio_t *recvi)
{
  return (device_t) ((char *) recvi - offsetof (struct device, com.net.recvi));
}

static OSKIT_COMDECL
net_query(oskit_netio_t *io, const oskit_iid_t *iid, void **out_ihandle)
{
  device_t dev = recv_netio_device (io);

  if (memcmp(iid, &oskit_iunknown_iid, sizeof(*iid)) == 0 ||
      memcmp(iid, &oskit_netio_iid, sizeof(*iid)) == 0) {
    *out_ihandle = &dev->com.net.recvi;
    device_reference (dev);
    return 0;
  }

  *out_ihandle = 0;
  return OSKIT_E_NOINTERFACE;
}

static OSKIT_COMDECL_U
net_addref(oskit_netio_t *io)
{
  device_t dev = recv_netio_device (io);
  device_reference (dev);
  return dev->ref_count;
}

static OSKIT_COMDECL_U
net_release(oskit_netio_t *io)
{
  device_t dev = recv_netio_device (io);
  oskit_u32_t n = dev->ref_count - 1;
  device_deallocate (dev);
  return n;
}

static OSKIT_COMDECL
net_push(oskit_netio_t *ioi, oskit_bufio_t *b, oskit_size_t pkt_size)
{
  device_t dev = recv_netio_device (ioi);
  ipc_kmsg_t kmsg;
  struct ether_header *eh;
  struct packet_header *ph;
  oskit_error_t rc;
  oskit_size_t n;

  /* Allocate a kernel message buffer.  */
  kmsg = net_kmsg_get ();
  if (!kmsg)
    /* No buffer, drop packet.  */
    return 0;

  /* Copy packet into message buffer.  */
  eh = (struct ether_header *) (net_kmsg (kmsg)->header);
  ph = (struct packet_header *) (net_kmsg (kmsg)->packet);

  rc = oskit_bufio_read (b, eh, 0, sizeof (struct ether_header), &n);
  if (rc || n != sizeof (struct ether_header))
    panic("oskit_bufio_read: %x\n", rc);
  rc = oskit_bufio_read (b, ph + 1, sizeof (struct ether_header),
			 pkt_size - sizeof (struct ether_header), &n);
  if (rc || n != pkt_size - sizeof (struct ether_header))
    panic("oskit_bufio_read: %x\n", rc);

  ph->type = eh->ether_type;
  ph->length = (sizeof (struct packet_header)
		+ pkt_size - sizeof (struct ether_header));

  /* Pass packet up to the microkernel.  */
  net_packet (&dev->com.net.ifnet, kmsg, ph->length, ethernet_priority (kmsg));

  return 0;
}

static struct oskit_netio_ops recv_netio_ops =
{ net_query, net_addref, net_release, net_push };


oskit_error_t
ds_netdev_open (device_t dev, oskit_netdev_t *netdev)
{
  oskit_error_t rc;
  oskit_etherdev_t *eth;
  struct ifnet *const ifp = &dev->com.net.ifnet;

  if_init_queues (ifp);

  dev->com.net.recvi.ops = &recv_netio_ops;

  rc = oskit_netdev_open (netdev, 0, &dev->com.net.recvi, &dev->com.net.sendi);

  return rc;
}
