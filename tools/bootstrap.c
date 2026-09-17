/* Binary probe: if stat+mount+open of sdc5 all succeed -> reboot (device falls
 * back to slot b automatically). Otherwise power off (device stays dark = the
 * observable signal distinguishing the two outcomes, fully unattended). */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>

#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_MAGIC2 672274793

static void do_reboot(void) {
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, 0x01234567, NULL);
}

static void do_poweroff(void) {
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, 0x4321fedc, NULL);
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);

    struct stat st;
    int s1 = stat("/dev/sdc5", &st);
    int s2 = stat("/dev/block/sdc5", &st);

    mkdir("/plog", 0755);
    long m = -1;
    if (s1 == 0) m = syscall(SYS_mount, "/dev/sdc5", "/plog", "ext4", 0, "sync");
    else if (s2 == 0) m = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync");

    if (s1 == 0 || s2 == 0) {
        if (m == 0) {
            /* mount ok: also try writing a marker file (readable later) */
            int fd = open("/plog/mount-ok.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd >= 0) { write(fd, "MOUNT OK\n", 9); close(fd); }
            do_reboot();
        } else {
            do_poweroff();
        }
    } else {
        do_poweroff();
    }
    for (;;) pause();
    return 0;
}
