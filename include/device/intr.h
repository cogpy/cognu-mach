#ifndef __INTR_H__

#define __INTR_H__

#include <device/device_types.h>
#include <kern/queue.h>

typedef struct
{
  mach_msg_header_t intr_header;
  mach_msg_type_t   intr_type;
  int		    line;
} mach_intr_notification_t;

typedef struct intr_entry
{
  queue_chain_t chain;
  ipc_port_t dest;
  int line;
  int interrupts;		/* The number of interrupts occur since last run of intr_thread. */
  int unacked_interrupts;	/* Number of times irqs were disabled for this */
} user_intr_t;

#define INTR_NOTIFY_MSGH_SEQNO 0
#define MACH_INTR_NOTIFY 100

int install_user_intr_handler (unsigned int line,
					unsigned long flags,
					user_intr_t *user_intr);

/* Returns 0 if action should be removed */
int deliver_user_intr (int line, user_intr_t *intr);

user_intr_t *insert_intr_entry (int line, ipc_port_t dest);

/* TODO: should rather take delivery port */
kern_return_t user_intr_enable (int line, char status);

void intr_thread (void);

void __disable_irq(unsigned int);
void __enable_irq(unsigned int);

#endif
