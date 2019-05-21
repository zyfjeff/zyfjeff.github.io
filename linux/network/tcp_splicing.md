# SOCKMAP - TCP splicing of the future

最近我偶然发现了一个反向代理中的秘密武器-TCP Socket splicing API，这引起了我的注意，正如你所知，我所在的公司正运行着全网级别的反向代理服务。
适当的使用TCP Socket splicing技术可以减少用户态进程的负载，可以实现更高效的数据转发。我了解到Linux kernel中的SOCKMAP技术可以达到这个目的。
SOCKMAP是一个非常有前景的API，它可能会导致数据密集型的应用程序(比如反向代理)的体系结构发生转变。

在此之前，请让我们回顾下历史。
