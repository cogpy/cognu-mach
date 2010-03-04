#include <device/irq.h>
#include <device/ds_routines.h>

void
deliver_irq (int irq)
{
  ipc_kmsg_t kmsg;
  mach_irq_notification_t *n;
  ipc_port_t dest_port = intr_rcv_ports[irq];
  mach_port_t dest = (mach_port_t) dest_port;

  if (dest == MACH_PORT_NULL)
    return;

  kmsg = ikm_alloc(sizeof *n);
  if (kmsg == IKM_NULL) 
    return;

  ikm_init(kmsg, sizeof *n);
  n = (mach_irq_notification_t *) &kmsg->ikm_header;

  mach_msg_header_t *m = &n->irq_header;
  mach_msg_type_t *t = &n->irq_type;

  m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND, 0);
  m->msgh_size = sizeof *n;
  m->msgh_seqno = IRQ_NOTIFY_MSGH_SEQNO;
  m->msgh_local_port = MACH_PORT_NULL;
  m->msgh_remote_port = MACH_PORT_NULL;
  m->msgh_id = MACH_NOTIFY_IRQ;

  t->msgt_name = MACH_MSG_TYPE_INTEGER_32;
  t->msgt_size = 32;
  t->msgt_number = 1;
  t->msgt_inline = TRUE;
  t->msgt_longform = FALSE;
  t->msgt_deallocate = FALSE;
  t->msgt_unused = 0;

  n->irq_header.msgh_remote_port = dest;
  n->irq = irq;

  ipc_port_copy_send (dest_port);

  printf ("before delivering a irq\n");
  ipc_mqueue_send_always(kmsg);

}
