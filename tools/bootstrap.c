/* Bisect probe A: full mount + read + cpio unpack, then stop.
 * Signal: unpack OK -> reboot (device returns to slot b normally);
 *         unpack error -> poweroff (device stays off, I detect it); 
 *         crash -> kernel panic -> lk falls back to b anyway. */
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
    char name[512], full[1024];
    int files = 0;
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
        if (strncmp(name, "./", 2) == 0) memmove(name, name + 2, strlen(name) - 1);
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
        files++;
        size_t total = 110 + namesize + filesize;
        p += total + ((4 - total % 4) % 4);
    }
    return -4;
}

int main(void) {
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);
    int fd = open("/dev/sdc41", O_RDONLY);
    if (fd < 0) fd = open("/dev/block/sdc41", O_RDONLY);
    if (fd < 0) do_poweroff();
    lseek(fd, SEG_OFFSET, SEEK_SET);
    unsigned char *buf = malloc(SEG_SIZE);
    if (!buf) do_poweroff();
    ssize_t got = read(fd, buf, SEG_SIZE);
    close(fd);
    if (got < 110) do_poweroff();

    mkdir("/newroot", 0755);
    if (syscall(SYS_mount, "tmpfs", "/newroot", "tmpfs", 0, 0) != 0) do_poweroff();
    if (chdir("/newroot") != 0) do_poweroff();
    int rc = unpack_cpio(buf, (size_t)got, ".");
    if (rc != 0) do_poweroff();
    /* unpack OK -> reboot so device returns to stock system; lk falls back to b */
    do_reboot();
    for (;;) pause();
    return 0;
}
