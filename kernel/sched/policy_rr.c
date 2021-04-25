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
/* Scheduler related functions are implemented here */
#include <common/errno.h>
#include <common/kmalloc.h>
#include <common/kprint.h>
#include <common/list.h>
#include <common/machine.h>
#include <common/macro.h>
#include <common/smp.h>
#include <common/util.h>
#include <exception/irq.h>
#include <process/thread.h>
#include <sched/context.h>
#include <sched/sched.h>

/* in arch/sched/idle.S */
void idle_thread_routine(void);

/*
 * rr_ready_queue
 * Per-CPU ready queue for ready tasks.
 */
struct list_head rr_ready_queue[PLAT_CPU_NUM];

/*
 * RR policy also has idle threads.
 * When no active user threads in ready queue,
 * we will choose the idle thread to execute.
 * Idle thread will **NOT** be in the RQ.
 */
struct thread idle_threads[PLAT_CPU_NUM];

/*
 * Lab4
 * Sched_enqueue
 * Put `thread` at the end of ready queue of assigned `affinity`.
 * If affinity = NO_AFF, assign the core to the current cpu.
 * If the thread is IDLE thread, do nothing!
 * Do not forget to check if the affinity is valid!
 */
int rr_sched_enqueue(struct thread *thread) {
    if (thread == NULL || thread->thread_ctx == NULL) return -1;
    if (thread->thread_ctx->type == TYPE_IDLE) return 0;
    // if thread state is TS_READY, return error
    if (thread->thread_ctx->state == TS_READY) return -2;
    if (thread == &idle_threads[smp_get_cpu_id()]) return -3;
    u32 cpu_id = smp_get_cpu_id();
    s32 aff = thread->thread_ctx->affinity;
    if (INVALID_AFF(aff)) return -4;
    s32 cpu = (aff == NO_AFF) ? cpu_id : aff;
    list_append(&thread->ready_queue_node, &rr_ready_queue[cpu]);
    thread->thread_ctx->cpuid = cpu;
    thread->thread_ctx->state = TS_READY;
    // kdebug("rr: enqueue %lx\n", thread);
    return 0;
}

/*
 * Lab4
 * Sched_dequeue
 * remove `thread` from its current residual ready queue
 * Do not forget to add some basic parameter checking
 */
int rr_sched_dequeue(struct thread *thread) {
    if (thread == NULL || thread->thread_ctx == NULL) return -1;
    if (thread == &idle_threads[smp_get_cpu_id()]) return -2;
    if (thread->thread_ctx->state == TS_RUNNING) return -3;
    if (INVALID_AFF(thread->thread_ctx->affinity)) return -4;
    // Remove the thread and set its state to TS_INTER
    list_del(&thread->ready_queue_node);
    thread->thread_ctx->state = TS_INTER;
    return 0;
}

/*
 * Lab4
 * The helper function
 * Choose an appropriate thread and dequeue from ready queue
 *
 * If there is no ready thread in the current CPU's ready queue,
 * choose the idle thread of the CPU.
 *
 * Do not forget to check the type and
 * state of the chosen thread
 */
struct thread *rr_sched_choose_thread(void) {
    u32 cpu = smp_get_cpu_id();
    if (list_empty(&rr_ready_queue[cpu])) {
        // if the ready queue is empty, return the idle thread
        return &idle_threads[cpu];
    }
    struct thread *target =
        container_of(rr_ready_queue[cpu].next, struct thread, ready_queue_node);
    int err = rr_sched_dequeue(target);
    if (err != 0 && err != -2) {
        kwarn("rr_sched_dequeue returns non-zero in rr_sched_choose_thread\n");
    }
    return target;
}

static inline void rr_sched_refill_budget(struct thread *target, u32 budget) {
    if (target && target->thread_ctx) target->thread_ctx->sc->budget = budget;
}

/*
 * Lab4
 * Schedule a thread to execute.
 * This function will suspend current running thread, if any, and schedule
 * another thread from `rr_ready_queue[cpu_id]`.
 *
 * Hints:
 * Macro DEFAULT_BUDGET defines the value for resetting thread's budget.
 * After you get one thread from rr_sched_choose_thread, pass it to
 * switch_to_thread() to prepare for switch_context().
 * Then ChCore can call eret_to_thread() to return to user mode.
 */
int rr_sched(void) {
    if (current_thread && current_thread->thread_ctx &&
        current_thread->thread_ctx->sc->budget > 0) {
        kinfo("no schedule: budget=%u\n",
              current_thread->thread_ctx->sc->budget);
        return -1;
    }
    // check if cpu is running some thread
    if (current_thread && current_thread != &idle_threads[smp_get_cpu_id()]) {
        // Some thread is running, add it to queue
        rr_sched_enqueue(current_thread);
        current_thread->thread_ctx->state = TS_READY;
    }
    // Choose a thread
    struct thread *target_thread = rr_sched_choose_thread();
    rr_sched_refill_budget(target_thread, DEFAULT_BUDGET);
    // kdebug("rr_sched: pause %lx, run %lx\n", current_thread, target_thread);
    switch_to_thread(target_thread);
    return 0;
}

/*
 * Initialize the per thread queue and idle thread.
 */
int rr_sched_init(void) {
    int i = 0;

    /* Initialize global variables */
    for (i = 0; i < PLAT_CPU_NUM; i++) {
        current_threads[i] = NULL;
        init_list_head(&rr_ready_queue[i]);
    }

    /* Initialize one idle thread for each core and insert into the RQ */
    for (i = 0; i < PLAT_CPU_NUM; i++) {
        /* Set the thread context of the idle threads */
        BUG_ON(!(idle_threads[i].thread_ctx = create_thread_ctx()));
        /* We will set the stack and func ptr in arch_idle_ctx_init */
        init_thread_ctx(&idle_threads[i], 0, 0, MIN_PRIO, TYPE_IDLE, i);
        /* Call arch-dependent function to fill the context of the idle
         * threads */
        arch_idle_ctx_init(idle_threads[i].thread_ctx, idle_thread_routine);
        /* Idle thread is kernel thread which do not have vmspace */
        idle_threads[i].vmspace = NULL;
    }
    kdebug("Scheduler initialized. Create %d idle threads.\n", i);

    return 0;
}

/*
 * Lab4
 * Handler called each time a timer interrupt is handled
 * Do not forget to call sched_handle_timer_irq() in proper code location.
 */
void rr_sched_handle_timer_irq(void) {
    struct thread *current = current_thread;
    if (current) {
        if (current->thread_ctx->sc->budget > 0)
            current->thread_ctx->sc->budget--;
    }
}

struct sched_ops rr = {
    .sched_init = rr_sched_init,
    .sched = rr_sched,
    .sched_enqueue = rr_sched_enqueue,
    .sched_dequeue = rr_sched_dequeue,
    .sched_choose_thread = rr_sched_choose_thread,
    .sched_handle_timer_irq = rr_sched_handle_timer_irq,
};
