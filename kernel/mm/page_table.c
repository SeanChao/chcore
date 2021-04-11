/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS),
 * Shanghai Jiao Tong University (SJTU) OS-Lab-2020 (i.e., ChCore) is licensed
 * under the Mulan PSL v1. You can use this software according to the terms and
 * conditions of the Mulan PSL v1. You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v1 for more details.
 */

#ifdef CHCORE
#include <common/kmalloc.h>
#include <common/util.h>
#endif
#include <common/errno.h>
#include <common/kprint.h>
#include <common/macro.h>
#include <common/mm.h>
#include <common/mmu.h>
#include <common/printk.h>
#include <common/types.h>
#include <common/vars.h>

#include "page_table.h"

/* Page_table.c: Use simple impl for debugging now. */

extern void set_ttbr0_el1(paddr_t);
extern void flush_tlb(void);

void set_page_table(paddr_t pgtbl) {
    set_ttbr0_el1(pgtbl);
}

#define USER_PTE   0
#define KERNEL_PTE 1
/*
 * the 3rd arg means the kind of PTE.
 */
static int set_pte_flags(pte_t *entry, vmr_prop_t flags, int kind) {
    if (flags & VMR_WRITE)
        entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RW_EL0_RW;
    else
        entry->l3_page.AP = AARCH64_PTE_AP_HIGH_RO_EL0_RO;

    if (flags & VMR_EXEC)
        entry->l3_page.UXN = AARCH64_PTE_UX;
    else
        entry->l3_page.UXN = AARCH64_PTE_UXN;

    // EL1 cannot directly execute EL0 accessiable region.
    if (kind == USER_PTE) entry->l3_page.PXN = AARCH64_PTE_PXN;
    entry->l3_page.AF = AARCH64_PTE_AF_ACCESSED;

    // inner sharable
    entry->l3_page.SH = INNER_SHAREABLE;
    // memory type
    entry->l3_page.attr_index = NORMAL_MEMORY;

    return 0;
}

#define GET_PADDR_IN_PTE(entry) \
    (((u64)entry->table.next_table_addr) << PAGE_SHIFT)
#define GET_NEXT_PTP(entry) phys_to_virt(GET_PADDR_IN_PTE(entry))

#define NORMAL_PTP (0)
#define BLOCK_PTP  (1)

/**
 * Find next page table page for the "va".
 *
 * @param cur_ptp: current page table page
 * @param level:   current ptp level
 * @param next_ptp: returns "next_ptp"
 * @param pte     : returns "pte" (points to next_ptp) in "cur_ptp"
 * @param alloc: if true, allocate a ptp when missing
 *
 */
static int get_next_ptp(ptp_t *cur_ptp, u32 level, vaddr_t va, ptp_t **next_ptp,
                        pte_t **pte, bool alloc) {
    u32 index = 0;
    pte_t *entry;

    if (cur_ptp == NULL) return -ENOMAPPING;

    switch (level) {
        case 0:
            index = GET_L0_INDEX(va);
            break;
        case 1:
            index = GET_L1_INDEX(va);
            break;
        case 2:
            index = GET_L2_INDEX(va);
            break;
        case 3:
            index = GET_L3_INDEX(va);
            break;
        default:
            BUG_ON(1);
    }

    entry = &(cur_ptp->ent[index]);
    if (IS_PTE_INVALID(entry->pte)) {
        if (alloc == false) {
            return -ENOMAPPING;
        } else {
            /* alloc a new page table page */
            ptp_t *new_ptp;
            paddr_t new_ptp_paddr;
            pte_t new_pte_val;

            /* alloc a single physical page as a new page table page */
            new_ptp = get_pages(0);
            BUG_ON(new_ptp == NULL);
            memset((void *)new_ptp, 0, PAGE_SIZE);
            new_ptp_paddr = virt_to_phys((vaddr_t)new_ptp);

            new_pte_val.pte = 0;
            new_pte_val.table.is_valid = 1;
            new_pte_val.table.is_table = 1;
            new_pte_val.table.next_table_addr = new_ptp_paddr >> PAGE_SHIFT;

            /* same effect as: cur_ptp->ent[index] = new_pte_val; */
            entry->pte = new_pte_val.pte;
        }
    }
    *next_ptp = (ptp_t *)GET_NEXT_PTP(entry);
    *pte = entry;
    if (IS_PTE_TABLE(entry->pte))
        return NORMAL_PTP;
    else
        return BLOCK_PTP;
}

#define PTP_ERROR(code) ((code) != NORMAL_PTP && (code) != BLOCK_PTP)

/*
 * Translate a va to pa, and get its pte for the flags
 */
/**
 * query_in_pgtbl: translate virtual address to physical
 * address and return the corresponding page table entry
 *
 * @param pgtbl @ ptr for the first level page table(pgd) virtual address
 * @param va @ query virtual address
 * @param pa @ return physical address
 * @param entry @ return page table entry
 *
 * Hint: check the return value of get_next_ptp, if ret == BLOCK_PTP
 * return the pa and block entry immediately
 */
int query_in_pgtbl(vaddr_t *pgtbl, vaddr_t va, paddr_t *pa, pte_t **entry) {
    int err;
    ptp_t *cur_ptp = (ptp_t *)pgtbl;
    pte_t *pte = NULL;
    err = get_next_ptp(cur_ptp, 0, va, &cur_ptp, &pte, false);
    if (err) return err;
    err = get_next_ptp(cur_ptp, 1, va, &cur_ptp, &pte, false);
    if (err) return err;
    err = get_next_ptp(cur_ptp, 2, va, &cur_ptp, &pte, false);
    if (err) return err;
    err = get_next_ptp(cur_ptp, 3, va, &cur_ptp, &pte, false);
    if (err) return err;
    *pa = pte->l3_page.pfn;
    *entry = pte;
    return 0;
}

int query_in_pgtbl_level(vaddr_t *pgtbl, vaddr_t va, paddr_t *pa, pte_t **entry,
                         u32 level) {
    int err;
    ptp_t *cur_ptp = (ptp_t *)pgtbl;
    pte_t *pte = NULL;
    err = get_next_ptp(cur_ptp, 0, va, &cur_ptp, &pte, false);
    if (err) return err;
    switch (level) {
        case 3:
            err = get_next_ptp(cur_ptp, 1, va, &cur_ptp, &pte, false);
            if (PTP_ERROR(err)) return err;
            err = get_next_ptp(cur_ptp, 2, va, &cur_ptp, &pte, false);
            if (PTP_ERROR(err)) return err;
            err = get_next_ptp(cur_ptp, 3, va, &cur_ptp, &pte, false);
            if (PTP_ERROR(err)) return err;
            *pa = pte->l3_page.pfn;
            break;
        case 2:
            err = get_next_ptp(cur_ptp, 1, va, &cur_ptp, &pte, false);
            if (PTP_ERROR(err)) return err;
            err = get_next_ptp(cur_ptp, 2, va, &cur_ptp, &pte, false);
            if (PTP_ERROR(err)) return err;
            *pa = (pte->l2_block.pfn << 21) | (va & 0x1FFFFF);
            break;
        case 1:
            err = get_next_ptp(cur_ptp, 1, va, &cur_ptp, &pte, false);
            if (PTP_ERROR(err)) return err;
            *pa = pte->l1_block.pfn;
            break;
        default:
            BUG_ON("Error: unexpected level\n");
            break;
    }
    *entry = pte;
    return 0;
}

/**
 * map_range_in_pgtbl: map the virtual address [va:va+size] to
 * physical address[pa:pa+size] in given pgtbl
 *
 * @param pgtbl ptr for the first level page table(pgd) virtual address
 * @param va start virtual address
 * @param pa start physical address
 * @param len mapping size
 * @param flag corresponding attribution bit
 *
 * Hint: In this function you should first invoke the get_next_ptp()
 * to get the each level page table entries. Read type pte_t carefully
 * and it is convenient for you to call set_pte_flags to set the page
 * permission bit. Don't forget to call flush_tlb at the end of this function
 */
int map_range_in_pgtbl(vaddr_t *pgtbl, vaddr_t va, paddr_t pa, size_t len,
                       vmr_prop_t flags) {
    kdebug(
        "mm/page_table/map_range_in_pgtbl: table@0x%lx va->pa 0x%lx->0x%lx\n",
        pgtbl, va, pa);
    int level = 4;
    int n_pages = len / PAGE_SIZE;
    for (int pg = 0; pg < n_pages; pg++) {
        ptp_t *cur_pgtbl = (ptp_t *)pgtbl;
        ptp_t *ptp = NULL;
        pte_t *pte = NULL;
        for (int i = 0; i < level; i++) {
            int err = get_next_ptp(cur_pgtbl, i, va, &ptp, &pte, true);
            if (err) {
                return err;
            }
            cur_pgtbl = ptp;
            set_pte_flags(pte, flags, USER_PTE);
        }
        pte->l3_page.is_valid = 1;
        pte->l3_page.is_page = 1;
        pte->l3_page.pfn = pa >> L3_INDEX_SHIFT;  // this kind of assignment
                                                  // only takes low address bits
        set_pte_flags(pte, flags, USER_PTE);
        va += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
    flush_tlb();
    kdebug("map range ok\n");
    return 0;
}

/**
 * The same as `map_range_in_pgtbl`, but map 2M pages
 *
 * @param pgtbl ptr for the first level page table(pgd) virtual address
 * @param va start virtual address
 * @param pa start physical address
 * @param len mapping size
 * @param flag corresponding attribution bit
 *
 */
int map_range_in_pgtbl_2m(vaddr_t *pgtbl, vaddr_t va, paddr_t pa, size_t len,
                          vmr_prop_t flags) {
    int level = 3;
    const int page_size = L2_PAGE_SIZE;
    int n_pages = len / page_size;
    for (int pg = 0; pg < n_pages; pg++) {
        ptp_t *cur_pgtbl = (ptp_t *)pgtbl;
        ptp_t *ptp = NULL;
        pte_t *pte = NULL;
        for (int i = 0; i < level; i++) {
            int err = get_next_ptp(cur_pgtbl, i, va, &ptp, &pte, true);
            // kinfo("visit pg=%d, level=%d, ptp=%x, pte=%x\n", pg, i, ptp,
            // pte);
            if (err != NORMAL_PTP && err != BLOCK_PTP) {
                return err;
            }
            cur_pgtbl = ptp;
        }
        pte->l2_block.pfn = pa >> 21;
        pte->l2_block.is_table = 0;
        pte->l2_block.UXN = 1;
        pte->l2_block.AF = 1;
        pte->l2_block.SH = 3;
        pte->l2_block.attr_index = 4;
        pte->l2_block.is_table = 0;
        pte->l2_block.is_valid = 1;
        va += page_size;
        pa += page_size;
    }
    flush_tlb();
    return 0;
}

/**
 * unmap_range_in_pgtble: unmap the virtual address [va:va+len]
 *
 * @param pgtbl ptr for the first level page table(pgd) virtual address
 * @param va start virtual address
 * @param len unmapping size
 *
 * Hint: invoke get_next_ptp to get each level page table, don't
 * forget the corner case that the virtual address is not mapped.
 * call flush_tlb() at the end of function
 *
 */
int unmap_range_in_pgtbl(vaddr_t *pgtbl, vaddr_t va, size_t len) {
    int err = 0;
    pte_t *pte;
    ptp_t *cur_pt = (ptp_t *)pgtbl;
    int n_pages = len / PAGE_SIZE;
    for (int pg = 0; pg < n_pages; pg++) {
        err = get_next_ptp(cur_pt, 0, va, &cur_pt, &pte, false);
        if (err) return err;
        err = get_next_ptp(cur_pt, 1, va, &cur_pt, &pte, false);
        if (err) return err;
        err = get_next_ptp(cur_pt, 2, va, &cur_pt, &pte, false);
        if (err) return err;
        err = get_next_ptp(cur_pt, 3, va, &cur_pt, &pte, false);
        if (err) return err;
        pte->l3_page.is_valid = false;
        va += PAGE_SIZE;
    }
    flush_tlb();
    return 0;
}

// TODO: add hugepage support for user space.
