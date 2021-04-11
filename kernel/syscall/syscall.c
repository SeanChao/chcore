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

#include <common/kmalloc.h>
#include <common/kprint.h>
#include <common/mm.h>
#include <common/types.h>
#include <common/uaccess.h>
#include <common/uart.h>

#include "syscall_num.h"

void sys_debug(long arg) {
    kinfo("[syscall] sys_debug: %lx\n", arg);
}

void sys_putc(char ch) {
    /*
     * Lab3: Your code here
     * Send ch to the screen in anyway as your like
     */
    // printk("%c", ch);
    uart_send(ch);
}

/*
 * Lab3: Your code here
 * Update the syscall table as you like to redirect syscalls
 * to functions accordingly
 */
const void *syscall_table[NR_SYSCALL] = {
    [21 ... NR_SYSCALL - 1] = sys_debug,
    [SYS_putc] = sys_putc,
    [SYS_getc] = sys_debug,
    [SYS_yield] = sys_debug,
    [SYS_exit] = sys_exit,
    [SYS_sleep] = sys_debug,
    [SYS_create_pmo] = sys_create_pmo,
    [SYS_map_pmo] = sys_map_pmo,
    [SYS_create_thread] = sys_debug,
    [SYS_create_process] = sys_debug,
    [SYS_register_server] = sys_debug,
    [SYS_register_client] = sys_debug,
    [SYS_get_conn_stack] = sys_debug,
    [SYS_ipc_call] = sys_debug,
    [SYS_ipc_return] = sys_debug,
    [SYS_cap_copy_to] = sys_debug,
    [SYS_cap_copy_from] = sys_debug,
    [SYS_unmap_pmo] = sys_debug,
    [SYS_set_affinity] = sys_debug,
    [SYS_get_affinity] = sys_debug,
    [SYS_create_device_pmo] = sys_debug,
	[SYS_handle_brk] = sys_handle_brk,
    /* lab3 syscalls finished */
};
