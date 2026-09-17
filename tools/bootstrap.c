/* Diagnostic full bootstrapper for OPD2407.
 * Every step is logged to sdc5 (persist) at a path readable from the stock
 * system: /data/persist_log/oplus_fsck_fulldiskscanuserdata/twrp.log
 * TWRP init's stdout/stderr are redirected to the same log.
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SEG_OFFSET 4096
#define SEG_SIZE (52 * 1024 * 1024)
#define LOGPATH "/plog/oplus_fsck_fulldiskscanuserdata/twrp.log"

static int logfd = -1;
static int con = -1;

static void conput(const char *s) {
    if (con < 0) {
        con = open("/dev/console", O_WRONLY);
        if (con < 0) con = open("/dev/tty0", O_WRONLY);
    }
    if (con >= 0) write(con, s, strlen(s));
}

static void L(const char *s) {
    if (logfd >= 0) {
        write(logfd, s, strlen(s));
        fsync(logfd);
    }
}

static void LN(long v) {
    char buf[32];
    char *p = buf + 30;
    *p = '\n';
    *--p = 0;
    if (v == 0) { *--p = '0'; }
    else {
        unsigned long u = v < 0 ? (unsigned long)(-v) : (unsigned long)v;
        while (u) { *--p = '0' + u % 10; u /= 10; }
        if (v < 0) *--p = '-';
    }
    L(p);
}

static unsigned hx(const unsigned char *p) {
    unsigned v = 0;
    for (int i = 0; i < 8; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    }
    return v;
}

static void make_path(const char *root, const char *name) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/%s", root, name);
    char *p = buf + 1;
    while (*p) {
        if (*p == '/') {
            *p = 0;
            mkdir(buf, 0755);
            *p = '/';
        }
        p++;
    }
}

static int unpack_cpio(const unsigned char *d, size_t size, const char *root) {
    const unsigned char *p = d, *end = d + size;
    char name[512], full[1024];
    int files = 0;
    while (p + 110 <= end) {
        if (memcmp(p, "070701", 6) != 0) {
            L("  bad magic at file #"); LN(files); return -1;
        }
        unsigned mode = hx(p + 14);
        unsigned filesize = hx(p + 54);
        unsigned rdevmaj = hx(p + 78);
        unsigned rdevmin = hx(p + 86);
        unsigned namesize = hx(p + 94);
        if (namesize == 0 || namesize > 500) { L("  bad namesize\n"); return -2; }
        if (p + 110 + namesize + filesize > end) { L("  overflow\n"); return -3; }
        memcpy(name, p + 110, namesize - 1);
        name[namesize - 1] = 0;
        if (strcmp(name, "TRAILER!!!") == 0) { L("  files unpacked: "); LN(files); return 0; }
        if (strncmp(name, "./", 2) == 0) memmove(name, name + 2, strlen(name) - 1);
        snprintf(full, sizeof(full), "%s/%s", root, name);
        make_path(root, name);
        const unsigned char *data = p + 110 + namesize;
        if (S_ISDIR(mode)) {
            mkdir(full, mode & 07777);
        } else if (S_ISLNK(mode)) {
            char tgt[512];
            unsigned tl = filesize < 511 ? filesize : 511;
            memcpy(tgt, data, tl);
            tgt[tl] = 0;
            symlink(tgt, full);
        } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
            mknod(full, mode, makedev(rdevmaj, rdevmin));
        } else if (S_ISREG(mode)) {
            int f = open(full, O_WRONLY | O_CREAT | O_TRUNC, mode & 07777);
            if (f >= 0) {
                size_t off = 0;
                while (off < filesize) {
                    ssize_t w = write(f, data + off, filesize - off);
                    if (w <= 0) break;
                    off += w;
                }
                close(f);
                chmod(full, mode & 07777);
            }
        }
        files++;
        size_t total = 110 + namesize + filesize;
        p += total + ((4 - total % 4) % 4);
    }
    L("  ran off end, files: "); LN(files);
    return -4;
}

int main(void) {
    conput("BOOTSTRAP DIAG\n");
    long m1 = syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    long m2 = syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    long m3 = syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);
    long m5 = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync");
    logfd = open(LOGPATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    L("=== diag run ===\n");
    L("mount proc/sys/dev/sdc5 rc: "); LN(m1); LN(m2); LN(m3); LN(m5);
    if (logfd < 0) {
        conput("LOG OPEN FAIL\n");
        for (;;) pause();
    }
    struct stat st;
    int mkr = stat("/plog/oplus_fsck_fulldiskscanuserdata/marker.txt", &st);
    L("marker stat rc: "); LN(mkr);

    int fd = open("/dev/block/sdc41", O_RDONLY);
    L("open sdc41: "); LN(fd);
    if (fd < 0) { L("FATAL no sdc41\n"); for (;;) pause(); }
    lseek(fd, SEG_OFFSET, SEEK_SET);
    unsigned char *buf = malloc(SEG_SIZE);
    L("malloc 52M: "); LN(buf ? 0 : -1);
    if (!buf) { L("FATAL malloc\n"); for (;;) pause(); }
    ssize_t got = read(fd, buf, SEG_SIZE);
    L("read vb bytes: "); LN(got);
    close(fd);
    {
        char mg[8];
        memcpy(mg, buf, 6);
        mg[6] = 0;
        L("cpio magic: "); L(mg); L("\n");
    }

    mkdir("/newroot", 0755);
    long mt = syscall(SYS_mount, "tmpfs", "/newroot", "tmpfs", 0, 0);
    L("mount newroot tmpfs: "); LN(mt);
    int cd = chdir("/newroot");
    L("chdir newroot: "); LN(cd);
    int rc = unpack_cpio(buf, (size_t)got, ".");
    L("unpack rc: "); LN(rc);

    long v1 = syscall(SYS_mount, NULL, "/proc", NULL, MS_MOVE, NULL);
    long v2 = syscall(SYS_mount, NULL, "/sys", NULL, MS_MOVE, NULL);
    long v3 = syscall(SYS_mount, NULL, "/dev", NULL, MS_MOVE, NULL);
    L("move proc/sys/dev: "); LN(v1); LN(v2); LN(v3);

    mkdir("/oldroot", 0755);
    long pr = syscall(SYS_pivot_root, ".", "./oldroot");
    L("pivot_root: "); LN(pr);
    if (pr == 0) {
        chdir("/");
        umount2("/oldroot", MNT_DETACH);
    }
    L("exec /init now\n");
    if (logfd >= 0) {
        dup2(logfd, 1);
        dup2(logfd, 2);
    }
    char *argv[] = {"/init", "second_stage", NULL};
    char *envp[] = {"HOME=/", "PATH=/sbin:/system/bin:/system/xbin", "ANDROID_ROOT=/system", NULL};
    execve("/init", argv, envp);
    L("EXEC FAILED\n");
    conput("EXEC FAILED\n");
    for (;;) pause();
    return 0;
}
