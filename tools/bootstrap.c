/* Minimal probe bootstrap: mounts sdc5, writes log to EVERY candidate path,
 * checks which path sees the shell-written marker.txt, reads vendor_boot and
 * stops (no unpack/pivot/exec). Logs stay on disk for offline inspection. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int fds[8];
static int nfds = 0;

static const char *paths[] = {
    "/plog/twrp.log",
    "/plog/oplus_fsck_fulldiskscanuserdata/twrp.log",
    "/plog/cache/twrp.log",
    "/plog/TMP/twrp.log",
    "/plog/pre_watchdog/twrp.log",
    "/plog/oplusreserve/twrp.log",
    "/plog/storage/twrp.log",
    "/plog/camera/twrp.log",
};

static void L(const char *s) {
    for (int i = 0; i < nfds; i++) {
        if (fds[i] >= 0) {
            write(fds[i], s, strlen(s));
            fsync(fds[i]);
        }
    }
}

static void LN(long v) {
    char b[32];
    char *p = b + 30;
    *p = '\n';
    *--p = 0;
    if (v == 0) *--p = '0';
    else {
        unsigned long u = v < 0 ? (unsigned long)(-v) : (unsigned long)v;
        while (u) { *--p = '0' + u % 10; u /= 10; }
        if (v < 0) *--p = '-';
    }
    L(p);
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);
    long m5 = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync");

    for (int i = 0; i < 8; i++) {
        fds[i] = open(paths[i], O_WRONLY | O_CREAT | O_APPEND, 0666);
        nfds = i + 1;
    }
    L("=== minimal probe ===\n");
    L("mount sdc5 rc: "); LN(m5);
    for (int i = 0; i < 8; i++) {
        L("path "); LN(i);
        L("  opened fd: "); LN(fds[i]);
    }
    /* marker visibility per candidate dir */
    for (int i = 0; i < 8; i++) {
        char dir[256], mk[300];
        strncpy(dir, paths[i], sizeof(dir));
        char *slash = strrchr(dir, '/');
        if (slash) *slash = 0;
        snprintf(mk, sizeof(mk), "%s/marker.txt", dir);
        struct stat st;
        int r = stat(mk, &st);
        L("marker at "); L(dir); L(" rc: "); LN(r);
    }
    /* read vendor_boot */
    int fd = open("/dev/block/sdc41", O_RDONLY);
    L("open sdc41 fd: "); LN(fd);
    if (fd >= 0) {
        lseek(fd, 4096, SEEK_SET);
        unsigned char *buf = malloc(52 * 1024 * 1024);
        L("malloc: "); LN(buf ? 1 : 0);
        if (buf) {
            ssize_t got = read(fd, buf, 52 * 1024 * 1024);
            L("read bytes: "); LN(got);
            if (got > 6) {
                char mg[8];
                memcpy(mg, buf, 6);
                mg[6] = 0;
                L("magic: "); L(mg); L("\n");
            }
        }
    }
    L("=== probe done, hanging ===\n");
    for (;;) pause();
    return 0;
}
