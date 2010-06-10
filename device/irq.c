#include <device/irq.h>
#include <device/ds_routines.h>
#include <kern/queue.h>
#include <kern/printf.h>

// TODO this is only for x86 system
#define sti() __asm__ __volatile__ ("sti": : :"memory")
#define cli() __asm__ __volatile__ ("cli": : :"memory")

struct intr_entry
{
  queue_chain_t chain;
  ipc_port_t dest;
  int irq;
};

queue_head_t intr_queue;


/* This function can only be used in the interrupt handler. */
void
queue_intr (int irq, ipc_port_t dest)
{
  extern void intr_thread ();
  struct intr_entry *e = (void *) kalloc (sizeof (*e));
  
  if (e == NULL)
    {
      printf ("queue_intr: no memory available\n");
      return;
    }

  e->irq = irq;
  e->dest = dest;
  
  cli ();
  queue_enter (&intr_queue, e, struct intr_entry *, chain);
  sti ();

  thread_wakeup ((event_t) &intr_thread);
}

struct intr_entry *
dequeue_intr ()
{
  struct intr_entry *e;

  cli ();
  if (queue_empty (&intr_queue))
    {
      sti ();
      return NULL;
    }

  queue_remove_first (&intr_queue, e, struct intr_entry *, chain);
  sti ();

  return e;
}

void
intr_thread ()
{
  queue_init (&intr_queue);
  
  for (;;)
    {
      struct intr_entry *e = dequeue_intr ();

      if (e == NULL)
	{
	  /* There aren't new interrupts,
	   * wait until someone wakes us up. */
	  assert_wait ((event_t) &intr_thread, FALSE);
	  thread_block (NULL);
	  continue;
	}
      
      deliver_irq (e->irq, e->dest);
      kfree ((vm_offset_t) e, sizeof (*e));
    }
}

boolean_t
deliver_irq (int irq, ipc_port_t dest_port)
{
  ipc_kmsg_t kmsg;
  mach_irq_notification_t *n;
  mach_port_t dest = (mach_port_t) dest_port;

  if (dest == MACH_PORT_NULL)
    return FALSE;

  kmsg = ikm_alloc(sizeof *n);
  if (kmsg == IKM_NULL) 
    return FALSE;

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
  ipc_mqueue_send_always(kmsg);

  return TRUE;
}
