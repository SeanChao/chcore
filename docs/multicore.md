# Multicore

## Enabling Multicore

The `_start` function first loads `mpidr_el`, which is a 64-bit read-only register, and is part of the other system control registers functional group, and takes its lowest 1 byte, which identifies individual threads within a multi-threaded core.

Only the main CPU core jumps to `primary`. Others cores are blocked.

## Backup CPU Cores Startup

After a core detects `secondary_boot_flag` is set, the cpu core begins to initialize.
It jumps to `secondary_init_c` and calls `secondary_cpu_boot`, in which it prepares the kernel stack and jumps to `secondary_start`.

Stack like `kernel_stack` is shared, so these CPU cores cannot start concurrently.

## Locks

When calling `unlock_kernel` in `exception_return`, no registers are saved because they are not used.

When an idle thread is running in kernel, lock should be acquired because `unlock` will be called if it's scheduled to some user thread.
