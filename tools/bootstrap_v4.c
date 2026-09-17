/* Bootstrap v4: runs as PID 1 from init_boot ramdisk, dumps the real kernel
 * cmdline into the kernel log (readable via pstore after reboot), then hands
 * over to the renamed TWRP init (/init_recovery) which came from vendor_boot's
 * ramdisk (already unpacked into this rootfs by the kernel). */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>

static void klog(const char *s) {
    int fd = open("/dev/kmsg", O_WRONLY);
    if (fd >= 0) { write(fd, s, strlen(s)); close(fd); }
}

static void poweroff(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x4321fedc, NULL);
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);

    klog("BOOTSTRAP: start\n");
    /* dump cmdline */
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd >= 0) {
        char buf[1024];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            klog("BOOTSTRAP: cmdline: ");
            klog(buf);
            klog("\n");
        }
    }
    /* check key paths */
    struct stat st;
    int has_init = (lstat("/init", &st) == 0);
    int has_recovery = (lstat("/init_recovery", &st) == 0);
    klog(has_init ? "BOOTSTRAP: /init exists\n" : "BOOTSTRAP: /init MISSING\n");
    klog(has_recovery ? "BOOTSTRAP: /init_recovery exists\n" : "BOOTSTRAP: /init_recovery MISSING\n");

    char *envp[] = {"HOME=/", "PATH=/sbin:/system/bin:/system/xbin", "ANDROID_ROOT=/system", NULL};
    if (has_recovery) {
        char *argv[] = {"/init_recovery", "second_stage", NULL};
        klog("BOOTSTRAP: exec /init_recovery\n");
        execve("/init_recovery", argv, envp);
    }
    if (has_init) {
        char *argv[] = {"/init", "second_stage", NULL};
        klog("BOOTSTRAP: exec /init\n");
        execve("/init", argv, envp);
    }
    klog("BOOTSTRAP: exec failed, poweroff\n");
    poweroff();
    for (;;) pause();
    return 0;
}
