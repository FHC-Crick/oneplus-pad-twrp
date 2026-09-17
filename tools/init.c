#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <time.h>

static int logfd = -1;

static void L(const char *s) {
    if (logfd >= 0) {
        write(logfd, s, strlen(s));
        fsync(logfd);
    }
}

static void LN(long v) {
    char buf[32];
    char *p = buf + 30;
    *p = '\n';
    *--p = 0;
    if (v == 0) { *--p = '0'; }
    else {
        unsigned long u = v < 0 ? (unsigned long)(-v) : (unsigned long)v;
        while (u) { *--p = '0' + u % 10; u /= 10; }
        if (v < 0) *--p = '-';
    }
    L(p);
}

static void console(const char *s) {
    int fd = open("/dev/console", O_WRONLY);
    if (fd < 0) fd = open("/dev/tty0", O_WRONLY);
    if (fd >= 0) { write(fd, s, strlen(s)); close(fd); }
}

int main(void) {
    console("HELLO INIT V2\n");
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);

    mkdir("/plog", 0755);
    struct stat st;
    long s5 = stat("/dev/block/sdc5", &st);
    long s4 = stat("/dev/block/sdc4", &st);
    long s13 = stat("/dev/block/sdc13", &st);
    long m5 = syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync");

    logfd = open("/plog/twrp.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    L("V2 alive\n");
    L("stat sdc5="); LN(s5);
    L("stat sdc4="); LN(s4);
    L("stat sdc13="); LN(s13);
    L("mount sdc5 rc="); LN(m5);
    L("mount errno="); LN((long)errno);
    if (m5 != 0) {
        // fallback mounts into subdirs of rootfs won't help log visibility;
        // try sdc4/sdc13 on other mount points
        mkdir("/plog4", 0755);
        long m4 = syscall(SYS_mount, "/dev/block/sdc4", "/plog4", "ext4", 0, "sync");
        L("mount sdc4 rc="); LN(m4);
        if (m4 == 0) {
            int lf4 = open("/plog4/twrp.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (lf4 >= 0) { write(lf4, "V2 via sdc4\n", 12); close(lf4); }
        }
    }
    // enumerate /dev/block
    DIR *d = opendir("/dev/block");
    if (d) {
        struct dirent *e;
        L("blockdevs:");
        while ((e = readdir(d)) != NULL) {
            L(" ");
            L(e->d_name);
        }
        L("\n");
        closedir(d);
    } else {
        L("opendir /dev/block failed errno="); LN((long)errno);
    }
    fsync(logfd);
    close(logfd);
    for (;;) {
        struct timespec ts = {5, 0};
        syscall(SYS_nanosleep, &ts, 0);
    }
    return 0;
}
