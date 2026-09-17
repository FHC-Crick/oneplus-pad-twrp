/* TWRP bootstrapper v2 for OPD2407 - uses magiskboot for vendor_boot unpacking
 * and cpio extraction (battle-tested against all cpio variants).
 * Flow: mounts -> read vendor_boot_a -> magiskboot unpack -> magiskboot cpio
 * extract into /newroot -> pivot_root -> exec TWRP init. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define VB_OFFSET 0
#define VB_SIZE (64 * 1024 * 1024)

static void do_reboot(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x01234567, NULL);
}

static void do_poweroff(void) {
    syscall(SYS_reboot, 0xfee1dead, 672274793, 0x4321fedc, NULL);
}

static int logfd = -1;

static void L(const char *s) {
    if (logfd >= 0) { write(logfd, s, strlen(s)); fsync(logfd); }
}

static char *g_envp[] = {"HOME=/", "PATH=/sbin:/bin:/system/bin", "ANDROID_ROOT=/system", NULL};

static int run(char *const argv[], const char *cwd) {
    pid_t pid = fork();
    if (pid < 0) return -100;
    if (pid == 0) {
        if (cwd) { if (chdir(cwd) != 0) _exit(126); }
        execve(argv[0], argv, g_envp);
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -101;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -102;
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);
    syscall(SYS_mount, "/dev/sdc5", "/plog", "ext4", 0, "sync,rw");
    logfd = open("/plog/twrp-boot.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    L("=== bootstrap v2 (magiskboot) ===\n");

    /* read whole vendor_boot_a partition */
    int fd = open("/dev/sdc41", O_RDONLY);
    if (fd < 0) fd = open("/dev/block/sdc41", O_RDONLY);
    if (fd < 0) { L("no vb node\n"); do_poweroff(); }
    unsigned char *buf = malloc(VB_SIZE);
    if (!buf) { L("malloc fail\n"); do_poweroff(); }
    ssize_t got = read(fd, buf, VB_SIZE);
    close(fd);
    L("vb read\n");
    if (got < 4096) { L("short read\n"); do_poweroff(); }

    mkdir("/tmp", 0755);
    syscall(SYS_mount, "tmpfs", "/tmp", "tmpfs", 0, 0);
    mkdir("/newroot", 0755);
    syscall(SYS_mount, "tmpfs", "/newroot", "tmpfs", 0, 0);
    mkdir("/tmp/work", 0755);

    /* write vendor_boot image to tmpfs for magiskboot */
    int of = open("/tmp/vb.img", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (of < 0) { L("open vb.img fail\n"); do_poweroff(); }
    size_t off = 0;
    while (off < (size_t)got) {
        ssize_t w = write(of, buf + off, (size_t)got - off);
        if (w <= 0) break;
        off += w;
    }
    close(of);
    L("vb.img written\n");

    /* magiskboot unpack vendor_boot -> ramdisk.cpio in /tmp/work */
    char *a1[] = {"/bin/magiskboot", "unpack", "/tmp/vb.img", NULL};
    int r1 = run(a1, "/tmp/work");
    L("magiskboot unpack rc\n");

    /* magiskboot cpio extract into /newroot */
    char *a2[] = {"/bin/magiskboot", "cpio", "/tmp/work/ramdisk.cpio", "extract", NULL};
    int r2 = run(a2, "/newroot");
    L("magiskboot extract rc\n");
    (void)r1; (void)r2;

    syscall(SYS_mount, NULL, "/proc", NULL, MS_MOVE, NULL);
    syscall(SYS_mount, NULL, "/sys", NULL, MS_MOVE, NULL);
    if (chdir("/newroot") != 0) { L("chdir fail\n"); do_poweroff(); }
    mkdir("/newroot/oldroot", 0755);
    if (syscall(SYS_pivot_root, ".", "./oldroot") != 0) { L("pivot fail\n"); do_poweroff(); }
    chdir("/");
    /* put devtmpfs in place for TWRP init */
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    umount2("/oldroot", MNT_DETACH);

    L("exec twrp init\n");
    if (logfd >= 0) { dup2(logfd, 1); dup2(logfd, 2); }
    char *argv[] = {"/init", "second_stage", NULL};
    execve("/init", argv, g_envp);
    L("exec failed\n");
    do_poweroff();
    for (;;) pause();
    return 0;
}
