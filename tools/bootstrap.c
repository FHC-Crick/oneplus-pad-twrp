/* Map probe: mount sdc5, write a numbered pong file into EVERY candidate path
 * (plus root and a few subdirs), then reboot. Whichever pong shows up in the
 * stock system reveals the exact path mapping. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static void do_reboot(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x01234567, NULL);
}

static void do_poweroff(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x4321fedc, NULL);
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);

    struct stat st;
    long m = -1;
    if (stat("/dev/sdc5", &st) == 0)
        m = syscall(SYS_mount, "/dev/sdc5", "/plog", "ext4", 0, "sync");
    else if (stat("/dev/block/sdc5", &st) == 0)
        m = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync");
    if (m != 0) do_poweroff();

    const char *cands[] = {
        "",                       /* 0: sdc5 root */
        "/TWRL",                  /* 1: fresh dir we create */
        "/oplus_fsck_fulldiskscanuserdata", /* 2 */
        "/cache",                 /* 3 */
        "/cache/factory",         /* 4 */
        "/TMP",                   /* 5 */
        "/pre_watchdog",          /* 6 */
        "/camera",                /* 7 */
        "/oplusreserve",          /* 8 */
        "/storage",               /* 9 */
        "/storage/vold",          /* 10 */
        "/DCS",                   /* 11 */
        "/backup",                /* 12 */
        "/config",                /* 13 */
        "/olc",                   /* 14 */
        "/stamp",                 /* 15 */
        "/theia",                 /* 16 */
        "/sf",                    /* 17 */
        "/postman",               /* 18 */
    };
    int n = sizeof(cands) / sizeof(cands[0]);
    char dir[256], path[512];
    for (int i = 0; i < n; i++) {
        snprintf(dir, sizeof(dir), "/plog%s", cands[i]);
        mkdir(dir, 0777);  /* ensure exists (no-op if already) */
        snprintf(path, sizeof(path), "%s/pong-%02d.txt", dir, i);
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) {
            char msg[64];
            int len = snprintf(msg, sizeof(msg), "PONG %02d path=%s\n", i, cands[i]);
            write(fd, msg, len);
            fsync(fd);
            close(fd);
        }
    }
    /* marker presence check for the record (drives a distinct delay) */
    int found = 0;
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "/plog%s/marker.txt", cands[i]);
        if (stat(path, &st) == 0) { found = i + 1; break; }
    }
    if (found == 0) {
        /* nothing seen: still reboot so we can re-enter the system */
        do_reboot();
    }
    do_reboot();
    for (;;) pause();
    return 0;
}
