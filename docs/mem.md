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

The start address of pool is `free_mem_start` in `mm_init()`.

## Buddy System

At initialization, buddy system is provided with the whole memory pool, divided into many 4K pages, and calls `buddy_free_pages` to give these memory to buddy system.

All pages are organized with a `free_list` with length `BUDDY_MAX_ORDER`.
`struct page` has a field `struct list_head node`, which works as a doubly linked list.

When a page is free, just add `page->node` into the free list of the corresponding order.

In order to get a page from a node, just take the address of node and subtract the offset of `node` in `struct page`. In this implemention, it's 0, so address of `node` is address of page.
