#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h>

static void wfd(int fd, const char *s) {
    const char *p = s;
    while (*p) p++;
    if (fd >= 0) write(fd, s, (size_t)(p - s));
}

int main(void) {
    int console = open("/dev/console", O_WRONLY);
    wfd(console, "HELLO CUSTOM INIT\n");
    syscall(SYS_mount, "proc", "/proc", "proc", 0, 0);
    syscall(SYS_mount, "sysfs", "/sys", "sysfs", 0, 0);
    syscall(SYS_mount, "devtmpfs", "/dev", "devtmpfs", 0, 0);
    mkdir("/plog", 0755);
    syscall(SYS_mount, "/dev/block/sdc5", "/plog", "ext4", 0, "sync");
    int lf = open("/plog/twrp.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    wfd(lf, "CUSTOM INIT ALIVE\n");
    if (lf >= 0) { fsync(lf); close(lf); }
    for (;;) {
        struct timespec ts = {5, 0};
        syscall(SYS_nanosleep, &ts, 0);
    }
    return 0;
}
