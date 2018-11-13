## kcmp (4.16)
这个系统调用是用于探测两个进程是否共享一个内核资源，glibc没有封装这个系统调用，
并且这个系统调用只在内核配置了`CONFIG_CHECKPOINT_RESTORE`选项的时候才开启，
这个系统调用的目的是为了`checkpoint/restore`的feature。

```
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/kcmp.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

static int
kcmp(pid_t pid1, pid_t pid2, int type,
    unsigned long idx1, unsigned long idx2)
{
    return syscall(SYS_kcmp, pid1, pid2, type, idx1, idx2);
}

static void
test_kcmp(char *msg, id_t pid1, pid_t pid2, int fd_a, int fd_b)
{
    printf("\t%s\n", msg);
    printf("\t\tkcmp(%ld, %ld, KCMP_FILE, %d, %d) ==> %s\n",
            (long) pid1, (long) pid2, fd_a, fd_b,
            (kcmp(pid1, pid2, KCMP_FILE, fd_a, fd_b) == 0) ?
                        "same" : "different");
}

int
main(int argc, char *argv[])
{
    int fd1, fd2, fd3;
    char pathname[] = "/tmp/kcmp.test";

    fd1 = open(pathname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd1 == -1)
        errExit("open");

    printf("Parent PID is %ld\n", (long) getpid());
    printf("Parent opened file on FD %d\n\n", fd1);

    switch (fork()) {
    case -1:
        errExit("fork");

    case 0:
        printf("PID of child of fork() is %ld\n", (long) getpid());

        test_kcmp("Compare duplicate FDs from different processes:",
                getpid(), getppid(), fd1, fd1);

        fd2 = open(pathname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if (fd2 == -1)
            errExit("open");
        printf("Child opened file on FD %d\n", fd2);

        test_kcmp("Compare FDs from distinct open()s in same process:",
                getpid(), getpid(), fd1, fd2);

        fd3 = dup(fd1);
        if (fd3 == -1)
            errExit("dup");
        printf("Child duplicated FD %d to create FD %d\n", fd1, fd3);

        test_kcmp("Compare duplicated FDs in same process:",
                getpid(), getpid(), fd1, fd3);
        break;

    default:
        wait(NULL);
    }

    exit(EXIT_SUCCESS);
}
```
reference: http://man7.org/linux/man-pages/man2/kcmp.2.html
