# CLAUDE.MD - GNU Mach Microkernel Development Guide

## Project Overview

GNU Mach is the microkernel upon which GNU Hurd is based. It provides Inter Process Communication (IPC), virtual memory management, and scheduling. This is a fork focused on modernization with 64-bit support, SMP, and enhanced debugging.

**Status**: ~75% feature complete | Primary targets: i686 (stable), x86_64 (in progress)

## Quick Reference Commands

### Build (i686 - Primary Target)
```bash
# Install dependencies (Ubuntu/WSL)
sudo apt update && sudo apt install -y build-essential gcc-multilib binutils \
  autoconf automake libtool pkg-config gawk bison flex nasm \
  xorriso grub-pc-bin mtools qemu-system-x86 git python3 cppcheck clang-tools

# Build MIG (required first time)
sudo mkdir -p /usr/include/mach
sudo cp -r include/mach/* /usr/include/mach/
sudo ln -sf $(pwd)/i386/include/mach/i386 /usr/include/mach/machine
cd mig && autoreconf --install && ./configure CPPFLAGS="-I/usr/include" && make -j$(nproc) && sudo make install && cd ..

# Configure and build kernel
autoreconf --install
./configure --host=i686-gnu CC='gcc -m32' LD='ld -melf_i386'
make -j$(nproc)
```

### Build (x86_64 - Secondary Target)
```bash
./configure --host=x86_64-gnu --enable-pae --enable-user32 MIG='mig'
make -j$(nproc)
```

### Testing
```bash
make check                    # Full test suite (15+ min)
make run-hello                # Basic functionality test
make run-vm                   # Virtual memory test
make run-mach_port            # IPC test
make debug-hello              # Start with GDB server on :1234
```

### Static Analysis
```bash
./scripts/run-static-analysis.sh
./scripts/run-static-analysis.sh --debug-focus      # Debugging code focus
./scripts/run-static-analysis.sh --timestamp-focus  # Timestamp focus
```

### Development Setup (One-liner)
```bash
./scripts/dev-setup-wsl.sh
```

## Project Structure

```
kern/           # Core kernel (scheduler, IPC, memory, threads, tensor logic)
vm/             # Virtual memory subsystem
ipc/            # Inter-process communication
i386/           # x86 32-bit architecture support (primary)
x86_64/         # x86 64-bit architecture support (in progress)
device/         # Native Mach device drivers
linux/          # Linux device driver compatibility layer
tests/          # QEMU-based test programs
scripts/        # Development and build scripts
docs/           # Architecture and API documentation
mig/            # Mach Interface Generator (included)
research/       # Mach 5 research and prototypes
```

## Development Phases

This project follows a 4-phase approach (see `CONTRIBUTING.md` for details):

| Phase | Focus | Approach |
|-------|-------|----------|
| **1** | Foundation & Quick Wins | Minimal edits, zero new warnings, housekeeping |
| **2** | Core Improvements | Performance/robustness with known methods |
| **3** | Major Features | New subsystems, design review required |
| **4** | Experimental R&D | Prototypes, research, document outcomes |

## Key Files for Common Tasks

### IPC Changes
- `ipc/ipc_port.c`, `ipc/ipc_mqueue.c` - Port and message queue operations
- `kern/ipc_kobject.c` - Kernel object IPC
- Test: `make run-mach_port`

### VM Changes
- `vm/vm_map.c`, `vm/vm_fault.c` - Memory mapping and fault handling
- `vm/vm_kern.c` - Kernel memory allocation
- `kern/rbtree.c` - VM map red-black tree implementation
- Test: `make run-vm`

### Scheduler Changes
- `kern/sched_prim.c` - Core scheduling primitives
- `kern/priority.c` - Priority management
- `kern/thread.c` - Thread management
- `kern/smp_sched.c` - SMP-aware scheduling

### Device Drivers
- `device/` - Native Mach drivers
- `linux/` - Linux compatibility layer drivers
- `device/virtio.c` - Virtio device framework

### Debugging
- `kern/gdb_stub.c` - GDB remote debugging stub
- `kern/debug.c` - Kernel debugging utilities
- `kern/printf.c` - Kernel printf implementation

### Tensor Logic (Cognitive Computing)
- `kern/tensor.h`, `kern/tensor.c` - Fixed-point tensor operations (ATen-inspired)
- `kern/atomspace.h`, `kern/atomspace.c` - Hypergraph knowledge representation (AtomSpace-inspired)
- `kern/tensor_logic.h`, `kern/tensor_logic.c` - Multi-entity tracking and network-aware operations

## Coding Standards

- Match existing formatting and style
- Descriptive names; guard clauses; handle errors early
- Prefer explicit types in public APIs
- Avoid unsafe casts; respect strict-aliasing
- Use constants from `kern/constants.h` - no magic numbers
- Run `./scripts/run-static-analysis.sh` before commits

## Build Timeouts (Important!)

| Operation | Expected Time | Minimum Timeout |
|-----------|---------------|-----------------|
| autoreconf | ~3 seconds | 60s |
| configure | ~5 seconds | 60s |
| MIG build | 30-60 seconds | 180s |
| Kernel build (clean) | 3-5 minutes | 900s |
| Single test | 30-60 seconds | 120s |
| Full test suite | 15+ minutes | 1800s |

## Common Issues & Solutions

### MIG Not Found
```bash
# Build and install MIG from included source
cd mig && autoreconf --install && ./configure && make && sudo make install && cd ..
```

### Missing GRUB/QEMU
```bash
sudo apt install -y xorriso grub-pc-bin mtools qemu-system-x86
```

### x86_64 Build Struct Size Errors
See `scripts/fix-mig-64bit.sh` for architecture-specific fixes.

### Build Hanging
Check MIG is installed: `which mig && mig -version`

## Validation Before Commits

Always run these before considering changes complete:
1. `make -j$(nproc)` - Build successfully
2. `make run-hello` - Basic test passes
3. `./scripts/run-static-analysis.sh` - No new warnings
4. Check `analysis-reports/compiler-warnings.txt`

## Documentation Index

- `docs/architecture-overview.md` - System architecture
- `docs/ipc.md` - IPC and port system
- `docs/virtual-memory.md` - VM subsystem
- `docs/debugging-guide.md` - Debugging techniques
- `docs/new-developer-guide.md` - Getting started
- `open-issues-gnumach.md` - Development roadmap
- `kern/tensor_logic.h` - Tensor Logic API (ATen + AtomSpace integration)

## Git Workflow

- Phase 1 issues: Just implement, keep PRs small
- Phase 2+: Brief rationale with changes
- Phase 3+: Design discussion before coding
- All phases: Zero new warnings, tests pass, CI green

## Resources

- GNU Mach Manual: https://www.gnu.org/software/hurd/gnumach-doc/
- Hurd FAQ: https://www.gnu.org/software/hurd/faq.html
- Bug Reports: https://savannah.gnu.org/bugs/?group=hurd
- IRC: #hurd on libera.chat
