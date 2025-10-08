/* Simulate a context where stdint.h would be included first */
#include <stdint.h>
/* Now include our fixed header */
#include <mach/lttng.h>

int main() {
    uint64_t a = 123;
    uint32_t b = 456;  
    uint16_t c = 789;
    uint8_t d = 101;
    
    struct mach_trace_event event;
    event.timestamp_hi = b;
    event.timestamp_lo = b;
    
    return 0;
}
