# VDSO Implementation for GNU Mach

## Overview

The Virtual Dynamic Shared Object (VDSO) implementation provides a mechanism to optimize system call performance by mapping kernel code directly into user-space address space. This allows frequently used system calls to execute without the overhead of a traditional kernel trap.

## Architecture

### Core Components

#### 1. VDSO Header (`kern/vdso.h`)
- Defines VDSO data structures and API
- Declares symbol types and configuration constants
- Provides function prototypes for VDSO operations

#### 2. VDSO Implementation (`kern/vdso.c`)
- Main VDSO functionality including:
  - Memory allocation and management
  - Symbol table management
  - Task mapping/unmapping
  - Fast-path system call implementations

#### 3. Architecture Support (`i386/i386/vdso_arch.c`)
- i386-specific VDSO page setup
- Assembly stubs for fast system calls
- CPU feature detection and optimization

### VDSO Page Layout

```
+-------------------+
| VDSO Header       |
| - Magic number    |
| - Version info    |
| - Symbol table    |
+-------------------+
| gettimeofday()    |
| Assembly stub     |
+-------------------+
| clock_gettime()   |
| Assembly stub     |
+-------------------+
| time()            |
| Assembly stub     |
+-------------------+
| getpid()          |
| Assembly stub     |
+-------------------+
| (Unused space)    |
+-------------------+
```

## Supported System Calls

### Currently Implemented
1. **gettimeofday()** - Get current time with microsecond precision
2. **clock_gettime()** - POSIX clock interface 
3. **time()** - Simple time retrieval
4. **getpid()** - Process ID retrieval

### Implementation Status
- **Phase 1** (Current): Basic infrastructure and placeholder implementations
- **Phase 2** (Future): Integration with actual time sources
- **Phase 3** (Future): Performance optimization with CPU features

## Usage

### Kernel Integration

The VDSO is automatically initialized during kernel startup:

```c
void vdso_init(void);  // Called from kern/startup.c
```

### Process Integration

When a new process is created, the VDSO page is mapped into its address space:

```c
kern_return_t vdso_map_into_task(task_t task, vm_offset_t *address);
```

### User-Space Access

User programs can access VDSO functions through the ELF auxiliary vector or by directly calling functions at known offsets in the VDSO page.

## Configuration

### Build-Time Options

VDSO support is compiled into the kernel by default. The following constants can be adjusted:

```c
#define VDSO_PAGE_SIZE     PAGE_SIZE    // Size of VDSO mapping
#define VDSO_MAX_SYMBOLS   16           // Maximum number of symbols
#define VDSO_USER_BASE     0x7ffff000UL // Default user-space address
```

### Runtime Configuration

VDSO functionality can be queried at runtime:

```c
boolean_t vdso_is_available(void);
vdso_state_t *vdso_get_state(void);
```

## Performance Benefits

### Traditional System Call
1. User-space trap instruction (int 0x80)
2. Kernel mode switch
3. System call dispatch
4. Kernel function execution
5. Return to user-space
6. Mode switch overhead

### VDSO System Call
1. Direct function call in user-space
2. Access to shared kernel data
3. Fast return to caller
4. No mode switch required

**Expected Performance Improvement**: 2-10x faster for simple operations like time retrieval.

## Testing

### Basic Test Suite

The VDSO implementation includes a test suite (`tests/test-vdso.c`) that validates:

- VDSO initialization
- Symbol table setup
- Memory mapping functionality
- Basic function calls

### Running Tests

```bash
make run-vdso  # Run VDSO-specific tests
make check     # Run full test suite including VDSO
```

## Debugging

### Debug Output

VDSO initialization produces debug output during kernel boot:

```
VDSO: Initializing Virtual Dynamic Shared Object support
VDSO: i386 architecture initialization
VDSO: Setting up i386 VDSO page at 0x...
VDSO: i386 VDSO page setup complete, used ... bytes
VDSO: Initialization complete
```

### Inspection Tools

```c
// Get VDSO state for debugging
vdso_state_t *state = vdso_get_state();

// Look up symbol addresses
vm_offset_t addr = vdso_lookup_symbol(VDSO_SYM_GETTIMEOFDAY);
```

## Future Enhancements

### Phase 2: Real Time Integration
- Integration with actual kernel time sources
- Shared time page for efficient time access
- Support for different clock sources

### Phase 3: Advanced Optimizations  
- CPU-specific optimizations (RDTSC, etc.)
- Additional system calls (getuid, getgid)
- Dynamic symbol resolution

### Phase 4: Multi-Architecture Support
- x86_64 optimized implementation
- ARM/AArch64 support
- RISC-V compatibility

## Security Considerations

- VDSO page is mapped read-execute only
- No sensitive kernel data exposed
- Graceful fallback to traditional system calls
- Address space layout randomization compatible

## Compatibility

- Maintains compatibility with existing applications
- Transparent optimization - no API changes required
- Fallback mechanisms for unsupported operations
- Standard ELF auxiliary vector integration

## References

- Linux VDSO implementation
- FreeBSD shared page mechanism  
- Mach microkernel design principles
- Performance analysis frameworks