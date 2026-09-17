/* Map probe v2: mount sdc5, FORCE remount rw (ext4 may come up read-only after
 * unclean shutdown), write numbered pong files to all candidate paths, reboot. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

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
        m = syscall(SYS_mount, "/dev/sdc5", "/plog", "ext4", 0, "sync,rw");
    else if (stat("/dev/block/sdc5", &st) == 0)
        m = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync,rw");
    if (m != 0) do_poweroff();

    /* force rw remount in case ext4 came up read-only */
    syscall(SYS_mount, NULL, "/plog", NULL, MS_REMOUNT, "rw");

    /* write test at root */
    int tfd = open("/plog/wtest.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int werr = errno;
    if (tfd >= 0) { write(tfd, "WTEST\n", 6); fsync(tfd); close(tfd); }

    const char *cands[] = {
        "", "/TWRL", "/oplus_fsck_fulldiskscanuserdata", "/cache", "/cache/factory",
        "/TMP", "/pre_watchdog", "/camera", "/oplusreserve", "/storage", "/storage/vold",
        "/DCS", "/backup", "/config", "/olc", "/stamp", "/theia", "/sf", "/postman",
    };
    int n = sizeof(cands) / sizeof(cands[0]);
    char dir[256], path[512];
    int wrote = 0;
    for (int i = 0; i < n; i++) {
        snprintf(dir, sizeof(dir), "/plog%s", cands[i]);
        mkdir(dir, 0777);
        snprintf(path, sizeof(path), "%s/pong-%02d.txt", dir, i);
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) {
            char msg[64];
            int len = snprintf(msg, sizeof(msg), "PONG %02d path=%s\n", i, cands[i]);
            write(fd, msg, len);
            fsync(fd);
            close(fd);
            wrote++;
        }
    }
    /* signal: wrote>0 -> reboot (device returns); wrote==0 -> poweroff */
    if (wrote == 0) do_poweroff();
    do_reboot();
    for (;;) pause();
    return 0;
}
