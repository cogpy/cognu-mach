#ifndef	_DS_OSKIT_H_
#define	_DS_OSKIT_H_

#include <device/device_types.h>
#include <device/net_status.h>
#include <device/if_hdr.h>
#include "device_interface.h"
#include "ds_routines.h"

#include <mach/port.h>
#include <mach/message.h>
#include <kern/lock.h>
#include <kern/queue.h>

#include <cpus.h>

struct device_ops {
  io_return_t (*write) (device_t, ipc_port_t, mach_msg_type_name_t,
			dev_mode_t, recnum_t, io_buf_ptr_t, unsigned, int *);
  io_return_t (*write_inband) (device_t, ipc_port_t, mach_msg_type_name_t,
			       dev_mode_t, recnum_t, io_buf_ptr_inband_t,
			       unsigned, int *);
  io_return_t (*read) (device_t, ipc_port_t, mach_msg_type_name_t,
		       dev_mode_t, recnum_t, int, io_buf_ptr_t *, unsigned *);
  io_return_t (*read_inband) (device_t, ipc_port_t, mach_msg_type_name_t,
			      dev_mode_t, recnum_t, int, char *, unsigned *);
  io_return_t (*set_status) (device_t, dev_flavor_t, dev_status_t,
			     mach_msg_type_number_t);
  io_return_t (*get_status) (device_t, dev_flavor_t, dev_status_t,
			     mach_msg_type_number_t *);
  io_return_t (*set_filter) (device_t, ipc_port_t, int, filter_t [], unsigned);

  io_return_t (*write_trap) (device_t, dev_mode_t,
			     recnum_t, vm_offset_t, vm_size_t);
  io_return_t (*writev_trap) (device_t, dev_mode_t,
			      recnum_t, io_buf_vec_t *, vm_size_t);

  /* Called with PA == 0 to check if mapping is allowed,
     and with PA != 0 to get an actual physical address.  */
  io_return_t (*map) (device_t, vm_prot_t, vm_offset_t, vm_size_t,
		      oskit_addr_t *pa);

  void (*close) (device_t);
};


#include <oskit/dev/device.h>
#include <oskit/io/blkio.h>
#include <oskit/diskpart/diskpart.h>
#include <oskit/com/stream.h>
#include <oskit/io/asyncio.h>
#include <oskit/io/netio.h>

struct device {
  const struct device_ops *ops;

  decl_simple_lock_data(,ref_lock) /* lock for reference count */
  int ref_count;		/* reference count */
  decl_simple_lock_data(, lock) /* lock for rest of state */

  struct ipc_port *port;	/* open port */
  dev_mode_t mode;		/* D_READ and/or D_WRITE */

  /* This COM object is the generic handle on the device.  We never use
     this object after device_open, but its pointer serves as our unique
     identifier for the device so we can detect a second open.  To be sure
     the pointer remains unique, we keep the COM object alive as long as
     this device port lives.  */
  oskit_device_t *com_device;
  queue_chain_t hash_chain;

  union {
    struct {
      oskit_blkio_t *io;
      oskit_u32_t size;		/* block size */
#define MAX_PARTS 30
      diskpart_t *parts;
    } blk;
    struct {
      device_t blk;		/* underlying device_t, which is blk type */
      diskpart_t *part;		/* which partition this is */
    } blkpart;
    struct {
      oskit_stream_t *io;
      oskit_asyncio_t *aio;
      oskit_s32_t listening;	/* OSKIT_ASYNCIO_* */
      queue_head_t read_queue, write_queue; /* queued requests */
      queue_chain_t ready_queue; /* when on device_ready_queue */
      oskit_listener_t listener; /* my life as a COM object */
    } stream;
    struct {
      char *contents;
      oskit_size_t size;
    } bus;
    struct {
      vm_offset_t pa;
      vm_size_t size, recsize;
    } mem;
    struct {
      oskit_netio_t *sendi;
      oskit_netio_t recvi;	/* my life as a COM object: incoming packets */
      struct ifnet ifnet;	/* cruft for net_io.c */
    } net;
  } com;
};

/*
 * To lock and unlock state and open-count
 */
#define	device_lock(device)	simple_lock(&(device)->lock)
#define	device_unlock(device)	simple_unlock(&(device)->lock)

/* These macros are used to take a global lock around entering
   any oskit driver code.  */
#if ! MULTIPROCESSOR
#else
# warning SMP support in oskit-mach is incomplete
#endif
#define DEV_LOCK_INIT	((void)0)
#define DEV_LOCK(dev)	((void)0)
#define DEV_UNLOCK(dev)	((void)0)

extern const struct device_ops stream_device_ops;
extern const struct device_ops asyncio_device_ops;
extern const struct device_ops block_device_ops;
extern const struct device_ops block_partition_device_ops;
extern const struct device_ops net_device_ops;
extern const struct device_ops mem_device_ops;
extern const struct device_ops bus_device_ops;


/* #define INVALOP while (1) asm volatile ("int $3") */
/* #define INVALSZ while (1) asm volatile ("int $3") */
#define INVALOP return D_INVALID_OPERATION
#define INVALSZ return D_INVALID_SIZE
/*#define INVALREC return D_INVALID_RECNUM*/
#define INVALREC ({dump_stack_trace();panic("invalid record");})


#include <oskit/dev/error.h>

static inline io_return_t
oskit_to_mach_error (oskit_error_t rc)
{
  switch (rc)
    {
    case 0:				return D_SUCCESS;
    case OSKIT_E_DEV_NOSUCH_DEV:	return D_NO_SUCH_DEVICE;
    case OSKIT_E_DEV_NOSUCH_CHILD:	return D_NO_SUCH_DEVICE;
    case OSKIT_E_DEV_NOMORE_CHILDREN:	return D_NO_SUCH_DEVICE;
    case OSKIT_E_DEV_BADOP:		INVALOP;
    case OSKIT_E_DEV_BADPARAM:		INVALOP;
    case OSKIT_E_OUTOFMEMORY:		return D_NO_MEMORY;
    case OSKIT_EWOULDBLOCK:		return D_WOULD_BLOCK;
    default:
      assert (OSKIT_FAILED (rc));
      return D_IO_ERROR;
    }
}


#include <oskit/dev/osenv.h>

extern oskit_osenv_t *mach_osenv;
extern oskit_stream_t *ds_console_stream;
extern oskit_stream_t *kmsg_stream;
extern unsigned int kmsg_readers;


#define splio	spltty		/* XXX */
#define SPLIO	SPLTTY		/* XXX */

#define	sploskit splio
#define	SPLOSKIT SPLIO


extern zone_t io_inband_zone; /* for inband reads */


#endif
