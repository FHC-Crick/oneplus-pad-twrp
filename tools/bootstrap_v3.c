/* Minimal bootstrapper v3 for OPD2407.
 * The kernel concatenates vendor_boot's ramdisk and init_boot's ramdisk into one
 * initramfs, so TWRP's files already exist in the rootfs. TWRP's own /init was
 * renamed to /init_recovery so this program owns /init as PID 1. Just set up
 * basic mounts and hand over to TWRP init. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>

static void do_poweroff(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x4321fedc, NULL);
}

static int logfd = -1;

static void L(const char *s) {
    if (logfd >= 0) { write(logfd, s, strlen(s)); fsync(logfd); }
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    logfd = open("/boot-strap.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    L("=== minimal bootstrap v3 ===\n");

    /* TWRP files are already unpacked into this rootfs by the kernel; TWRP's
     * original init was renamed to /init_recovery. Hand over. */
    char *argv[] = {"/init_recovery", "second_stage", NULL};
    char *envp[] = {"HOME=/", "PATH=/sbin:/system/bin:/system/xbin", "ANDROID_ROOT=/system", NULL};
    execve("/init_recovery", argv, envp);
    L("exec /init_recovery failed\n");
    /* fallback: original name */
    char *argv2[] = {"/init", "second_stage", NULL};
    execve("/init", argv2, envp);
    L("exec /init failed\n");
    do_poweroff();
    for (;;) pause();
    return 0;
}
