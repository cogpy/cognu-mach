/*
 * VM Object Verification and Consistency Checks Header
 * Copyright (c) 2024 GNU Project.
 */

#ifndef _VM_VM_OBJECT_VERIFY_H_
#define _VM_VM_OBJECT_VERIFY_H_

#include <mach/kern_return.h>
#include <mach/boolean.h>
#include <vm/vm_types.h>

/* Forward declarations */
struct vm_object;
typedef struct vm_object *vm_object_t;

/*
 * VM Object Memory Statistics Structure
 */
typedef struct {
    unsigned long resident_pages;    /* Total resident pages */
    unsigned long wired_pages;       /* Wired (non-pageable) pages */
    unsigned long active_pages;      /* Pages in active queue */
    unsigned long inactive_pages;    /* Pages in inactive queue */
    unsigned long dirty_pages;       /* Modified pages */
    unsigned long referenced_pages;  /* Recently accessed pages */
    vm_size_t memory_size;          /* Total memory in bytes */
} vm_object_memory_stats_t;

/*
 * Verification Functions
 */
extern boolean_t vm_object_verify_resident_count(vm_object_t object);
extern void vm_object_increment_resident_count(vm_object_t object);
extern void vm_object_decrement_resident_count(vm_object_t object);
extern kern_return_t vm_object_get_memory_stats(vm_object_t object, 
                                               vm_object_memory_stats_t *stats);
extern void vm_object_verify_all_counts(void);

#endif /* _VM_VM_OBJECT_VERIFY_H_ */