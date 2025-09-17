/*
 * Performance Monitoring Host Interface
 * Implementation of Mach RPC interface for performance analysis
 *
 * Copyright (C) 2024 Free Software Foundation, Inc.
 */

#include <kern/host.h>
#include <kern/perf_analysis.h>
#include <mach/perf_monitor.h>
#include <kern/kalloc.h>
#include <string.h>

/*
 * Enable or disable performance monitoring
 */
kern_return_t
perf_monitor_enable(host_t host, boolean_t enable)
{
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    return perf_monitor_enable(enable);
}

/*
 * Configure performance monitoring parameters
 */
kern_return_t
perf_monitor_configure(host_t host, uint32_t sample_rate, uint32_t buffer_size)
{
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    return perf_monitor_configure(sample_rate, buffer_size);
}

/*
 * Get performance statistics for a specific event type
 */
kern_return_t
perf_get_event_stats(host_t host, uint32_t event_type,
                    uint64_t *count, uint64_t *total_time_us,
                    uint64_t *min_time_us, uint64_t *max_time_us,
                    uint64_t *avg_time_us, uint64_t *last_timestamp)
{
    struct perf_event_stats stats;
    kern_return_t ret;
    
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    if (event_type >= PERF_EVENT_MAX) {
        return KERN_INVALID_ARGUMENT;
    }
    
    ret = perf_get_event_stats((perf_event_type_t)event_type, &stats);
    if (ret != KERN_SUCCESS) {
        return ret;
    }
    
    *count = stats.count;
    *total_time_us = stats.total_time_us;
    *min_time_us = stats.min_time_us;
    *max_time_us = stats.max_time_us;
    *avg_time_us = stats.avg_time_us;
    *last_timestamp = stats.last_timestamp;
    
    return KERN_SUCCESS;
}

/*
 * Get system-wide performance summary
 */
kern_return_t
perf_get_system_stats(host_t host, uint32_t *total_events,
                     uint32_t *samples_dropped, boolean_t *regression_detected,
                     uint64_t *overall_count, uint64_t *overall_total_time,
                     uint64_t *overall_min_time, uint64_t *overall_max_time,
                     uint64_t *overall_avg_time)
{
    struct perf_event_stats summary;
    kern_return_t ret;
    
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    ret = perf_get_system_stats(&summary, total_events);
    if (ret != KERN_SUCCESS) {
        return ret;
    }
    
    simple_lock(&global_perf_monitor.lock);
    *samples_dropped = global_perf_monitor.samples_dropped;
    *regression_detected = global_perf_monitor.regression_detected;
    simple_unlock(&global_perf_monitor.lock);
    
    *overall_count = summary.count;
    *overall_total_time = summary.total_time_us;
    *overall_min_time = summary.min_time_us;
    *overall_max_time = summary.max_time_us;
    *overall_avg_time = summary.avg_time_us;
    
    return KERN_SUCCESS;
}

/*
 * Read performance samples from buffer
 */
kern_return_t
perf_read_samples(host_t host, uint32_t max_samples,
                 uint8_t **samples, mach_msg_type_number_t *samples_count)
{
    struct perf_sample *sample_buffer;
    uint32_t samples_read, buffer_bytes;
    kern_return_t ret;
    
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    if (max_samples == 0 || max_samples > 1024) {
        max_samples = 1024;  /* Limit to reasonable size */
    }
    
    /* Allocate temporary buffer for samples */
    buffer_bytes = max_samples * sizeof(struct perf_sample);
    sample_buffer = (struct perf_sample*)kalloc(buffer_bytes);
    if (sample_buffer == NULL) {
        return KERN_RESOURCE_SHORTAGE;
    }
    
    /* Read samples from performance monitor */
    ret = perf_read_samples(sample_buffer, max_samples, &samples_read);
    if (ret != KERN_SUCCESS) {
        kfree(sample_buffer, buffer_bytes);
        return ret;
    }
    
    /* Return samples to user space */
    *samples = (uint8_t*)sample_buffer;
    *samples_count = samples_read * sizeof(struct perf_sample);
    
    return KERN_SUCCESS;
}

/*
 * Set performance baseline for regression detection
 */
kern_return_t
perf_set_baseline(host_t host)
{
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    return perf_set_baseline();
}

/*
 * Check for performance regressions
 */
kern_return_t
perf_check_regression(host_t host, uint32_t event_type, 
                     uint32_t threshold_percent, boolean_t *regression_detected)
{
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    if (event_type >= PERF_EVENT_MAX) {
        return KERN_INVALID_ARGUMENT;
    }
    
    *regression_detected = perf_check_regression((perf_event_type_t)event_type, 
                                                threshold_percent);
    
    return KERN_SUCCESS;
}

/*
 * Reset performance statistics
 */
kern_return_t
perf_reset_stats(host_t host)
{
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    perf_reset_stats();
    return KERN_SUCCESS;
}

/*
 * Set real-time monitoring thresholds
 */
kern_return_t
perf_set_thresholds(host_t host, uint32_t latency_us, 
                   uint32_t throughput, uint32_t error_rate)
{
    if (host == HOST_NULL) {
        return KERN_INVALID_HOST;
    }
    
    perf_monitor_set_thresholds(latency_us, throughput, error_rate);
    return KERN_SUCCESS;
}