/* Ping-pong probe: find shell-written marker.txt among candidate paths after
 * mounting sdc5; if found, write pong.txt there and reboot (falls back to b).
 * If no candidate sees the marker, power off (distinct observable signal). */
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
        "/plog/oplus_fsck_fulldiskscanuserdata",
        "/plog/cache/factory",
        "/plog/storage/vold",
        "/plog/oplusreserve",
        "/plog/cache",
        "/plog/TMP",
        "/plog/pre_watchdog",
        "/plog/camera",
        "/plog",
    };
    const char *found = NULL;
    char path[512];
    for (int i = 0; i < 9; i++) {
        snprintf(path, sizeof(path), "%s/marker.txt", cands[i]);
        if (stat(path, &st) == 0) { found = cands[i]; break; }
    }
    if (!found) do_poweroff();

    /* write pong into the directory where marker was found */
    snprintf(path, sizeof(path), "%s/pong.txt", found);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        write(fd, "PONG FROM INIT\n", 15);
        fsync(fd);
        close(fd);
    }
    /* also drop a marker at sdc5 root for the record */
    fd = open("/plog/root-marker.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) { write(fd, "ROOT OK\n", 8); close(fd); }

    do_reboot();
    for (;;) pause();
    return 0;
}
