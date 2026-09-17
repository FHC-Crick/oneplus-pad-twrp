/* Full TWRP bootstrapper for OPD2407 (standards-compliant newc).
 * mount basics -> read vendor_boot_a TWRP cpio -> unpack to tmpfs ->
 * pivot_root -> exec TWRP init. Failures power off (observable signal). */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SEG_OFFSET 4096
#define SEG_SIZE (52 * 1024 * 1024)

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

static unsigned hx(const unsigned char *p) {
    unsigned v = 0;
    for (int i = 0; i < 8; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    }
    return v;
}

static void make_path(const char *root, const char *name) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/%s", root, name);
    char *p = buf + 1;
    while (*p) {
        if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
        p++;
    }
}

static int unpack_cpio(const unsigned char *d, size_t size, const char *root) {
    const unsigned char *p = d, *end = d + size;
    char name[1024], full[2048];
    int files = 0;
    while (p + 110 <= end) {
        if (memcmp(p, "070701", 6) != 0) { L("bad magic\n"); return -1; }
        unsigned mode = hx(p + 14);
        unsigned filesize = hx(p + 54);
        unsigned rdevmaj = hx(p + 78);
        unsigned rdevmin = hx(p + 86);
        unsigned namesize = hx(p + 94);
        if (namesize == 0 || namesize >= 1024) { L("bad namesize\n"); return -2; }
        if ((size_t)(end - p) < 110 + (size_t)namesize + filesize) { L("overflow\n"); return -3; }
        memcpy(name, p + 110, namesize - 1);
        name[namesize - 1] = 0;
        if (strcmp(name, "TRAILER!!!") == 0) { L("unpack done\n"); return 0; }
        if (strncmp(name, "./", 2) == 0) memmove(name, name + 2, strlen(name) - 1);
        snprintf(full, sizeof(full), "%s/%s", root, name);
        make_path(root, name);
        const unsigned char *data = p + 110 + namesize;
        if (S_ISDIR(mode)) {
            mkdir(full, mode & 07777);
        } else if (S_ISLNK(mode)) {
            char tgt[1024];
            unsigned tl = filesize < 1023 ? filesize : 1023;
            memcpy(tgt, data, tl);
            tgt[tl] = 0;
            symlink(tgt, full);
        } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
            mknod(full, mode, makedev(rdevmaj, rdevmin));
        } else if (S_ISREG(mode)) {
            int f = open(full, O_WRONLY | O_CREAT | O_TRUNC, mode & 07777);
            if (f >= 0) {
                size_t off = 0;
                while (off < filesize) {
                    ssize_t w = write(f, data + off, filesize - off);
                    if (w <= 0) break;
                    off += w;
                }
                close(f);
                chmod(full, mode & 07777);
            }
        }
        files++;
        size_t total = 110 + (size_t)namesize + filesize;
        p += total + ((4 - total % 4) % 4);
    }
    L("no trailer\n");
    return -4;
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);
    if (syscall(SYS_mount, "/dev/sdc5", "/plog", "ext4", 0, "sync,rw") != 0)
        syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync,rw");
    logfd = open("/plog/twrp-boot.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    L("=== bootstrap start ===\n");

    int fd = open("/dev/sdc41", O_RDONLY);
    if (fd < 0) fd = open("/dev/block/sdc41", O_RDONLY);
    if (fd < 0) { L("no vendor_boot node\n"); do_poweroff(); }
    lseek(fd, SEG_OFFSET, SEEK_SET);
    unsigned char *buf = malloc(SEG_SIZE);
    if (!buf) { L("malloc fail\n"); do_poweroff(); }
    ssize_t got = read(fd, buf, SEG_SIZE);
    close(fd);
    L("read vb ok\n");
    if (got < 110) { L("short read\n"); do_poweroff(); }

    mkdir("/newroot", 0755);
    if (syscall(SYS_mount, "tmpfs", "/newroot", "tmpfs", 0, 0) != 0) { L("tmpfs fail\n"); do_poweroff(); }
    if (chdir("/newroot") != 0) { L("chdir fail\n"); do_poweroff(); }
    if (unpack_cpio(buf, (size_t)got, ".") != 0) do_poweroff();
    L("unpack ok\n");

    syscall(SYS_mount, NULL, "/proc", NULL, MS_MOVE, NULL);
    syscall(SYS_mount, NULL, "/sys", NULL, MS_MOVE, NULL);
    syscall(SYS_mount, NULL, "/dev", NULL, MS_MOVE, NULL);
    mkdir("/oldroot", 0755);
    if (syscall(SYS_pivot_root, ".", "./oldroot") != 0) { L("pivot fail\n"); do_poweroff(); }
    chdir("/");
    umount2("/oldroot", MNT_DETACH);
    L("exec twrp init\n");
    if (logfd >= 0) { dup2(logfd, 1); dup2(logfd, 2); }
    char *argv[] = {"/init", "second_stage", NULL};
    char *envp[] = {"HOME=/", "PATH=/sbin:/system/bin:/system/xbin", "ANDROID_ROOT=/system", NULL};
    execve("/init", argv, envp);
    L("exec failed\n");
    do_poweroff();
    for (;;) pause();
    return 0;
}
