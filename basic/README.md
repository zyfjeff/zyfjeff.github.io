* x86平台上，目前有两套固件标准，一个就是早期的BIOS、另外一个就是UEFI。
* 为了避免让每一个操作系统实现bootloader，所以有了bootloader标准Multiboot，这个标准定义了bootloader和OS交互的接口，其参考实现就是grub。
* [ReadZone](https://os.phil-opp.com/red-zone/)问题
* 通过内存映射的方式访问，VGA (0xb8000)