#ifndef	_DEVICE_DEV_HDR_H_
#define	_DEVICE_DEV_HDR_H_

typedef struct device *device_t;
#define DEVICE_NULL	((device_t) 0)

void device_reference (device_t);
void device_deallocate (device_t device);

struct ipc_port;
device_t dev_port_lookup (struct ipc_port *);
struct ipc_port *convert_device_to_port (device_t device);


#endif	/* _DEVICE_DEV_HDR_H_ */
