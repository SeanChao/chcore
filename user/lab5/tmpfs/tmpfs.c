#include "tmpfs.h"

#include <defs.h>
#include <launcher.h>
#include <print.h>
#include <string.h>
#include <syscall.h>

static struct inode *tmpfs_root;

/*
 * Helper functions to calucate hash value of string
 */
static inline u64 hash_chars(const char *str, ssize_t len) {
    u64 seed = 131; /* 31 131 1313 13131 131313 etc.. */
    u64 hash = 0;
    int i;

    if (len < 0) {
        while (*str) {
            hash = (hash * seed) + *str;
            str++;
        }
    } else {
        for (i = 0; i < len; ++i) hash = (hash * seed) + str[i];
    }

    return hash;
}

/* BKDR hash */
static inline u64 hash_string(struct string *s) {
    return (s->hash = hash_chars(s->str, s->len));
}

static inline int init_string(struct string *s, const char *name, size_t len) {
    int i;

    s->str = malloc(len + 1);
    if (!s->str) return -ENOMEM;
    s->len = len;

    for (i = 0; i < len; ++i) s->str[i] = name[i];
    s->str[len] = '\0';

    hash_string(s);
    return 0;
}

/*
 *  Helper functions to create instances of key structures
 */
static inline struct inode *new_inode(void) {
    struct inode *inode = malloc(sizeof(*inode));

    if (!inode) return ERR_PTR(-ENOMEM);

    inode->type = 0;
    inode->size = 0;

    return inode;
}

static struct inode *new_dir(void) {
    struct inode *inode;

    inode = new_inode();
    if (IS_ERR(inode)) return inode;
    inode->type = FS_DIR;
    init_htable(&inode->dentries, 1024);

    return inode;
}

static struct inode *new_reg(void) {
    struct inode *inode;

    inode = new_inode();
    if (IS_ERR(inode)) return inode;
    inode->type = FS_REG;
    init_radix_w_deleter(&inode->data, free);

    return inode;
}

static struct dentry *new_dent(struct inode *inode, const char *name,
                               size_t len) {
    struct dentry *dent;
    int err;

    dent = malloc(sizeof(*dent));
    if (!dent) return ERR_PTR(-ENOMEM);
    err = init_string(&dent->name, name, len);
    if (err) {
        free(dent);
        return ERR_PTR(err);
    }
    dent->inode = inode;

    return dent;
}

// this function create a file (directory if `mkdir` == true, otherwise regular
// file) and its size is `len`. You should create an inode and corresponding
// dentry, then add dentey to `dir`'s htable by `htable_add`.
// Assume that no separator ('/') in `name`.
static int tfs_mknod(struct inode *dir, const char *name, size_t len,
                     int mkdir) {
    struct inode *inode;
    struct dentry *dent;

    BUG_ON(!name);

    if (len == 0) {
        WARN("mknod with len of 0");
        return -ENOENT;
    }
    // TODO: write your code here
    // inode = new_inode();
    inode = mkdir ? new_dir() : new_reg();
    dent = new_dent(inode, name, len);
    htable_add(&dir->dentries, dent->name.hash, &dent->node);
    return 0;
}

/**
 * @param dir the parent directory inode
 * @param name filename
 * @param len the length of filename
 */
int tfs_mkdir(struct inode *dir, const char *name, size_t len) {
    return tfs_mknod(dir, name, len, 1 /* mkdir */);
}

/**
 * @param dir the parent directory inode
 * @param name filename
 * @param len the length of filename
 */
int tfs_creat(struct inode *dir, const char *name, size_t len) {
    printf("tfs_create %s, len=%lu\n", name, len);
    return tfs_mknod(dir, name, len, 0 /* mkdir */);
}

// look up a file called `name` under the inode `dir`
// and return the dentry of this file
static struct dentry *tfs_lookup(struct inode *dir, const char *name,
                                 size_t len) {
    printf("lookup %s in %p, len=%lu\n", name, dir, len);
    u64 hash = hash_chars(name, len);
    struct dentry *dent;
    struct hlist_head *head;

    head = htable_get_bucket(&dir->dentries, (u32)hash);

    for_each_in_hlist(dent, node, head) {
        if (dent->name.len == len && 0 == strcmp(dent->name.str, name))
            return dent;
    }
    printf("lookup NOT FOUND\n");
    return NULL;
}

// Walk the file system structure to locate a file with the pathname stored in
// `*name` and saves parent dir to `*dirat` and the filename to `*name`. If
// `mkdir_p` is true, you need to create intermediate directories when it
// missing. If the pathname `*name` starts with '/', then lookup starts from
// `tmpfs_root`, else from `*dirat`. Note that when `*name` ends with '/', the
// inode of last component will be saved in `*dirat` regardless of its type
// (e.g., even when it's FS_REG) and
// `*name` will point to '\0'
int tfs_namex(struct inode **dirat, const char **name, int mkdir_p) {
    BUG_ON(dirat == NULL);
    BUG_ON(name == NULL);
    BUG_ON(*name == NULL);

    printf("tfs_namex: name=%s\n", *name);

    // char buff[MAX_FILENAME_LEN + 1];
    char *name_ptr;
    int i;
    struct dentry *dent;
    int err;

    if (**name == '/') {
        *dirat = tmpfs_root;
        // make sure `name` starts with actual name
        while (**name && **name == '/') ++(*name);
        name_ptr = *name;
    } else {
        BUG_ON(*dirat == NULL);
        BUG_ON((*dirat)->type != FS_DIR);
        name_ptr = *name;
    }

    while (*name_ptr != '\0') {
        printf("loop name_ptr=%s\n", name_ptr);
        char *lookahead_ptr = name_ptr;
        while (*lookahead_ptr != '/' && *lookahead_ptr != '\0') lookahead_ptr++;
        printf("lookahead_ptr=%s, %d %d\n", lookahead_ptr, *lookahead_ptr,
               *lookahead_ptr == '\0');
        if (*lookahead_ptr == '\0') break;
        u64 len = lookahead_ptr - name_ptr;
        char *component = malloc(len + 1);
        memcpy(component, name_ptr, len);
        component[len] = '\0';

        struct htable dents = (*dirat)->dentries;
        u64 hash = hash_chars(component, len);
        bool exist = false;
        struct dentry *dent = tfs_lookup(*dirat, component, len);
        if (!dent && mkdir_p) {
            printf("namex mkdir %s under %p\n", component, *dirat);
            err = tfs_mkdir(*dirat, component,
                            len);  // len-1 to exclude the suffix '/'
            if (err) return err;
            dent = tfs_lookup(*dirat, component, len);
        }
        if (!dent) {
            return -EEXIST;
        }
        *dirat = dent->inode;
        name_ptr = lookahead_ptr + 1;
    }

    *name = name_ptr;
    printf("returned name:%s, dirat: %p\n", *name, *dirat);
    // make sure a child name exists
    if (!**name) return -EINVAL;
    return 0;
}

int tfs_remove(struct inode *dir, const char *name, size_t len) {
    u64 hash = hash_chars(name, len);
    struct dentry *dent, *target = NULL;
    struct hlist_head *head;

    BUG_ON(!name);

    if (len == 0) {
        WARN("mknod with len of 0");
        return -ENOENT;
    }

    head = htable_get_bucket(&dir->dentries, (u32)hash);

    for_each_in_hlist(dent, node, head) {
        if (dent->name.len == len && 0 == strcmp(dent->name.str, name)) {
            target = dent;
            break;
        }
    }

    if (!target) return -ENOENT;

    BUG_ON(!target->inode);

    // remove only when file is closed by all processes
    if (target->inode->type == FS_REG) {
        // free radix tree
        radix_free(&target->inode->data);
        // free inode
        free(target->inode);
        // remove dentry from parent
        htable_del(&target->node);
        // free dentry
        free(target);
    } else if (target->inode->type == FS_DIR) {
        if (!htable_empty(&target->inode->dentries)) return -ENOTEMPTY;

        // free htable
        htable_free(&target->inode->dentries);
        // free inode
        free(target->inode);
        // remove dentry from parent
        htable_del(&target->node);
        // free dentry
        free(target);
    } else {
        BUG("inode type that shall not exist");
    }

    return 0;
}

int init_tmpfs(void) {
    tmpfs_root = new_dir();
    printf("tmpfs_root is %p\n", tmpfs_root);
    return 0;
}

// write memory into `inode` at `offset` from `buf` for length is `size`
// it may resize the file
// `radix_get`, `radix_add` are used in this function
// You can use memory functions defined in libc
ssize_t tfs_file_write(struct inode *inode, off_t offset, const char *data,
                       size_t size) {
    BUG_ON(inode->type != FS_REG);
    BUG_ON(offset > inode->size);

    u64 page_no, page_off;
    u64 cur_off = offset;
    size_t to_write;
    void *page;
    struct radix *rt = &inode->data;
    BUG_ON(!rt);

    // TODO: write your code here
    page_off = offset % PAGE_SIZE;
    while (size > 0) {
        page_no = cur_off / PAGE_SIZE;
        void *page = radix_get(rt, page_no);
        u64 write_size = MIN(PAGE_SIZE - page_off, size);
        if (!page) {
            page = malloc(PAGE_SIZE);
            radix_add(rt, page_no, page);
            BUG_ON(!page);
            inode->size += write_size;
        }
        memcpy(page, data, write_size);
        cur_off += write_size;
        size -= write_size;
        page_off = 0;
    }

    return cur_off - offset;
}

// read memory from `inode` at `offset` in to `buf` for length is `size`, do not
// exceed the file size
// `radix_get` is used in this function
// You can use memory functions defined in libc
ssize_t tfs_file_read(struct inode *inode, off_t offset, char *buff,
                      size_t size) {
    BUG_ON(inode->type != FS_REG);
    BUG_ON(offset > inode->size);

    u64 page_no, page_off;
    u64 cur_off = offset;
    size_t to_read;
    void *page;
    size_t buff_idx = 0;

    struct radix *r = &inode->data;
    // printf("buff_idx %lu, size %lu, cur_off %lu sz %lu\n", buff_idx, size, cur_off , inode->size);
    while (buff_idx < size && cur_off < inode->size) {
        page_no = cur_off / PAGE_SIZE;
        page = radix_get(r, page_no);
        BUG_ON(!page);
        if (!page) break;
        u64 size_to_read = size - buff_idx;
        if (PAGE_SIZE - page_off < size_to_read)
            size_to_read = PAGE_SIZE - page_off;
        memcpy(buff + buff_idx, page + page_off, size_to_read);
        buff_idx += size_to_read;
        cur_off += size_to_read;
        page_off = 0;
    }
    // printf("tfs_file_read: cur_off=%lu, offset=%lu, ret=%lu\n", cur_off, offset,
    //        cur_off - offset);
    return cur_off - offset;
}

// load the cpio archive into tmpfs with the begin address as `start` in memory
// You need to create directories and files if necessary. You also need to write
// the data into the tmpfs.
int tfs_load_image(const char *start) {
    struct cpio_file *f;
    struct inode *dirat;
    struct dentry *dent;
    const char *leaf;
    size_t len;
    int err;
    ssize_t write_count;

    BUG_ON(start == NULL);

    cpio_init_g_files();
    cpio_extract(start, "/");

    dirat = malloc(sizeof(*dirat));
    for (f = g_files.head.next; f; f = f->next) {
        dirat = tmpfs_root;
        // TODO: Lab5: your code is here
        char *fname = f->name;
        char *filename = malloc(strlen(fname) + 2);
        strcpy(filename, fname);
        const char *data_ptr = f->data;

        if (strcmp(filename, ".") == 0) continue;
        // printf("filename: %s\n", filename);
        err = tfs_namex(&dirat, &filename, 1);
        if (err) {
            printf("tfsnamex err:%d\n", err);
            return err;
        }
        // printf("load_image: header.cmode is:%o\n", f->header.c_mode);
        // refer to https://www.freebsd.org/cgi/man.cgi?query=cpio&sektion=5
        u64 type = f->header.c_mode & 0170000;
        if (type == C_ISDIR) {
            // printf("load_image: mkdir %s under %p\n", filename, dirat);
            err = tfs_mkdir(dirat, filename, strlen(filename));
            if (err) return err;
        } else if (type == C_ISREG) {
            // printf("load_image: create %s under %p\n", filename, dirat);
            err = tfs_creat(dirat, filename, strlen(filename));
            // printf("create ok\n");
            if (err) return err;
            struct dentry *entry =
                tfs_lookup(dirat, filename, strlen(filename));
            // printf("lookup ok: entry %p\n", entry);
            if (!entry) return -EEXIST;
            int write_size =
                tfs_file_write(entry->inode, 0, f->data, f->header.c_filesize);
            // printf("write done, err=%d\n", err);
            if (write_size < f->header.c_filesize) return -233;
        }
    }

    return 0;
}

static int dirent_filler(void **dirpp, void *end, char *name, off_t off,
                         unsigned char type, ino_t ino) {
    struct dirent *dirp = *(struct dirent **)dirpp;
    void *p = dirp;
    unsigned short len = strlen(name) + 1 + sizeof(dirp->d_ino) +
                         sizeof(dirp->d_off) + sizeof(dirp->d_reclen) +
                         sizeof(dirp->d_type);
    p += len;
    if (p > end) return -EAGAIN;
    dirp->d_ino = ino;
    dirp->d_off = off;
    dirp->d_reclen = len;
    dirp->d_type = type;
    strcpy(dirp->d_name, name);
    *dirpp = p;
    return len;
}

int tfs_scan(struct inode *dir, unsigned int start, void *buf, void *end) {
    s64 cnt = 0;
    int b;
    int ret;
    ino_t ino;
    void *p = buf;
    unsigned char type;
    struct dentry *iter;

    for_each_in_htable(iter, b, node, &dir->dentries) {
        if (cnt >= start) {
            type = iter->inode->type;
            ino = iter->inode->size;
            ret = dirent_filler(&p, end, iter->name.str, cnt, type, ino);
            if (ret <= 0) {
                // printf("tfs_scan early return cnt=%ld, start=%u\n", cnt, start);
                return cnt - start;
            }
        }
        cnt++;
    }
    // printf("tfs_scan return cnt=%ld, start=%u\n", cnt, start);
    return cnt - start;
}

/* path[0] must be '/' */
struct inode *tfs_open_path(const char *path) {
    struct inode *dirat = NULL;
    const char *leaf = path;
    struct dentry *dent;
    int err;

    if (*path == '/' && !*(path + 1)) return tmpfs_root;

    err = tfs_namex(&dirat, &leaf, 0);
    printf("open_path dirat: %p\n", dirat);
    if (err) return NULL;

    dent = tfs_lookup(dirat, leaf, strlen(leaf));
    printf("open_path dent: %p\n", dent);
    return dent ? dent->inode : NULL;
}
