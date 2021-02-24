#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include <stdlib.h>

int read_cross(pid_t pid, char *mybuf, char *theirbuf, size_t length) {
    struct iovec local[1];
    struct iovec remote[1];
    local[0].iov_base = mybuf;
    local[0].iov_len = length;
    remote[0].iov_base = theirbuf;
    remote[0].iov_len = length;
    return process_vm_readv(pid, local, 1, remote, 1, 0);
}

typedef struct {
    pid_t pid;
} shake_hand_t;

typedef struct {
    int length;
    char *buf;
    char free;
} data_t;

void loop(int fd, int pid) {
    data_t data;
    read(fd, &data, sizeof(data));
    if (data.free) {
        free(data.buf);
    } else {
        char *buf = malloc(data.length);
        int nread = read_cross(pid, buf, data.buf, data.length);
        printf("nread: %d error: %s tpid:%d pid:%d content: %s buf:%x data.buf: %x\n", nread, strerror(errno), pid, getpid(), buf, buf, data.buf);
        sprintf(buf, "%d", atoi(buf) + 1);
        sleep(1);
        data.free = 1;
        write(fd, &data, sizeof(data));
        data.free = 0;
        data.buf = buf;
        write(fd, &data, sizeof(data));
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage: vm <message-size> <roundtrip-count>\n");
        return 1;
    }
    int64_t size = atoi(argv[1]);
    int64_t count = atol(argv[2]);
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    pid_t child_pid = fork();
    shake_hand_t sh;
    sh.pid = getpid();
    if (child_pid) { // parent
        close(sv[1]);
        int fd = sv[0];
        write(fd, &sh, sizeof(sh));
        read(fd, &sh, sizeof(sh));
        pid_t pid = sh.pid;

        data_t data;
        data.free = 0;
        data.length = size;
        data.buf = malloc(data.length);
        sprintf(data.buf, "%d", 1);
        write(fd, &data, sizeof(data));

        struct timespec start, stop;
        int64_t delta;
        clock_gettime(CLOCK_MONOTONIC, &start);
        int64_t idx = 0;

        while(++idx < count) loop(fd, pid);

        clock_gettime(CLOCK_MONOTONIC, &stop);

        delta = ((stop.tv_sec - start.tv_sec) * 1000000000 +
                (stop.tv_nsec - start.tv_nsec));
        printf("average latency: %li ns\n", delta / (count * 2));

    } else {
        malloc(1234);
        close(sv[0]);
        int fd = sv[1];
        write(fd, &sh, sizeof(sh));
        read(fd, &sh, sizeof(sh));
        pid_t pid = sh.pid;

        while(1) loop(fd, pid);

    }

    return 0;
}
