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

#include <common/bitops.h>
#include <common/kprint.h>
#include <common/lock.h>
#include <common/machine.h>
#include <common/macro.h>
#include <common/smp.h>
#include <common/tools.h>
#include <common/types.h>
#include <common/uart.h>
#include <exception/exception.h>
#include <exception/irq.h>
#include <exception/timer.h>

/* Per core IRQ SOURCE MMIO address */
u64 core_irq_source[PLAT_CPU_NUM] = {CORE0_IRQ, CORE1_IRQ, CORE2_IRQ,
                                     CORE3_IRQ};

void handle_irq(int type) {
    /**
     * Lab4
     * Acquire the big kernel lock, if :
     *	The irq is not from the kernel
     * 	The thread being interrupted is an idle thread.
     */
    if (type > ERROR_EL1h || (current_thread && current_thread->thread_ctx &&
                              current_thread->thread_ctx->type == TYPE_IDLE)) {
        lock_kernel();
    }
    plat_handle_irq();

    /**
     * Lab4
     * Do you miss something?
     */
    // sched_handle_timer_irq();
    sched();
    eret_to_thread(switch_context());
}

void plat_handle_irq(void) {
    u32 cpuid = 0;
    unsigned int irq_src, irq;

    cpuid = smp_get_cpu_id();
    irq_src = get32(core_irq_source[cpuid]);

    irq = 1 << ctzl(irq_src);
    switch (irq) {
        case INT_SRC_TIMER3:
            handle_timer_irq();
            break;
        default:
            kinfo("Unsupported IRQ %d\n", irq);
    }
    return;
}
