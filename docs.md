# ChCore Documentation

## ARM64 Basics

Calling Convention

`x0` to `x7` are parameter/result registers.

`r19`...`r28` are callee-saved registers.

`x29` is the frame pointer(sp).

`sp` is the stack pointer.

`x30` stores return address.

## Common Instructions

- `adrp Xd Label` Form PC-relative address to 4KB page.
- `mov Xt1, sp` Xt1 <- sp
- `stp Xt1, Xt2, [sp, #-16]!` store 2 registers to mem[sp-16], decrements sp by 16,
  set `sp` to `sp-16`

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

### Stack Initialization

the kernel stack address is loaded to `kernel_stack` (@`kernel/head.S`), the size of stack is 8192 (defined in `kernel/common/vars.h`). The space is reserved by adding `sp` by size, since the stack grows from high address to low address.

The top limit of kernel stack is `0xffffff000008c018`.

## Reference

- [MPIDR_EL1, Multiprocessor Affinity Register, EL1](https://developer.arm.com/documentation/100403/0200/register-descriptions/aarch64-system-registers/mpidr-el1--multiprocessor-affinity-register--el1)
- [Procedure Call Standard for the Arm® 64-bit Architecture](https://developer.arm.com/documentation/ihi0055/latest/)
