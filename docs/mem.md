# Memory Management

## Memory Layout

```text
+--------+
|        |      - pages, npages * PAGE_SIZE
+--------+ pool <- real_start_vadd/pool_start_addr @0x60000000000
|        |      - page metadata, npages * sizeof(struct page)
+--------+      <- page_meta_start/page_metadata @0x50000000000
|        | kernel
+--------+
|        | bootloader(img_start@0x80000)
+--------+
|        | preserved
+--------+
```

At compilation time, the memory layout is determined in the linker script `linker-aarch64.lds.in`, including `img_start`, `img_end`.

At run time, the start address of pool is `free_mem_start` in `mm_init()`.

## Buddy System

At initialization, buddy system is provided with the whole memory pool, divided into many 4K pages, and calls `buddy_free_pages` to give these memory to buddy system.

All pages are organized with a `free_list` with length `BUDDY_MAX_ORDER`.
`struct page` has a field `struct list_head node`, which works as a doubly linked list.

When a page is free, just add `page->node` into the free list of the corresponding order.

In order to get a page from a node, just take the address of node and subtract the offset of `node` in `struct page`. In this implemention, it's 0, so address of `node` is address of page.

## Virtual Memory and Page Table

AArch64 has 2 page table base registers while x86-64 has only 1 page table base register. With 2 page table base registers, context switches don't have to switch page tables, thus avoiding a TLB flush. With two page table base registers, the page table of user space and kernel space are separated, resulting in security.

Assuming we have 4G memory, how much space is necessary for management?

4G / 4K = 1024 * 1024

At least 2048 L3 pages, 4 L2 page, and 1 L1, L0 page. A minimum of (2048 + 4 + 1 + 1) * 4K is sufficient.

Since we don't have to allocate a page for unused memory, a lot of space is spared.
