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

### reference:
1. [man pages](http://man7.org/linux/man-pages/man2/kcmp.2.html)

## vsock (4.8+)

```3
  #include <sys/socket.h>
  #include <linux/vm_sockets.h>

  stream_socket = socket(AF_VSOCK, SOCK_STREAM, 0);
  datagram_socket = socket(AF_VSOCK, SOCK_DGRAM, 0);
```

这是一个新的socket类型，是Linux独有的，主要是用于虚拟机和宿主机之间的通信。

reference:
1. [man pages](http://man7.org/linux/man-pages/man7/vsock.7.html)
2. [vsock test](https://cregit.linuxsources.org/code/4.19/tools/testing/vsock/vsock_diag_test.c.html)


## mincore

用于判断一个文件是否在内存中

```
       mincore - determine whether pages are resident in memory

SYNOPSIS
       #include <unistd.h>
       #include <sys/mman.h>

       int mincore(void *addr, size_t length, unsigned char *vec);
```

reference:

1. [man pages](http://man7.org/linux/man-pages/man2/mincore.2.html)

## memfd_create

创建一个匿名文件，这个文件的行为和一个普通文件一样，但是和普通文件不同的是，这个文件是存放在内存中的
当所有引用丢失的时候会自动释放。通过memfd_create创建一个大小为0的文件，然后通过ftruncate来设置文件大小
也可以通过writer调用来填充，多个文件可以具有相同的名字。这个syscall的目的是为了限制fd的可用操作来避免发生一些race condition。
一般需要结合mmap来使用，等用与在tmpfs中打开一个文件。

```
   File sealing
       In the absence of file sealing, processes that communicate via shared
       memory must either trust each other, or take measures to deal with
       the possibility that an untrusted peer may manipulate the shared
       memory region in problematic ways.  For example, an untrusted peer
       might modify the contents of the shared memory at any time, or shrink
       the shared memory region.  The former possibility leaves the local
       process vulnerable to time-of-check-to-time-of-use race conditions
       (typically dealt with by copying data from the shared memory region
       before checking and using it).  The latter possibility leaves the
       local process vulnerable to SIGBUS signals when an attempt is made to
       access a now-nonexistent location in the shared memory region.
       (Dealing with this possibility necessitates the use of a handler for
       the SIGBUS signal.)

       Dealing with untrusted peers imposes extra complexity on code that
       employs shared memory.  Memory sealing enables that extra complexity
       to be eliminated, by allowing a process to operate secure in the
       knowledge that its peer can't modify the shared memory in an
       undesired fashion.
```

通过memfd_create就可以实现File sealing来避免对untrusted的操作进行处理。可以直接限制允许的操作。

reference:
1. [man pages](http://www.man7.org/linux/man-pages/man2/memfd_create.2.html)


## SOCK_SEQPACKET

* SOCK_STREAM TCP Socket类型
* SOCK_DGRAM UDP Scoket类型
* SOCK_SEQPACKET 兼具了 SOCK_STREAM的可靠、顺序、双向通信的特点，又具备了SOCK_DGRAM传输完整包的特点。传输的包具有完整的边界、不会拆包。


## TIOCSTI

可以往指定的终端的输入缓冲区放置相关的字符


```
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv)
{
  char tty[16] = {0};
  char cmd[64] = {0};
  char tmp[2] = {0};
  int len, i;
  int fd;

  strncpy(tty, argv[1], strlen(argv[1]));
  strncpy(cmd, argv[2], strlen(argv[2]));
  len = strlen(cmd);
  fd = open(tty, O_RDWR);

  for (i = 0; i < len; i++) {
    sprintf(tmp, "%c ", cmd[i]);
    ioctl(fd, TIOCSTI, tmp);
  }
  ioctl(fd, TIOCSTI, "\n ");
}
```

## xstat
https://man7.org/linux/man-pages/man2/statx.2.html