# Kernel Feature Integration Module

## Overview

The Kernel Feature Integration module provides a small, thread-safe framework
for registering and controlling an integrated kernel feature at runtime. It
tracks the feature state, capability flags for the supported platforms, and
operation statistics (call counts and latency) with minimal overhead.

## Components

### Core Module (`kern/new_feature.h`, `kern/new_feature.c`)

- `feature_init()` — initialize the subsystem during kernel startup; detects
  architecture capabilities (x86, x86_64, QEMU) and prepares synchronization.
- `feature_enable()` / `feature_disable()` — transition the feature between
  the disabled and enabled states; idempotent and protected by a simple lock.
- `feature_get_state()` / `feature_is_enabled()` — query the current state.
- `feature_get_stats()` / `feature_reset_stats()` — inspect and clear the
  operation statistics (init/enable/disable counts, total operations,
  average and maximum operation latency in microseconds).

### System Call Interface (`include/mach/kernel_feature.defs`)

A MIG subsystem (base message id 5300) exposing the feature to user space:

- `kernel_feature_enable(host)` — enable the feature.
- `kernel_feature_disable(host)` — disable the feature.
- `kernel_feature_get_state(host, &state)` — read the current state.
- `kernel_feature_get_stats(host, ...)` — retrieve operation statistics.
- `kernel_feature_is_enabled(host, &enabled)` — check whether the feature is
  enabled.
- `kernel_feature_reset_stats(host)` — reset the statistics counters.

The server side is implemented in `kern/kernel_feature_server.c`, which
validates the host port argument and forwards each request to the core
module.

## Initialization

`feature_init()` is called from `setup_main()` in `kern/startup.c` after the
console and core kernel services are available.

## Testing

- `tests/test-kernel-feature.c` exercises the full system call interface from
  user space (state query, enable, statistics, disable, reset).
- `validate-kernel-feature.sh` performs a build-independent structural
  validation of the module and its integration points.

Run the QEMU test with:

```sh
make run-kernel-feature
```
