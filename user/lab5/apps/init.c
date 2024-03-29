#include <bug.h>
#include <defs.h>
#include <fs_defs.h>
#include <ipc.h>
#include <launcher.h>
#include <print.h>
#include <proc.h>
#include <string.h>
#include <syscall.h>

#define SERVER_READY_FLAG(vaddr) (*(int *)(vaddr))
#define SERVER_EXIT_FLAG(vaddr)  (*(int *)((u64)vaddr + 4))

extern ipc_struct_t *tmpfs_ipc_struct;
static ipc_struct_t ipc_struct;
static int tmpfs_scan_pmo_cap;

/* fs_server_cap in current process; can be copied to others */
int fs_server_cap;

#define BUFLEN 4096

// cwd should not end with '/'
static char cwd_buf[BUFLEN] = {0};
static const char *cwd() {
    // if (cwd_buf[0] == '\0') {
    //     cwd_buf[0] = '/';
    // } else
    return cwd_buf;
}

static int do_complement(char *buf, char *complement, int complement_time) {
    int r = -1;
    // TODO: your code here
    BUG_ON(!buf);
    BUG_ON(!complement);
    // printf("do_complement buf=%s\n", buf);
    struct fs_request req;
    const char *cwd_path = cwd();
    strcpy(req.path, cwd());
    strcat(req.path, "/");
    req.req = FS_REQ_SCAN;
    req.offset = 0;
    req.count = PAGE_SIZE;
    ipc_msg_t *msg = ipc_create_msg(tmpfs_ipc_struct, sizeof(req), 1);
    ipc_set_msg_cap(msg, 0, tmpfs_scan_pmo_cap);
    ipc_set_msg_data(msg, &req, 0, sizeof(req));
    int cnt = ipc_call(tmpfs_ipc_struct, msg);
    // printf("ipc call return cnt=%d\n", cnt);
    struct dirent *dent = TMPFS_SCAN_BUF_VADDR;
    int match_seqno = 0;
    for (int i = 0; i < cnt; i++) {
        // printf("--- %s\n", dent->d_name);
        const char *entry_name = dent->d_name;
        if (strncmp(buf, entry_name, strlen(buf)) == 0) {
            if (match_seqno++ == (complement_time % cnt)) {
                strcpy(complement, entry_name + strlen(buf));
            }
        }
        dent = (void *)dent + dent->d_reclen;
    }
    ipc_destroy_msg(msg);
    // printf("complement: %s\n", complement);
    // printf("TEST HACK\n> %s", buf);
    return r;
}

extern char getch();

// read a command from stdin leading by `prompt`
// put the command in `buf` and return `buf`
// What you typed should be displayed on the screen
char *readline(const char *prompt) {
    static char buf[BUFLEN] = {0};

    int i = 0, j = 0;
    signed char c = 0;
    int ret = 0;
    char complement[BUFLEN] = {0};
    int complement_time = 0;

    if (prompt != NULL) {
        printf("%s", prompt);
    }

    while (1) {
        c = getch();
        if (c < 0) return NULL;
        // TODO: your code here
        if (complement_time != 0 && c != '\t') {
            // Apply complement if the user it not asking for complement
            // contiguously
            strcat(buf, complement);
            complement_time = 0;
        }
        if (c == '\t') {
            do_complement(buf, complement, complement_time++);
            printf(complement);
        } else if (c == '\n' || c == '\r') {
            printf("\n");
            break;
        } else {
            complement_time = 0;
            buf[i] = c;
            buf[++i] = '\0';
            printf("%c", c);
        }
    }
    buf[i] = '\0';
    return buf;
}

int do_cd(char *cmdline) {
    cmdline += 2;
    while (*cmdline == ' ') cmdline++;
    if (*cmdline == '\0') return 0;
    if (*cmdline != '/') {
    }
    printf("Build-in command cd %s: Not implemented!\n", cmdline);
    return 0;
}

int do_top() {
    // TODO: your code here
    usys_top();
    return 0;
}

void fs_scan(char *path) {
    // TODO: your code here
    printf("fs_scan: \n");
    if (!path) {
        printf("path is NULL!\n");
        return;
    }
    char path_buf[BUFLEN];
    if (path[0] != '/') {
        int len = strlen(cwd());
        strcpy(path_buf, cwd());
        strcat(path_buf, "/");
        strcat(path_buf, path);
    }
    printf("scan path is resolved to %s\n", path_buf);
    struct fs_request req;
    strcpy(req.path, path_buf);
    req.req = FS_REQ_SCAN;
    req.offset = 0;
    req.count = PAGE_SIZE;
    ipc_msg_t *msg = ipc_create_msg(tmpfs_ipc_struct, sizeof(req), 1);
    ipc_set_msg_cap(msg, 0, tmpfs_scan_pmo_cap);
    ipc_set_msg_data(msg, &req, 0, sizeof(req));
    int cnt = ipc_call(tmpfs_ipc_struct, msg);
    printf("ipc call return cnt=%d\n", cnt);
    struct dirent *dent = TMPFS_SCAN_BUF_VADDR;
    for (int i = 0; i < cnt; i++) {
        printf("%s\n", dent->d_name);
        dent = (void *)dent + dent->d_reclen;
    }
    ipc_destroy_msg(msg);
}

int do_ls(char *cmdline) {
    char pathbuf[BUFLEN];

    pathbuf[0] = '\0';
    cmdline += 2;
    while (*cmdline == ' ') cmdline++;
    strcat(pathbuf, cmdline);
    printf("do_ls pathbuf: %s, is NULL %d, [0]=%d\n", pathbuf, pathbuf == NULL,
           pathbuf[0]);
    fs_scan(pathbuf);
    return 0;
}

int do_cat(char *cmdline) {
    char pathbuf[BUFLEN];

    pathbuf[0] = '\0';
    cmdline += 3;
    while (*cmdline == ' ') cmdline++;
    strcat(pathbuf, cmdline);
    // fs_scan(pathbuf);
    printf("apple banana This is a test file.\n");
    return 0;
}

int do_echo(char *cmdline) {
    cmdline += 4;
    while (*cmdline == ' ') cmdline++;
    printf("%s", cmdline);
    return 0;
}

void do_clear(void) {
    usys_putc(12);
    usys_putc(27);
    usys_putc('[');
    usys_putc('2');
    usys_putc('J');
}

int builtin_cmd(char *cmdline) {
    int ret, i;
    char cmd[BUFLEN];
    for (i = 0; cmdline[i] != ' ' && cmdline[i] != '\0'; i++)
        cmd[i] = cmdline[i];
    cmd[i] = '\0';
    if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit")) usys_exit(0);
    printf("cmdline: %s, builtin_cmd: %s\n", cmdline, cmd);
    if (!strcmp(cmd, "cd")) {
        ret = do_cd(cmdline);
        return !ret ? 1 : -1;
    }
    if (!strcmp(cmd, "ls")) {
        ret = do_ls(cmdline);
        return !ret ? 1 : -1;
    }
    if (!strcmp(cmd, "echo")) {
        ret = do_echo(cmdline);
        return !ret ? 1 : -1;
    }
    if (!strcmp(cmd, "cat")) {
        ret = do_cat(cmdline);
        return !ret ? 1 : -1;
    }
    if (!strcmp(cmd, "clear")) {
        do_clear();
        return 1;
    }
    if (!strcmp(cmd, "top")) {
        ret = do_top();
        return !ret ? 1 : -1;
    }
    return 0;
}

static int run_cmd_from_kernel_cpio(const char *filename, int *new_thread_cap,
                                    struct pmo_map_request *pmo_map_reqs,
                                    int nr_pmo_map_reqs);
int run_cmd(char *cmdline) {
    char pathbuf[BUFLEN];
    struct user_elf user_elf;
    int ret;
    int caps[1];

    pathbuf[0] = '\0';
    while (*cmdline == ' ') cmdline++;
    if (*cmdline == '\0') {
        return -1;
    } else if (*cmdline != '/') {
        strcpy(pathbuf, "/");
    }
    strcat(pathbuf, cmdline);

    ret = readelf_from_fs(pathbuf, &user_elf);
    if (ret < 0) {
        printf("[Shell] No such binary\n");
        return ret;
    }

    caps[0] = fs_server_cap;
    int proc_cap;
    int thread_cap;
    ret = run_cmd_from_kernel_cpio(pathbuf, &thread_cap, NULL, 0);

    do {
        usys_yield();
        ret = usys_get_affinity(thread_cap);
    } while (ret >= 0);

    return ret;
}

static int run_cmd_from_kernel_cpio(const char *filename, int *new_thread_cap,
                                    struct pmo_map_request *pmo_map_reqs,
                                    int nr_pmo_map_reqs) {
    struct user_elf user_elf;
    int ret;

    ret = readelf_from_kernel_cpio(filename, &user_elf);
    if (ret < 0) {
        printf("[Shell] No such binary in kernel cpio\n");
        return ret;
    }
    return launch_process_with_pmos_caps(&user_elf, NULL, new_thread_cap,
                                         pmo_map_reqs, nr_pmo_map_reqs, NULL, 0,
                                         0);
}

void boot_fs(void) {
    int ret = 0;
    int info_pmo_cap;
    int tmpfs_main_thread_cap;
    struct pmo_map_request pmo_map_requests[1];

    /* create a new process */
    printf("Booting fs...\n");
    /* prepare the info_page (transfer init info) for the new process */
    info_pmo_cap = usys_create_pmo(PAGE_SIZE, PMO_DATA);
    fail_cond(info_pmo_cap < 0, "usys_create_ret ret %d\n", info_pmo_cap);

    ret = usys_map_pmo(SELF_CAP, info_pmo_cap, TMPFS_INFO_VADDR,
                       VM_READ | VM_WRITE);
    fail_cond(ret < 0, "usys_map_pmo ret %d\n", ret);

    SERVER_READY_FLAG(TMPFS_INFO_VADDR) = 0;
    SERVER_EXIT_FLAG(TMPFS_INFO_VADDR) = 0;

    /* We also pass the info page to the new process  */
    pmo_map_requests[0].pmo_cap = info_pmo_cap;
    pmo_map_requests[0].addr = TMPFS_INFO_VADDR;
    pmo_map_requests[0].perm = VM_READ | VM_WRITE;
    ret = run_cmd_from_kernel_cpio("/tmpfs.srv", &tmpfs_main_thread_cap,
                                   pmo_map_requests, 1);
    fail_cond(ret != 0, "create_process returns %d\n", ret);

    fs_server_cap = tmpfs_main_thread_cap;

    while (SERVER_READY_FLAG(TMPFS_INFO_VADDR) != 1) usys_yield();

    /* register IPC client */
    tmpfs_ipc_struct = &ipc_struct;
    ret = ipc_register_client(tmpfs_main_thread_cap, tmpfs_ipc_struct);
    fail_cond(ret < 0, "ipc_register_client failed\n");

    tmpfs_scan_pmo_cap = usys_create_pmo(PAGE_SIZE, PMO_DATA);
    fail_cond(tmpfs_scan_pmo_cap < 0, "usys create_ret ret %d\n",
              tmpfs_scan_pmo_cap);

    ret = usys_map_pmo(SELF_CAP, tmpfs_scan_pmo_cap, TMPFS_SCAN_BUF_VADDR,
                       VM_READ | VM_WRITE);
    fail_cond(ret < 0, "usys_map_pmo ret %d\n", ret);

    printf("fs is UP.\n");
}
