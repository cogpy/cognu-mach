# GNU Mach Microkernel Development Instructions

**MANDATORY**: Always reference these instructions first and only fallback to search or bash commands when you encounter unexpected information that does not match the content here.

## Working Effectively

### Bootstrap and Build Commands
- Install system dependencies (Ubuntu/WSL):
  ```bash
  sudo apt update && sudo apt install -y build-essential gcc-multilib binutils binutils-multiarch \
    autoconf automake libtool pkg-config gawk bison flex nasm \
    xorriso grub-pc-bin mtools qemu-system-x86 \
    git python3 cppcheck clang-tools texinfo
  ```
- **NEVER CANCEL**: Dependency installation takes 2-3 minutes. Always wait for completion.

### Build Process (i686 Architecture - Primary Target)
- Prepare build system:
  ```bash
  autoreconf --install  # Takes ~3 seconds
  ```
- Configure:
  ```bash
  ./configure --host=i686-gnu CC='gcc -m32' LD='ld -melf_i386'  # Takes ~5 seconds
  ```
- **CRITICAL - MIG Dependency**: Build will fail without MIG (Mach Interface Generator). MIG is included in this repository but requires proper header setup:
  ```bash
  # Set up headers for MIG build
  sudo mkdir -p /usr/include/mach
  sudo cp -r include/mach/* /usr/include/mach/
  sudo ln -sf $(pwd)/i386/include/mach/i386 /usr/include/mach/machine
  
  # Build and install MIG
  cd mig && autoreconf --install && ./configure CPPFLAGS="-I/usr/include"
  make -j$(nproc) && sudo make install && cd ..
  ```
- **NEVER CANCEL**: MIG build takes 30-60 seconds. Set timeout to 180+ seconds.
- Build kernel:
  ```bash
  make -j$(nproc)  # Takes 1-2 minutes for incremental, 3-5 minutes for clean build
  ```
- **NEVER CANCEL**: Full build takes up to 5 minutes. Set timeout to 900+ seconds (15 minutes).

### x86_64 Architecture (Secondary Target)
- Configure x86_64 with PAE and user32 support:
  ```bash
  ./configure --host=x86_64-gnu --enable-pae --enable-user32 MIG='mig'
  ```
- **NEVER CANCEL**: x86_64 build takes similar time to i686. Set timeout to 900+ seconds.

### Testing Commands
- **CRITICAL - QEMU Required**: Tests run in QEMU emulation and require proper setup.
- Run all tests:
  ```bash
  make check  # Takes 5-15 minutes depending on test count
  ```
- **NEVER CANCEL**: Test suite takes 15+ minutes. Set timeout to 1800+ seconds (30 minutes).
- Run specific test:
  ```bash
  make run-hello          # Basic functionality test (~30 seconds)
  make run-vm             # Virtual memory test (~60 seconds)  
  make run-mach_port      # IPC test (~45 seconds)
  make run-console-timestamps  # Console timestamp functionality (~30 seconds)
  ```
- Debug a test with GDB:
  ```bash
  make debug-hello  # Starts QEMU with GDB server on port 1234
  # In another terminal:
  gdb gnumach -ex 'target remote :1234' -ex 'b setup_main' -ex c
  ```

### Static Analysis
- Run comprehensive analysis:
  ```bash
  ./scripts/run-static-analysis.sh  # Takes ~20 seconds
  ```
- **NEVER CANCEL**: Static analysis takes up to 60 seconds. Set timeout to 300+ seconds.
- Focused debugging analysis:
  ```bash
  ./scripts/run-static-analysis.sh --debug-focus     # Analyze debugging code
  ./scripts/run-static-analysis.sh --timestamp-focus # Analyze timestamp functionality
  ```

### Validation Requirements
- **MANDATORY VALIDATION STEPS**: Always run these before considering changes complete:
  1. Build successfully: `make -j$(nproc)`
  2. Run basic test: `make run-hello`
  3. Run static analysis: `./scripts/run-static-analysis.sh`
  4. Verify no new warnings: Check `analysis-reports/compiler-warnings.txt`

## Manual Testing Scenarios
After making changes to the kernel, **ALWAYS** validate with these scenarios:

### Basic Functionality Test
```bash
make run-hello
# Expected: Test boots, prints messages, shows "gnumach-test-success-and-reboot"
# Timing: Completes in ~30 seconds
```

### IPC and Memory Management Test
```bash
make run-vm && make run-mach_port
# Expected: Both tests pass, demonstrating core kernel functionality
# Timing: Each test takes 30-60 seconds
```

### Console and Debugging Test
```bash
make run-console-timestamps
# Expected: Verifies timestamp format [seconds.milliseconds] in kernel logs
# Timing: Completes in ~30 seconds
```

### Full Test Suite (for significant changes)
```bash
make check
# Expected: All tests pass or show expected failures
# Timing: 15+ minutes, NEVER CANCEL
```

## Build Failure Troubleshooting

### MIG Not Found Error
```
configure: WARNING: mig was not found, we will not be able to build a kernel
```
**Solution**: Build MIG from the included source as shown above.

### Missing GRUB/QEMU Tools
```
grub-mkrescue: command not found
qemu-system-i386: command not found
```
**Solution**: Install missing packages:
```bash
sudo apt install -y xorriso grub-pc-bin mtools qemu-system-x86
```

### Build Hanging on MIG Generation
If build appears stuck on MIG file generation, check MIG is properly installed:
```bash
which mig && mig -version
```

## Key Project Structure

### Core Kernel Directories
- `kern/` - Core kernel functionality (scheduler, IPC, memory management)
- `vm/` - Virtual memory subsystem
- `ipc/` - Inter-process communication
- `i386/` - x86 32-bit architecture support  
- `x86_64/` - x86 64-bit architecture support
- `device/` - Device drivers
- `linux/` - Linux device driver compatibility layer

### Build and Configuration
- `configure.ac` - Autotools configuration
- `Makefile.am` - Main build rules
- `Makefrag.am` - Build fragment for kernel components
- `build-aux/` - Autotools helper scripts

### Testing Infrastructure
- `tests/` - User-space test programs
- `tests/user-qemu.mk` - QEMU test runner configuration
- `tests/test-*.c` - Individual test cases
- `tests/run-qemu.sh.template` - QEMU test execution template

### Documentation and Development
- `README` - Basic project information and build instructions
- `CONTRIBUTING.md` - Development guidelines and project phases
- `scripts/dev-setup-wsl.sh` - WSL development environment setup
- `scripts/run-static-analysis.sh` - Code quality analysis

## Timing Expectations (Measured)
- **autoreconf**: ~3 seconds
- **configure**: ~5 seconds  
- **MIG build**: 30-60 seconds
- **Kernel build (incremental)**: 1-2 minutes
- **Kernel build (clean)**: 3-5 minutes
- **Single test**: 30-60 seconds
- **Full test suite**: 15+ minutes
- **Static analysis**: ~20 seconds

## Common Development Tasks

### Adding a New Test
1. Create `tests/test-yourfeature.c`
2. Follow existing test patterns (see `tests/test-hello.c`)
3. Add to `USER_TESTS` in `tests/user-qemu.mk`
4. Test with: `make run-yourfeature`

### Modifying Core Kernel Code
1. Edit files in `kern/`, `vm/`, or `ipc/`
2. **Always build and test**: `make -j$(nproc) && make run-hello`
3. **Always run analysis**: `./scripts/run-static-analysis.sh`
4. For IPC changes, test: `make run-mach_port`
5. For VM changes, test: `make run-vm`

### Debugging Kernel Issues
1. Use KDB (in-kernel debugger): Configure with `--enable-kdb`
2. Use QEMU + GDB: `make debug-hello` then attach GDB
3. Check analysis reports in `analysis-reports/`
4. Review console output for panic messages

### Working with Device Drivers  
1. Most drivers are in `linux/` (Linux compatibility layer)
2. Native Mach drivers are in `device/`
3. **Always test**: Boot and verify device functionality
4. Network drivers: `grep CONFIG_.*NET build-*/config.h`
5. SCSI drivers: `grep CONFIG_SCSI build-*/config.h`

Remember: This is a microkernel - the kernel provides minimal services (IPC, scheduling, memory management) with most functionality in user-space servers.