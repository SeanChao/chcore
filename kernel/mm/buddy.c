#include "buddy.h"

#include <common/kmalloc.h>
#include <common/kprint.h>
#include <common/macro.h>
#include <common/util.h>

static int check(struct phys_mem_pool *pool) {
    int page_num = 524288;
    int sum = 0;
    for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
        int list_nb_free = pool->free_lists[i].nr_free;
        if (pool->free_lists[i].nr_free < 0 ||
            pool->free_lists[i].nr_free >
                pool->pool_mem_size / (BUDDY_PAGE_SIZE * (1 << i)))
            return 1;
        sum += list_nb_free * (1 << i);
    }
}

/*
 * The layout of a phys_mem_pool:
 * | page_metadata are (an array of struct page) | alignment pad | usable memory
 * |
 *
 * The usable memory: [pool_start_addr, pool_start_addr + pool_mem_size).
 */
void init_buddy(struct phys_mem_pool *pool, struct page *start_page,
                vaddr_t start_addr, u64 page_num) {
    int order;
    int page_idx;
    struct page *page;

    /* Init the physical memory pool. */
    pool->pool_start_addr = start_addr;
    pool->page_metadata = start_page;
    pool->pool_mem_size = page_num * BUDDY_PAGE_SIZE;
    /* This field is for unit test only. */
    pool->pool_phys_page_num = page_num;

    /* Init the free lists */
    for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
        pool->free_lists[order].nr_free = 0;
        init_list_head(&(pool->free_lists[order].free_list));
    }

    /* Clear the page_metadata area. */
    memset((char *)start_page, 0, page_num * sizeof(struct page));

    /* Init the page_metadata area. */
    for (page_idx = 0; page_idx < page_num; ++page_idx) {
        page = start_page + page_idx;
        page->allocated = 1;
        page->order = 0;
    }

    /* Put each physical memory page into the free lists. */
    for (page_idx = 0; page_idx < page_num; ++page_idx) {
        page = start_page + page_idx;
        buddy_free_pages(pool, page);
    }
}

/**
 * Get the buddy page of given page
 *
 * @param pool physical memory structure reserved in the kernel
 * @param chunk the page to find buddy for
 */
static struct page *get_buddy_chunk(struct phys_mem_pool *pool,
                                    struct page *chunk) {
    u64 chunk_addr;
    u64 buddy_chunk_addr;
    int order;

    /* Get the address of the chunk. */
    chunk_addr = (u64)page_to_virt(pool, chunk);
    order = chunk->order;
    /*
     * Calculate the address of the buddy chunk according to the address
     * relationship between buddies.
     */
#define BUDDY_PAGE_SIZE_ORDER (12)
    buddy_chunk_addr = chunk_addr ^ (1UL << (order + BUDDY_PAGE_SIZE_ORDER));

    /* Check whether the buddy_chunk_addr belongs to pool. */
    if ((buddy_chunk_addr < pool->pool_start_addr) ||
        (buddy_chunk_addr >= (pool->pool_start_addr + pool->pool_mem_size))) {
        return NULL;
    }

    return virt_to_page(pool, (void *)buddy_chunk_addr);
}

/**
 * split_page: split the memory block into two smaller sub-block, whose order
 * is half of the origin page.
 * pool @ physical memory structure reserved in the kernel
 * @param order split order limit
 * @param page splitted page
 *
 * Hints: don't forget to substract the free page number for the corresponding
 * free_list. you can invoke split_page recursively until the given page can not
 * be splitted into two smaller sub-pages.
 */
static struct page *split_page(struct phys_mem_pool *pool, u64 target_order,
                               struct page *page) {
    if (page->order == target_order) return page;
    struct page *splitted = NULL;
    struct free_list *list = &pool->free_lists[page->order];
    list->nr_free--;
    list_del(&page->node);
    int small_order = page->order - 1;
    struct free_list *small_list = &pool->free_lists[small_order];
    struct page *sub_page1 = page;
    struct page *sub_page2 = page + (1 << small_order);
    struct page *pages[] = {sub_page1, sub_page2};
    for (int i = 0; i < 2; i++) {
        struct page *p = pages[i];
        p->order = small_order;
        p->allocated = 0;
        list_add(&p->node, &small_list->free_list);
    }
    small_list->nr_free += 2;
    splitted = sub_page1;
    return split_page(pool, target_order, splitted);
}

/**
 * buddy_get_pages: get free page from buddy system.
 * @param pool physical memory structure reserved in the kernel
 * @param order get the (1<<order) continuous pages from the buddy system
 *
 */
struct page *buddy_get_pages(struct phys_mem_pool *pool, u64 order) {
    //  Hints: Find the corresponding free_list which can allocate 1<<order
    //  continuous pages and don't forget to split the list node after
    //  allocation
    check(pool);
    struct page *page = NULL;
    if (order < 0 || order > BUDDY_MAX_ORDER) return NULL;
    struct free_list *list = &pool->free_lists[order];
    // find an order that can satisfy the request
    int order_it = order;
    while (list->nr_free == 0) {
        if (order_it >= BUDDY_MAX_ORDER) return NULL;
        list = &pool->free_lists[++order_it];
    }
    void *node_ptr = list->free_list.prev;
    page = node_ptr - offsetof(struct page, node);
    page = split_page(pool, order, page);
    pool->free_lists[page->order].nr_free--;
    list_del(node_ptr);
    page->allocated = 1;
    check(pool);
    return page;
}

/**
 * merge_page: merge the given page with the buddy page
 * @param pool physical memory structure reserved in the kernel
 * @param page merged page (attempted)
 * @return merged page
 *
 * Hints: you can invoke the merge_page recursively until
 * there is not corresponding buddy page. get_buddy_chunk
 * is helpful in this function.
 */
static struct page *merge_page(struct phys_mem_pool *pool, struct page *page) {
    if (!page) return NULL;
    int order = page->order;
    if (order == BUDDY_MAX_ORDER - 1) return page;
    struct page *merged = NULL;
    struct page *buddy_page = get_buddy_chunk(pool, page);
    if (!buddy_page->allocated && buddy_page->order == order) {
        merged = MIN(page, buddy_page);
        // buddy is not allocated, merge
        int new_order = order + 1;
        // update free_list: remove old pages from its freelist, add to new list
        list_del(&page->node);
        list_del(&buddy_page->node);
        list_add(&merged->node, &pool->free_lists[new_order].free_list);
        merged->allocated = 0;
        merged->order = new_order;
        pool->free_lists[order].nr_free -= 2;
        pool->free_lists[new_order].nr_free += 1;
    } else
        return page;
    return merge_page(pool, merged);
}

/**
 * buddy_free_pages: give back the pages to buddy system
 * @param pool physical memory structure reserved in the kernel
 * @param page free page structure
 * Hints: you can invoke merge_page.
 */
void buddy_free_pages(struct phys_mem_pool *pool, struct page *page) {
    if (!page->allocated) return;
    page->allocated = 0;
    // put to list
    int order = page->order;
    if (order < 0 || order > BUDDY_MAX_ORDER) return;
    struct free_list *freelists = pool->free_lists;
    struct free_list *list = &freelists[order];
    list_add(&page->node, &list->free_list);
    list->nr_free++;
    // merge
    merge_page(pool, page);
}

void *page_to_virt(struct phys_mem_pool *pool, struct page *page) {
    u64 addr;

    /* page_idx * BUDDY_PAGE_SIZE + start_addr */
    addr =
        (page - pool->page_metadata) * BUDDY_PAGE_SIZE + pool->pool_start_addr;
    return (void *)addr;
}

struct page *virt_to_page(struct phys_mem_pool *pool, void *addr) {
    struct page *page;

    page = pool->page_metadata +
           (((u64)addr - pool->pool_start_addr) / BUDDY_PAGE_SIZE);
    return page;
}

u64 get_free_mem_size_from_buddy(struct phys_mem_pool *pool) {
    int order;
    struct free_list *list;
    u64 current_order_size;
    u64 total_size = 0;

    for (order = 0; order < BUDDY_MAX_ORDER; order++) {
        /* 2^order * 4K */
        current_order_size = BUDDY_PAGE_SIZE * (1 << order);
        list = pool->free_lists + order;
        total_size += list->nr_free * current_order_size;

        /* debug : print info about current order */
        kdebug("buddy memory chunk order: %d, size: 0x%lx, num: %d\n", order,
               current_order_size, list->nr_free);
    }
    return total_size;
}
