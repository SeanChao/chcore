# ChCore Documentation

## Machine Booting

The OS entry point is at `0x80000: _start`, defined at `/boot/start.S`.

```asm
mrs	x8, mpidr_el1   # mpidr_el1: 
and	x8, x8,	#0xFF
cbz	x8, primary
```

The `_start` function first loads `mpidr_el`, which is a 64-bit read-only register, and is part of the Other system control registers functional group, and takes its lowest 1 byte, which identifies individual threads within a multi-threaded core.

Therefore, only thread 0 jumps to `primary` and runs. For other threads, control flow jumps to `secondary_hang` repeatedly, and thus hangs.

Tip: use `set scheduler-locking step` to follow only one thread's execution in gdb.

After initialization jobs are done in primary, it jumps to init_c (@`boot/init_c.c`). `init_c` calls `start_kernel` (@`kernel/head.S`), then jumps to `main` (@`kernel/main.c`).

## Reference

- [MPIDR_EL1, Multiprocessor Affinity Register, EL1](https://developer.arm.com/documentation/100403/0200/register-descriptions/aarch64-system-registers/mpidr-el1--multiprocessor-affinity-register--el1)
