/**
 * Simple Pass-Through File System:
 * gcc -Wall ptfs.c `pkg-config fuse --cflags --libs` -o ptfs
 *
 */


#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#define _XOPEN_SOURCE
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/types.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <fuse.h>

#define MAX_PATHLEN 255

struct pt_dirp
{
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

// TODO:
/* root of the filesystem */
char *root;

static inline void sanitize_path(const char* path, char *outpath)
{
    if (!path) {
        outpath = root;
    }
    else {
        snprintf(outpath, MAX_PATHLEN, "%s%s", root, path);
    }
    fprintf(stdout, "DEBUG sanitize_path: (%s)\n", outpath);
}

static int pt_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = lstat(p, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_access(const char *path, int mask)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = access(p, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = readlink(p, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int pt_opendir(const char *path, struct fuse_file_info *fi)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    struct pt_dirp *dir = (struct pt_dirp*) malloc(sizeof(struct pt_dirp));
    if (dir == NULL)
        return -ENOMEM;

    dir->dp = opendir(p);
    if (dir->dp == NULL) {
        res = -errno;
        free(dir);
        return res;
    }
    dir->offset = 0;
    dir->entry = NULL;
    fi->fh = (unsigned long) dir;
    return 0;
}

static int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi)
{

    struct pt_dirp *d = (struct pt_dirp*) fi->fh;

    (void) path;
    if (offset != d->offset) {
        seekdir(d->dp, offset);
        d->entry = NULL;
        d->offset = offset;
    }

    while ((d->entry = readdir(d->dp)) != NULL) {
        struct stat st;
        off_t nextoff;
        memset(&st, 0, sizeof(st));
        st.st_ino = d->entry->d_ino;
        st.st_mode = d->entry->d_type << 12;
        nextoff = telldir(d->dp);
        if (filler(buf, d->entry->d_name, &st, nextoff)) {
            break;
        }
        d->entry = NULL;
        d->offset = nextoff;
    }

    return 0;
}

static int pt_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct pt_dirp *d = (struct pt_dirp*) fi->fh;
    (void) path;
    closedir(d->dp);
    free(d);
    return 0;
}

static int pt_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    if (S_ISREG(mode)) {
        res = open(p, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode))
        res = mkfifo(p, mode);
    else
        res = mknod(p, mode, rdev);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_mkdir(const char *path, mode_t mode)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = mkdir(p, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_unlink(const char *path)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = unlink(p);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_rmdir(const char *path)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = rmdir(p);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_symlink(const char *from, const char *to)
{
    int res;
    char f[MAX_PATHLEN];
    char t[MAX_PATHLEN];
    sanitize_path(from, f);
    sanitize_path(to, t);

    res = symlink(f, t);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_rename(const char *from, const char *to, unsigned int flags)
{
    int res;
    char f[MAX_PATHLEN];
    char t[MAX_PATHLEN];
    sanitize_path(from, f);
    sanitize_path(to, t);

    if (flags)
        return -EINVAL;

    res = rename(f, t);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_link(const char *from, const char *to)
{
    int res;
    char f[MAX_PATHLEN];
    char t[MAX_PATHLEN];
    sanitize_path(from, f);
    sanitize_path(to, t);

    res = link(f, t);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_chmod(const char *path, mode_t mode)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = chmod(p, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = lchown(p, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_truncate(const char *path, off_t size)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = truncate(p, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_ftruncate(const char* path, off_t size, struct fuse_file_info *fi)
{
    int res;
    (void) path;
    res = ftruncate(fi->fh, size);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_UTIMENSAT
static int pt_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    /* don't use utime/utimes since they follow symlinks */
    res = utimensat(0, p, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;

    return 0;
}
#endif

static int pt_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = open(p, fi->flags, mode);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int pt_open(const char *path, struct fuse_file_info *fi)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = open(p, fi->flags);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int pt_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi)
{
    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int pt_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi)
{
    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int pt_statfs(const char *path, struct statvfs *stbuf)
{
    int res;
    char p[MAX_PATHLEN];
    sanitize_path(path, p);

    res = statvfs(p, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_flush(const char* path, struct fuse_file_info *fi)
{
    int res;
    (void) path;

    res = close(dup(fi->fh));
    if (res == -1)
        return -errno;

    return 0;
}

static int pt_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);

    return 0;
}

static int pt_fsync(const char *path, int isdatasync,
        struct fuse_file_info *fi)
{
    int res;
    (void) path;
    (void) isdatasync;

    res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int pt_fallocate(const char *path, int mode,
        off_t offset, off_t length, struct fuse_file_info *fi)
{

    (void) path;

    if (mode)
        return -EOPNOTSUPP;

    res = -posix_fallocate(fi->fh, offset, length);
    return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int pt_setxattr(const char *path, const char *name, const char *value,
        size_t size, int flags)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = lsetxattr(p, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int pt_getxattr(const char *path, const char *name, char *value,
        size_t size)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = lgetxattr(p, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int pt_listxattr(const char *path, char *list, size_t size)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = llistxattr(p, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int pt_removexattr(const char *path, const char *name)
{
    char p[MAX_PATHLEN];
    sanitize_path(path, p);
    int res = lremovexattr(p, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static int pt_flock(const char* path, struct fuse_file_info *fi, int op)
{
    int res;
    (void) path;

    res = flock(fi->fh, op);
    if (res == -1)
        return -errno;

    return 0;
}

static struct fuse_operations pt_oper = {
    .getattr	= pt_getattr,
    .access		= pt_access,
    .readlink	= pt_readlink,
    .opendir    = pt_opendir,
    .readdir	= pt_readdir,
    .mknod		= pt_mknod,
    .mkdir		= pt_mkdir,
    .symlink	= pt_symlink,
    .unlink		= pt_unlink,
    .rmdir		= pt_rmdir,
    .rename		= pt_rename,
    .link		= pt_link,
    .chmod		= pt_chmod,
    .chown		= pt_chown,
    .truncate	= pt_truncate,
#ifdef HAVE_UTIMENSAT
    .utimens	= pt_utimens,
#endif
    .create     = pt_create,
    .open		= pt_open,
    .read		= pt_read,
    .write		= pt_write,
    .statfs		= pt_statfs,
    .flush      = pt_flush,
    .release	= pt_release,
    .fsync		= pt_fsync,
#ifdef HAVE_POSIX_FALLOCATE
    .fallocate	= pt_fallocate,
#endif
#ifdef HAVE_SETXATTR
    .setxattr	= pt_setxattr,
    .getxattr	= pt_getxattr,
    .listxattr	= pt_listxattr,
    .removexattr	= pt_removexattr,
#endif
    .flock = pt_flock,
};


struct pt_opt_struct {
    unsigned long val;
    char * str;
} pt_opts;


/*
 * option parsing callback
 * return -1 indicates an error
 * return 0 accepts the parameter
 * return 1 retain the parameter to fuse
 */
int pt_opt_proc(void *data, const char* arg, int key, struct fuse_args *outargs)
{
    switch(key)
    {
        case FUSE_OPT_KEY_NONOPT:
            if (!root) {
                root = NULL;
                root = realpath(arg, root);
                return 0;
            }
            return 1;
        default: /* else pass to fuse */
            return 1;
    }
}


int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int res;

    //TODO:
    pt_opts.val = 0;
    pt_opts.str = NULL;

    if (fuse_opt_parse(&args, &pt_opts, NULL, pt_opt_proc) == -1) {
        perror("error on fuse_opt_parse");
        exit(1);
    }
    else {
        printf("arguments to fuse_main: ");
        for (int i=0; i < args.argc; i++) {
            printf("%s ", args.argv[i]);
        }
        printf("\n");
        printf("Demo parameters in pt_opts: val= %lu, str=", pt_opts.val);
        if (pt_opts.str) {
            printf(" %s\n", pt_opts.str);
        }
        else {
            printf(" NULL\n");
        }
        if (root) {
            printf("root: %s\n", root);
        }
        else {
            printf("no root!\n");
        }
    }

    umask(0);
    res = fuse_main(args.argc, args.argv, &pt_oper, NULL);

    fuse_opt_free_args(&args);
    if (root) {
        free(root);
    }

    return res;
}
