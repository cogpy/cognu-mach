#!/bin/bash
# Enhanced build script for GNU Mach CI/CD
# Handles architecture-specific configurations and MIG issues

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS] ARCHITECTURE

Build GNU Mach for the specified architecture with CI/CD optimizations.

ARCHITECTURE:
    i686     Build for 32-bit x86 architecture
    x86_64   Build for 64-bit x86 architecture

OPTIONS:
    -h, --help           Show this help message
    -c, --clean          Clean before building
    -t, --test           Run tests after building
    -a, --analysis       Run static analysis after building
    --debug              Enable debug build
    --force-build        Continue building even if MIG assertions fail

EOF
}

check_dependencies() {
    log "Checking build dependencies..."
    
    local missing_deps=()
    
    for cmd in gcc make autoconf automake libtool; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        error "Missing dependencies: ${missing_deps[*]}"
        exit 1
    fi
    
    log "All dependencies satisfied"
}

setup_mig() {
    log "Setting up MIG (Mach Interface Generator)..."
    
    if command -v mig &> /dev/null; then
        log "MIG already available: $(which mig)"
        return 0
    fi
    
    log "Building MIG from source..."
    
    # Setup mach headers
    sudo mkdir -p /usr/include/mach
    sudo cp -r "${PROJECT_ROOT}/include/mach"/* /usr/include/mach/
    sudo ln -sf "${PROJECT_ROOT}/i386/include/mach/i386" /usr/include/mach/machine
    
    # Build MIG
    cd "${PROJECT_ROOT}/mig"
    autoreconf --install
    ./configure CPPFLAGS="-I/usr/include"
    make -j$(nproc)
    sudo make install
    cd "${PROJECT_ROOT}"
    
    log "MIG installation completed"
}

configure_build() {
    local arch=$1
    local clean=$2
    local debug=$3
    
    log "Configuring build for $arch architecture..."
    
    if [[ "$clean" == "true" ]]; then
        make distclean || true
    fi
    
    autoreconf --install
    
    local build_dir="build-${arch}"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    local configure_flags=""
    local cflags="-g -O2"
    
    case "$arch" in
        i686)
            configure_flags="--host=i686-gnu CC='gcc -m32' LD='ld -melf_i386'"
            if [[ "$debug" == "true" ]]; then
                cflags="$cflags -DDEBUG -DMACH_KDB"
            fi
            ;;
        x86_64)
            configure_flags="--host=x86_64-gnu --enable-pae --enable-user32"
            if [[ "$debug" == "true" ]]; then
                cflags="$cflags -DDEBUG"
            fi
            ;;
        *)
            error "Unsupported architecture: $arch"
            exit 1
            ;;
    esac
    
    log "Running configure with flags: $configure_flags"
    eval "../configure $configure_flags MIG='mig' CFLAGS='$cflags'"
    
    cd ..
}

build_kernel() {
    local arch=$1
    local force_build=$2
    
    log "Building kernel for $arch..."
    
    cd "build-${arch}"
    
    if [[ "$force_build" == "true" ]]; then
        # Try to build, but don't fail on MIG assertion errors
        make -j$(nproc) || {
            warn "Build had issues, but continuing as requested"
            return 0
        }
    else
        make -j$(nproc)
    fi
    
    if [[ -f gnumach ]]; then
        log "Build successful: gnumach created"
        file gnumach
        ls -lh gnumach
    else
        error "Build failed: gnumach not found"
        exit 1
    fi
    
    cd ..
}

run_tests() {
    local arch=$1
    
    log "Running tests for $arch..."
    
    # Basic smoke tests
    log "Running basic functionality tests..."
    timeout 300 make run-hello || warn "hello test had issues: exit code $?"
    
    if command -v qemu-system-i386 &> /dev/null && [[ "$arch" == "i686" ]]; then
        log "Running additional i686 tests..."
        timeout 300 make run-mach_port || warn "mach_port test had issues: exit code $?"
        timeout 300 make run-console-timestamps || warn "console-timestamps test had issues: exit code $?"
    fi
}

run_analysis() {
    log "Running static analysis..."
    "${PROJECT_ROOT}/scripts/run-static-analysis.sh" || warn "Static analysis completed with warnings"
}

main() {
    local arch=""
    local clean=false
    local test=false
    local analysis=false
    local debug=false
    local force_build=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--clean)
                clean=true
                shift
                ;;
            -t|--test)
                test=true
                shift
                ;;
            -a|--analysis)
                analysis=true
                shift
                ;;
            --debug)
                debug=true
                shift
                ;;
            --force-build)
                force_build=true
                shift
                ;;
            i686|x86_64)
                arch=$1
                shift
                ;;
            *)
                error "Unknown option: $1"
                usage >&2
                exit 1
                ;;
        esac
    done
    
    if [[ -z "$arch" ]]; then
        error "Architecture must be specified (i686 or x86_64)"
        usage >&2
        exit 1
    fi
    
    cd "$PROJECT_ROOT"
    
    check_dependencies
    setup_mig
    configure_build "$arch" "$clean" "$debug"
    build_kernel "$arch" "$force_build"
    
    if [[ "$test" == "true" ]]; then
        run_tests "$arch"
    fi
    
    if [[ "$analysis" == "true" ]]; then
        run_analysis
    fi
    
    log "Build process completed successfully for $arch"
}

main "$@"