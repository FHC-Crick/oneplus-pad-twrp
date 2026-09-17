/* Full TWRP bootstrapper for OPD2407.
 * Mounts basic fs, reads vendor_boot_a (sdc41) TWRP lz4 ramdisk at offset 4096,
 * decompresses, unpacks cpio to /newroot, pivot_root, exec /init (TWRP init).
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lz4.h"

#define SEG_OFFSET 4096
#define SEG_SIZE (52 * 1024 * 1024)
#define OUT_MAX (160 * 1024 * 1024)

static int con = -1;

static void cput(const char *s) {
    if (con < 0) {
        con = open("/dev/console", O_WRONLY);
        if (con < 0) con = open("/dev/tty0", O_WRONLY);
    }
    if (con >= 0) write(con, s, strlen(s));
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
        if (*p == '/') {
            *p = 0;
            mkdir(buf, 0755);
            *p = '/';
        }
        p++;
    }
}

static int unpack_cpio(const unsigned char *d, size_t size, const char *root) {
    const unsigned char *p = d, *end = d + size;
    char name[512], full[1024];
    while (p + 110 <= end) {
        if (memcmp(p, "070701", 6) != 0) return -1;
        unsigned mode = hx(p + 14);
        unsigned filesize = hx(p + 54);
        unsigned rdevmaj = hx(p + 78);
        unsigned rdevmin = hx(p + 86);
        unsigned namesize = hx(p + 94);
        if (namesize == 0 || namesize > 500) return -2;
        if (p + 110 + namesize + filesize > end) return -3;
        memcpy(name, p + 110, namesize - 1);
        name[namesize - 1] = 0;
        if (strcmp(name, "TRAILER!!!") == 0) return 0;
        if (strncmp(name, "./", 2) == 0) {
            memmove(name, name + 2, strlen(name) - 1);
        }
        snprintf(full, sizeof(full), "%s/%s", root, name);
        make_path(root, name);
        const unsigned char *data = p + 110 + namesize;
        if (S_ISDIR(mode)) {
            mkdir(full, mode & 07777);
        } else if (S_ISLNK(mode)) {
            char tgt[512];
            unsigned tl = filesize < 511 ? filesize : 511;
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
        size_t total = 110 + namesize + filesize;
        p += total + ((4 - total % 4) % 4);
    }
    return 0;
}

int main(void) {
    cput("TWRP BOOTSTRAP START\n");
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    cput("FS MOUNTED\n");

    int fd = open("/dev/block/sdc41", O_RDONLY);
    if (fd < 0) { cput("OPEN SDC41 FAIL\n"); for (;;) pause(); }
    lseek(fd, SEG_OFFSET, SEEK_SET);
    unsigned char *lz = malloc(SEG_SIZE);
    unsigned char *out = malloc(OUT_MAX);
    if (!lz || !out) { cput("MALLOC FAIL\n"); for (;;) pause(); }
    ssize_t got = read(fd, lz, SEG_SIZE);
    cput("READ VB DONE\n");
    close(fd);

    int n = LZ4_decompress_safe((const char *)lz, (char *)out, (int)got, OUT_MAX);
    if (n <= 0) { cput("LZ4 FAIL\n"); for (;;) pause(); }
    cput("LZ4 OK\n");

    mkdir("/newroot", 0755);
    syscall(SYS_mount, "tmpfs", "/newroot", "tmpfs", 0, 0);
    if (chdir("/newroot") != 0) { cput("CHDIR FAIL\n"); for (;;) pause(); }
    int rc = unpack_cpio(out, (size_t)n, ".");
    if (rc != 0) { cput("CPIO FAIL\n"); for (;;) pause(); }
    cput("CPIO OK\n");

    syscall(SYS_mount, NULL, "/proc", NULL, MS_MOVE, NULL);
    syscall(SYS_mount, NULL, "/sys", NULL, MS_MOVE, NULL);
    syscall(SYS_mount, NULL, "/dev", NULL, MS_MOVE, NULL);

    mkdir("/oldroot", 0755);
    if (syscall(SYS_pivot_root, ".", "./oldroot") != 0) { cput("PIVOT FAIL\n"); for (;;) pause(); }
    chdir("/");
    umount2("/oldroot", MNT_DETACH);

    cput("EXEC TWRP INIT\n");
    char *argv[] = {"/init", "second_stage", NULL};
    char *envp[] = {"HOME=/", "PATH=/sbin:/system/bin:/system/xbin", "ANDROID_ROOT=/system", NULL};
    execve("/init", argv, envp);
    cput("EXEC FAIL\n");
    for (;;) pause();
    return 0;
}
