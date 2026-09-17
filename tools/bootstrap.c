/* Probe bootstrap v3: try both /dev/sdcX and /dev/block/sdcX paths, enumerate
 * /dev to learn real device node names, log to many candidate paths, then
 * ALWAYS reboot so lk falls back to slot b automatically (unattended safe). */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

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
    for (int i = 0; i < nfds; i++)
        if (fds[i] >= 0) { write(fds[i], s, strlen(s)); fsync(fds[i]); }
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

static void enum_dir(const char *d) {
    L("enum "); L(d); L(": ");
    DIR *dp = opendir(d);
    if (!dp) { L("FAILED\n"); return; }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(dp)) != NULL && n < 60) {
        L(e->d_name); L(" ");
        n++;
    }
    closedir(dp);
    L("\n");
}

static void do_reboot(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x01234567, NULL);
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);

    /* determine correct block device path */
    struct stat st;
    int a_plain = stat("/dev/sdc5", &st);
    int a_block = stat("/dev/block/sdc5", &st);
    int vb_plain = stat("/dev/sdc41", &st);
    int vb_block = stat("/dev/block/sdc41", &st);

    const char *sdc5 = (a_plain == 0) ? "/dev/sdc5" : ((a_block == 0) ? "/dev/block/sdc5" : NULL);

    mkdir("/plog", 0755);
    long m5 = -999;
    if (sdc5)
        m5 = syscall(SYS_mount, sdc5, "/plog", "ext4", 0, "sync");

    for (int i = 0; i < 8; i++)
        fds[i] = open(paths[i], O_WRONLY | O_CREAT | O_APPEND, 0666);
    nfds = 8;

    L("=== probe v3 ===\n");
    L("stat /dev/sdc5: "); LN(a_plain);
    L("stat /dev/block/sdc5: "); LN(a_block);
    L("stat /dev/sdc41: "); LN(vb_plain);
    L("stat /dev/block/sdc41: "); LN(vb_block);
    L("mount rc: "); LN(m5);
    for (int i = 0; i < 8; i++) { L("fd "); LN(i); L(" -> "); LN(fds[i]); }
    enum_dir("/dev");
    enum_dir("/dev/block");
    /* read vendor_boot via whichever path exists */
    const char *vb = (vb_plain == 0) ? "/dev/sdc41" : ((vb_block == 0) ? "/dev/block/sdc41" : NULL);
    if (vb) {
        int fd = open(vb, O_RDONLY);
        L("open vb fd: "); LN(fd);
        if (fd >= 0) {
            lseek(fd, 4096, SEEK_SET);
            unsigned char *buf = malloc(52 * 1024 * 1024);
            if (buf) {
                ssize_t got = read(fd, buf, 52 * 1024 * 1024);
                L("read bytes: "); LN(got);
                if (got > 6) {
                    char mg[8];
                    memcpy(mg, buf, 6);
                    mg[6] = 0;
                    L("magic: "); L(mg); L("\n");
                }
            } else {
                L("malloc failed\n");
            }
        }
    } else {
        L("no vendor_boot node found\n");
    }
    L("=== probe done, rebooting for auto-fallback ===\n");
    do_reboot();
    for (;;) pause();
    return 0;
}
