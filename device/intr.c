#include <device/intr.h>
#include <device/ds_routines.h>
#include <kern/queue.h>
#include <kern/printf.h>

#ifndef MACH_XEN
// TODO this is only for x86 system
#define sti() __asm__ __volatile__ ("sti": : :"memory")
#define cli() __asm__ __volatile__ ("cli": : :"memory")

static boolean_t deliver_intr (int line, ipc_port_t dest_port);

static queue_head_t intr_queue;
/* The total number of unprocessed interrupts. */
static int tot_num_intr;

static struct intr_entry *
search_intr (int line, ipc_port_t dest)
{
  struct intr_entry *e;
  queue_iterate (&intr_queue, e, struct intr_entry *, chain)
    {
      if (e->dest == dest && e->line == line)
	return e;
    }
  return NULL;
}

static struct intr_entry *
search_intr_line (int line)
{
  struct intr_entry *e;
  queue_iterate (&intr_queue, e, struct intr_entry *, chain)
    {
      if (e->line == line &&
	  (e->dest != MACH_PORT_NULL
	   && e->dest->ip_references != 1
	   && e->unacked_interrupts))
	return e;
    }
  return NULL;
}

kern_return_t user_intr_enable (int line, char status)
{
  struct intr_entry *e;
  kern_return_t ret = D_SUCCESS;

  cli();
  /* FIXME: Use search_intr instead once we get the delivery port from ds_device_intr_enable, and get rid of search_intr_line */
  e = search_intr_line (line);

  if (!e)
    printf("didn't find user intr for interrupt %d!?\n", line);
  else if (status)
  {
    if (!e->unacked_interrupts)
      ret = D_INVALID_OPERATION;
    else
      e->unacked_interrupts--;
  }
  else
  {
    e->unacked_interrupts++;
    if (!e->unacked_interrupts)
    {
      ret = D_INVALID_OPERATION;
      e->unacked_interrupts--;
    }
  }
  sti();

  if (ret)
    return ret;

  if (status)
    /* TODO: better name for generic-to-arch-specific call */
    __enable_irq (line);
  else
    __disable_irq (line);
  return D_SUCCESS;
}

/* This function can only be used in the interrupt handler. */
static void
queue_intr (int line, user_intr_t *e)
{
  /* Until userland has handled the IRQ in the driver, we have to keep it
   * disabled. Level-triggered interrupts would keep raising otherwise. */
  __disable_irq (line);

  cli ();
  e->unacked_interrupts++;
  e->interrupts++;
  tot_num_intr++;
  sti ();

  thread_wakeup ((event_t) &intr_thread);
}

int deliver_user_intr (int line, user_intr_t *intr)
{
  /* The reference of the port was increased
   * when the port was installed.
   * If the reference is 1, it means the port should
   * have been destroyed and I destroy it now. */
  if (intr->dest
      && intr->dest->ip_references == 1)
    {
      ipc_port_release (intr->dest);
      intr->dest = MACH_PORT_NULL;
      printf ("irq handler %d: release a dead delivery port\n", line);
      thread_wakeup ((event_t) &intr_thread);
      return 0;
    }
  else
    {
      queue_intr (line, intr);
      return 1;
    }
}

/* insert an interrupt entry in the queue.
 * This entry exists in the queue until
 * the corresponding interrupt port is removed.*/
user_intr_t *
insert_intr_entry (int line, ipc_port_t dest)
{
  struct intr_entry *e, *new, *ret;
  int free = 0;

  new = (struct intr_entry *) kalloc (sizeof (*new));
  if (new == NULL)
    return NULL;

  /* check whether the intr entry has been in the queue. */
  cli ();
  e = search_intr (line, dest);
  if (e)
    {
      printf ("the interrupt entry for line %d and port %p has already been inserted\n", line, dest);
      free = 1;
      ret = NULL;
      goto out;
    }
  ret = new;
  new->line = line;
  new->dest = dest;
  new->interrupts = 0;

  /* For now netdde calls device_intr_enable once after registration. Assume
   * it does so for now. When we move to IRQ acknowledgment convention we will
   * change this. */
  new->unacked_interrupts = 1;

  queue_enter (&intr_queue, new, struct intr_entry *, chain);
out:
  sti ();
  if (free)
    kfree ((vm_offset_t) new, sizeof (*new));
  return ret;
}

void
intr_thread (void)
{
  struct intr_entry *e;
  int line;
  ipc_port_t dest;
  queue_init (&intr_queue);
  
  for (;;)
    {
      assert_wait ((event_t) &intr_thread, FALSE);
      /* Make sure we wake up from times to times to check for aborted processes */
      thread_set_timeout (hz);
      cli ();

      /* Check for aborted processes */
      queue_iterate (&intr_queue, e, struct intr_entry *, chain)
	{
	  if ((!e->dest || e->dest->ip_references == 1) && e->unacked_interrupts)
	    {
	      printf ("irq handler %d: release dead delivery %d unacked irqs\n", e->line, e->unacked_interrupts);
	      /* The reference of the port was increased
	       * when the port was installed.
	       * If the reference is 1, it means the port should
	       * have been destroyed and I clear unacked irqs now, so the Linux
	       * handling can trigger, and we will cleanup later after the Linux
	       * handler is cleared. */
	      /* TODO: rather immediately remove from Linux handler */
	      while (e->unacked_interrupts)
	      {
		__enable_irq(e->line);
		e->unacked_interrupts--;
	      }
	    }
	}

      /* Now check for interrupts */
      while (tot_num_intr)
	{
	  int del = 0;

	  queue_iterate (&intr_queue, e, struct intr_entry *, chain)
	    {
	      /* if an entry doesn't have dest port,
	       * we should remove it. */
	      if (e->dest == MACH_PORT_NULL)
		{
		  clear_wait (current_thread (), 0, 0);
		  del = 1;
		  break;
		}

	      if (e->interrupts)
		{
		  clear_wait (current_thread (), 0, 0);
		  line = e->line;
		  dest = e->dest;
		  e->interrupts--;
		  tot_num_intr--;

		  sti ();
		  deliver_intr (line, dest);
		  cli ();
		}
	    }

	  /* remove the entry without dest port from the queue and free it. */
	  if (del)
	    {
	      assert (!queue_empty (&intr_queue));
	      queue_remove (&intr_queue, e, struct intr_entry *, chain);
	      if (e->unacked_interrupts)
		printf("irq handler %d: still %d unacked irqs\n", e->line, e->unacked_interrupts);
	      while (e->unacked_interrupts)
	      {
		__enable_irq(e->line);
		e->unacked_interrupts--;
	      }
	      printf("irq handler %d: removed\n");
	      sti ();
	      kfree ((vm_offset_t) e, sizeof (*e));
	      cli ();
	    }
	}
      sti ();
      thread_block (NULL);
    }
}

static boolean_t
deliver_intr (int line, ipc_port_t dest_port)
{
  ipc_kmsg_t kmsg;
  mach_intr_notification_t *n;
  mach_port_t dest = (mach_port_t) dest_port;

  if (dest == MACH_PORT_NULL)
    return FALSE;

  kmsg = ikm_alloc(sizeof *n);
  if (kmsg == IKM_NULL) 
    return FALSE;

  ikm_init(kmsg, sizeof *n);
  n = (mach_intr_notification_t *) &kmsg->ikm_header;

  mach_msg_header_t *m = &n->intr_header;
  mach_msg_type_t *t = &n->intr_type;

  m->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND, 0);
  m->msgh_size = sizeof *n;
  m->msgh_seqno = INTR_NOTIFY_MSGH_SEQNO;
  m->msgh_local_port = MACH_PORT_NULL;
  m->msgh_remote_port = MACH_PORT_NULL;
  m->msgh_id = MACH_INTR_NOTIFY;

  t->msgt_name = MACH_MSG_TYPE_INTEGER_32;
  t->msgt_size = 32;
  t->msgt_number = 1;
  t->msgt_inline = TRUE;
  t->msgt_longform = FALSE;
  t->msgt_deallocate = FALSE;
  t->msgt_unused = 0;

  n->intr_header.msgh_remote_port = dest;
  n->line = line;

  ipc_port_copy_send (dest_port);
  ipc_mqueue_send_always(kmsg);

  return TRUE;
}
#endif	/* MACH_XEN */
