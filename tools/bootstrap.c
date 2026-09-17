/* Modify probe: search candidate paths for the shell-written marker.txt, then
 * TRUNCATE and rewrite it with "MODIFIED-<i> <path>". If the stock system sees
 * the new content, the mapping and write path are proven. Also writes fresh
 * files as a secondary signal. */
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
        m = syscall(SYS_mount, "/dev/sdc5", "/plog", "ext4", 0, "sync,rw");
    else if (stat("/dev/block/sdc5", &st) == 0)
        m = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync,rw");
    if (m != 0) do_poweroff();
    syscall(SYS_mount, NULL, "/plog", NULL, MS_REMOUNT, "rw");

    const char *cands[] = {
        "", "/TWRL", "/oplus_fsck_fulldiskscanuserdata", "/cache", "/cache/factory",
        "/TMP", "/pre_watchdog", "/camera", "/oplusreserve", "/storage", "/storage/vold",
        "/DCS", "/backup", "/config", "/olc", "/stamp", "/theia", "/sf", "/postman",
    };
    int n = sizeof(cands) / sizeof(cands[0]);
    char path[512], msg[128];
    int modified = 0;
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "/plog%s/marker.txt", cands[i]);
        if (stat(path, &st) != 0) continue;
        int fd = open(path, O_WRONLY | O_TRUNC);
        if (fd >= 0) {
            int len = snprintf(msg, sizeof(msg), "MODIFIED-%02d %s\n", i, cands[i]);
            write(fd, msg, len);
            fsync(fd);
            close(fd);
            modified = i + 1;
        }
    }
    /* secondary: create files in root and TWRL */
    int w = 0;
    for (int i = 0; i < 2; i++) {
        const char *d = i == 0 ? "/plog" : "/plog/TWRL";
        mkdir(d, 0777);
        snprintf(path, sizeof(path), "%s/newfile-%d.txt", d, i);
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) { write(fd, "NEW\n", 4); fsync(fd); close(fd); w++; }
    }
    if (modified == 0 && w == 0) do_poweroff();
    do_reboot();
    for (;;) pause();
    return 0;
}
