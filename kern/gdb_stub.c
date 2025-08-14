/*
 * Modern GDB stub implementation for GNU Mach - Basic Implementation
 * Copyright (C) 2024 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include <gdb_stub.h>
#include <kern/printf.h>
#include <i386/i386/kttd_machdep.h>

/* Global GDB stub state */
static struct gdb_stub_config gdb_config = GDB_STUB_DEFAULT_CONFIG;
static gdb_state_t gdb_state = GDB_STATE_DISCONNECTED;
static struct gdb_stub_stats gdb_stats = {0};

/*
 * Initialize the GDB stub subsystem
 * This enhances the existing TTD infrastructure with modern GDB support
 */
void gdb_stub_init(void)
{
    printf("[GDB] Modern GDB stub initializing...\n");
    
    /* Initialize based on existing TTD infrastructure */
    gdb_config.enabled = FALSE;  /* Start disabled */
    gdb_state = GDB_STATE_DISCONNECTED;
    
    /* Check hardware capabilities */
    gdb_config.hardware_breakpoints = gdb_stub_hw_breakpoint_available();
    
    printf("[GDB] Hardware breakpoints: %s\n", 
           gdb_config.hardware_breakpoints ? "available" : "not available");
    printf("[GDB] GDB stub initialization complete\n");
}

/*
 * Configure GDB stub behavior
 */
void gdb_stub_configure(const struct gdb_stub_config *config)
{
    if (!config) {
        printf("[GDB] Error: NULL configuration\n");
        return;
    }
    
    gdb_config = *config;
    printf("[GDB] Configuration updated:\n");
    printf("  - Hardware breakpoints: %s\n", 
           config->hardware_breakpoints ? "enabled" : "disabled");
    printf("  - Software breakpoints: %s\n", 
           config->software_breakpoints ? "enabled" : "disabled");
    printf("  - Watchpoints: %s\n", 
           config->watchpoints ? "enabled" : "disabled");
    printf("  - Thread-aware: %s\n", 
           config->thread_aware ? "enabled" : "disabled");
}

/*
 * Enable/disable GDB stub
 */
void gdb_stub_enable(boolean_t enable)
{
    gdb_config.enabled = enable;
    if (enable) {
        gdb_state = GDB_STATE_CONNECTED;
        printf("[GDB] GDB stub enabled - waiting for connection\n");
    } else {
        gdb_state = GDB_STATE_DISCONNECTED;
        printf("[GDB] GDB stub disabled\n");
    }
}

/*
 * Check if GDB stub is enabled
 */
boolean_t gdb_stub_is_enabled(void)
{
    return gdb_config.enabled;
}

/*
 * Handle GDB stub communication
 * This is the main entry point from trap handlers
 */
void gdb_stub_handle_exception(int exception_type, 
                              struct i386_saved_state *state)
{
    if (!gdb_config.enabled) {
        return;
    }
    
    gdb_stats.exceptions_handled++;
    
    printf("[GDB] Exception %d handled, EIP=0x%lx\n",
           exception_type, state->eip);
    
    /* In a full implementation, this would:
     * 1. Convert saved state to GDB register format
     * 2. Send stop reply to GDB
     * 3. Enter command processing loop
     * 4. Handle continue/step commands
     * 5. Update saved state from GDB register changes
     */
    
    gdb_state = GDB_STATE_STOPPED;
}

/*
 * Hardware breakpoint support check
 */
boolean_t gdb_stub_hw_breakpoint_available(void)
{
    /* Check for x86 debug register support */
    /* In a full implementation, would check CPUID and DR register availability */
    return TRUE;  /* Assume available on modern x86 */
}

/*
 * Set a hardware breakpoint
 */
boolean_t gdb_stub_set_hw_breakpoint(vm_offset_t address, 
                                    gdb_breakpoint_type_t type)
{
    if (!gdb_config.hardware_breakpoints) {
        return FALSE;
    }
    
    printf("[GDB] Setting hardware breakpoint at 0x%x, type %d\n", 
           address, type);
    
    /* In a full implementation, would:
     * 1. Find available debug register (DR0-DR3)
     * 2. Set DRn to address
     * 3. Configure DR7 for breakpoint type and length
     * 4. Track breakpoint in internal table
     */
    
    return TRUE;
}

/*
 * Remove a hardware breakpoint
 */
boolean_t gdb_stub_remove_hw_breakpoint(vm_offset_t address)
{
    if (!gdb_config.hardware_breakpoints) {
        return FALSE;
    }
    
    printf("[GDB] Removing hardware breakpoint at 0x%x\n", address);
    
    /* In a full implementation, would:
     * 1. Find debug register containing this address
     * 2. Clear DRn and corresponding DR7 bits
     * 3. Update internal breakpoint table
     */
    
    return TRUE;
}

/*
 * Set a breakpoint (software or hardware)
 */
boolean_t gdb_stub_set_breakpoint(gdb_breakpoint_type_t type,
                                 vm_offset_t address,
                                 vm_size_t length)
{
    if (!gdb_config.enabled) {
        return FALSE;
    }
    
    switch (type) {
    case GDB_BP_SOFTWARE:
        if (gdb_config.software_breakpoints) {
            printf("[GDB] Setting software breakpoint at 0x%x\n", address);
            /* Replace instruction with INT3 (0xCC) */
            return TRUE;
        }
        break;
        
    case GDB_BP_HARDWARE:
        return gdb_stub_set_hw_breakpoint(address, type);
        
    case GDB_BP_WRITE_WATCH:
    case GDB_BP_READ_WATCH:
    case GDB_BP_ACCESS_WATCH:
        if (gdb_config.watchpoints) {
            printf("[GDB] Setting watchpoint at 0x%x, type %d, length %zu\n", 
                   address, type, length);
            return gdb_stub_set_hw_breakpoint(address, type);
        }
        break;
    }
    
    return FALSE;
}

/*
 * Remove a breakpoint
 */
boolean_t gdb_stub_remove_breakpoint(gdb_breakpoint_type_t type,
                                    vm_offset_t address,
                                    vm_size_t length)
{
    if (!gdb_config.enabled) {
        return FALSE;
    }
    
    printf("[GDB] Removing breakpoint at 0x%x, type %d\n", address, type);
    
    switch (type) {
    case GDB_BP_SOFTWARE:
        /* Restore original instruction */
        return TRUE;
        
    case GDB_BP_HARDWARE:
    case GDB_BP_WRITE_WATCH:
    case GDB_BP_READ_WATCH:
    case GDB_BP_ACCESS_WATCH:
        return gdb_stub_remove_hw_breakpoint(address);
    }
    
    return FALSE;
}

/*
 * Thread debugging notifications
 */
void gdb_stub_thread_create(thread_t thread)
{
    if (gdb_config.thread_aware && gdb_config.enabled) {
        printf("[GDB] Thread created: %p\n", thread);
    }
}

void gdb_stub_thread_destroy(thread_t thread)
{
    if (gdb_config.thread_aware && gdb_config.enabled) {
        printf("[GDB] Thread destroyed: %p\n", thread);
    }
}

void gdb_stub_thread_switch(thread_t old_thread, thread_t new_thread)
{
    if (gdb_config.thread_aware && gdb_config.enabled) {
        printf("[GDB] Thread switch: %p -> %p\n", old_thread, new_thread);
    }
}

/*
 * Get debugging statistics
 */
void gdb_stub_get_stats(struct gdb_stub_stats *stats)
{
    if (stats) {
        *stats = gdb_stats;
    }
}

/*
 * Reset debugging statistics
 */
void gdb_stub_reset_stats(void)
{
    gdb_stats.packets_sent = 0;
    gdb_stats.packets_received = 0;
    gdb_stats.exceptions_handled = 0;
    gdb_stats.breakpoints_hit = 0;
    gdb_stats.commands_processed = 0;
    gdb_stats.errors = 0;
}

/*
 * Send signal to GDB (for panics, etc.)
 */
void gdb_stub_send_signal(int signal)
{
    if (gdb_config.enabled) {
        printf("[GDB] Sending signal %d to debugger\n", signal);
        gdb_state = GDB_STATE_STOPPED;
    }
}

/*
 * Check if debugger should break
 */
boolean_t gdb_stub_should_break(void)
{
    return (gdb_config.enabled && gdb_state == GDB_STATE_STOPPED);
}

/*
 * Memory validation for GDB access
 */
boolean_t gdb_stub_memory_valid(vm_offset_t address, vm_size_t length)
{
    /* In a full implementation, would check:
     * 1. Address range validity
     * 2. Memory protection
     * 3. Page presence
     */
    return TRUE;  /* Simplified for now */
}

/*
 * Memory change notification
 */
void gdb_stub_memory_changed(vm_offset_t address, vm_size_t length)
{
    if (gdb_config.enabled) {
        printf("[GDB] Memory changed at 0x%x, length %zu\n", address, length);
    }
}

/* Modern GDB protocol extensions (stubs for future implementation) */
void gdb_stub_send_thread_info(void)
{
    printf("[GDB] Sending thread information\n");
}

void gdb_stub_send_register_info(void)
{
    printf("[GDB] Sending register information\n");
}

void gdb_stub_send_memory_map(void)
{
    printf("[GDB] Sending memory map\n");
}