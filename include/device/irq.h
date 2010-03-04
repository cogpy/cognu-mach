#ifndef __IRQ_H__

#define __IRQ_H__

#include <device/device_types.h>

boolean_t	 deliver_irq (int irq, ipc_port_t dest);

typedef struct
{
  mach_msg_header_t irq_header;
  mach_msg_type_t   irq_type;
  int		    irq;
} mach_irq_notification_t;

#define IRQ_NOTIFY_MSGH_SEQNO 0
#define MACH_NOTIFY_IRQ 100

#endif
