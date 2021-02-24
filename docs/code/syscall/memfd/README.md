$ ./memfd_create my_memfd_file 4096 sw &
[1] 11775
PID: 11775; fd: 3; /proc/11775/fd/3

$ readlink /proc/11775/fd/3
/memfd:my_memfd_file (deleted)
$ ./get_seals /proc/11775/fd/3
Existing seals: WRITE SHRINK