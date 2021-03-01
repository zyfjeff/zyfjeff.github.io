---
hide:
  - toc        # Hide table of contents
---
# 如何mock系统调用
## 背景

​&emsp; &emsp;Linux下开发存储系统、网络库的时候会用到一系列Linux的系统调用，每一个系统调用都有一些出错的场景，有些场景很极端，比如内存使用达到上限、磁盘写满等，如果对其进行测试的话，很难去构造这样的一个场景，这个时候集成测试就显得力不存心了，只能靠单元测试来覆盖这些场景。现在的问题就是如何去mock这些系统调用，然后通过程序返回对应场景的错误码来模拟各种场景。也就是将对系统函数的依赖注入到程序中。

## 系统函数的依赖注入

​&emsp; &emsp;目前实现系统函数的依赖注入的手段有很多，分为编译期注入，和运行期注入，至于什么是依赖注入可以参考知乎的一篇文章[如何用最简单的方式解释依赖注入](https://www.zhihu.com/question/32108444)，下面介绍几种依赖注入的方法:

* 虚函数实现依赖注入(运行期注入)

​&emsp; &emsp;使用传统的面向对象的手法，借助运行期的延迟绑定实现注入和替换，自己实现一个System接口类，把程序用到的系统调用都用虚函数封装一层，然后在调用的时候不直接调用系统调用，而是调用的System对应的方法。这样代码的主动权就交给了System接口类了。写单元测试的时候将这个System接口类替换成我们自己的mock对象就可以。完整的示例代码如下:

```C++
  // system.h
  class System {
   public:
    virtual int  open(const char *path, int oflag, ...) = 0;
    virtual ssize_t read(int fildes, void *buf, size_t nbyte) = 0;
    virtual ssize_t write(int fildes, const void *buf, size_t nbyte) = 0;
    virtual int close(int fildes) = 0;

    static System* GetInstance();
    static void set_instance(System* instance) {
      instance_ = instance;
    }

   private:
    static System* instance_;
  };
  // 具体的实现
  class FileOps : public System {
   public:
    int open(const char *path, int oflag, ...) override;
    ssize_t read(int fildes, void *buf, size_t nbyte) override;
    ssize_t write(int fildes, const void *buf, size_t nbyte) override;
    int close(int fildes) override;
    static FileOps* GetInstance();
  };

  // system.cc
  System* System::instance_ = nullptr;
  // 默认实现是FileOps，mock的时候通过改变这个默认实现从而把主动权从默认实现转到了mock的实现
  System* System::GetInstance() {
    if (!instance_) {
      instance_ = FileOps::GetInstance();
    }
    assert(instance_);
    return instance_;
  }

  int FileOps::open(const char *path, int oflag, ...) {
    return ::open(path, oflag, 0777);
  }

  ssize_t FileOps::read(int fildes, void *buf, size_t nbyte) {
    return ::read(fildes, buf, nbyte);
  }

  ssize_t FileOps::write(int fildes, const void *buf, size_t nbyte) {
    return ::write(fildes, buf, nbyte);
  }

  int FileOps::close(int fildes) {
    return ::close(fildes);
  }

  FileOps* FileOps::GetInstance() {
    static FileOps sys;
    return &sys;
  }
  // 正常调用 main.cc
  int main() {
    assert(System::GetInstance() != nullptr);
    int fd = System::GetInstance()->open("txt", O_RDWR|O_CREAT, 0777);
    assert(fd > 0);
    int ret = System::GetInstance()->write(fd, "12345", 5);
    assert(ret > 0);
    ret = System::GetInstance()->close(fd);
    assert(ret == 0);

    return 0;
  }

  // 测试的时候调用如下，模拟一个IO错误

  // 一个mock版本的实现 test.cc
  class MockFileOps : public System {
  public:
   int open(const char *path, int oflag, ...) override;
   ssize_t read(int fildes, void *buf, size_t nbyte) override;
   ssize_t write(int fildes, const void *buf, size_t nbyte) override;
   int close(int fildes) override;
   static MockFileOps* GetInstance();
  };

  int MockFileOps::open(const char *path, int oflag, ...) {
    return ::open(path, oflag, 0777);
  }

  ssize_t MockFileOps::read(int fildes, void *buf, size_t nbyte) {
    return ::read(fildes, buf, nbyte);
  }
  // 模拟的一个IO错误
  ssize_t MockFileOps::write(int fildes, const void *buf, size_t nbyte) {
    errno = EIO;
    return -1;
  }

  int MockFileOps::close(int fildes) {
    return ::close(fildes);
  }

  MockFileOps* MockFileOps::GetInstance() {
    static MockFileOps sys;
    return &sys;
  }

  int main() {
    // 改变默认实现
    System::set_instance(MockFileOps::GetInstance());
    assert(System::GetInstance() != nullptr);
    int fd = System::GetInstance()->open("txt", O_RDWR|O_CREAT, 0777);
    assert(fd > 0);
    int ret = System::GetInstance()->write(fd, "12345", 5);
    assert(ret ==  -1);	// 发生错误
    perror("write");
    ret = System::GetInstance()->close(fd);
    assert(ret == 0);

    return 0;
  }
```

* 编译期延迟绑定(编译期注入)

​&emsp; &emsp;创建一个命名空间，创建一系列和系统调用同名的方法，间接的调用系统调用，写测试代码的时候重新定义这些方法，这就相当于一份代码有了两份实现，根据编译的时候链接哪份代码来决定是否启用mock，这个看起来要比基于虚函数的要简单的多了。完整的示例代码如下:

```C++
  // file_ops.h
  namespace FileOps {
    int  open(const char *path, int oflag, ...);
    ssize_t read(int fildes, void *buf, size_t nbyte);
    ssize_t write(int fildes, const void *buf, size_t nbyte);
    int close(int fildes);
  }  // namespace FileOps

  // file_ops.cc
  namespace FileOps {

  int open(const char *path, int oflag, ...) {
    return ::open(path, oflag, 0777);
  }

  ssize_t read(int fildes, void *buf, size_t nbyte) {
    return ::read(fildes, buf, nbyte);
  }

  ssize_t write(int fildes, const void *buf, size_t nbyte) {
    return ::write(fildes, buf, nbyte);
  }

  int close(int fildes) {
    return ::close(fildes);
  }

  }  // namespace FileOps

  // mock_file_ops.cc

  namespace FileOps {

  int open(const char *path, int oflag, ...) {
    return ::open(path, oflag, 0777);
  }

  ssize_t read(int fildes, void *buf, size_t nbyte) {
    return ::read(fildes, buf, nbyte);
  }
  // 这里做了mock，改变了write的行为
  ssize_t write(int fildes, const void *buf, size_t nbyte) {
    errno = EIO;
    return -1;
  }

  int close(int fildes) {
    return ::close(fildes);
  }

  }  // namespace FileOps

  // 测试程序
  int main() {
    int fd = FileOps::open("txt", O_RDWR|O_CREAT, 0777);
    assert(fd > 0);
    int ret = FileOps::write(fd, "12345", 5);
    if (ret == -1) {
      perror("write:");
    }
    ret = FileOps::close(fd);
    assert(ret == 0);
    return 0;
  }
```

​&emsp; &emsp;​两种方法都比较好实现，前提是代码在一开始的时候就考虑过这些因素，并按照上述方式来编写，然后现实总是残酷的，面对一个已经编码完成的程序该如何为其编写系统调用的mock呢?就需要用到链接期垫片(link seam)的方法。



## 链接期垫片(link seam)

​&emsp; &emsp;连接器垫片的方式一般情况有三种，如下:

- *Shadowing functions* through linking order (override functions in libraries with new definitions in object files)
- *Wrapping functions* with GNU's linker option -wrap (GNU Linux only)
- Run-time function interception* with the preload functionality of the dynamic linker for shared libraries (GNU Linux and Mac OS X only)

​&emsp; &emsp;第一种就是通过链接顺序来改变链接的对象，将要mock的对象重新实现一遍，链接的时候链接器会优先使用我们自己实现的同名函数，这样就可以将目标替换为要mock的对象了，完整代码如下:

```C++
//  一个待测试的对象
int main() {
  int fd = ::open("txt", O_RDWR|O_CREAT, 0777);
  assert(fd > 0);
  int ret = ::write(fd, "12345", 5);
  if (ret == -1) {
    perror("write:");
  }
  ret = ::close(fd);
  assert(ret == 0);
  return 0;
}

// 对目标进行mock，mock的对象是write系统调用
typedef ssize_t (*write_func_t)(int fildes, const void *buf, size_t nbyte);
// 通过dlsym的RTLD_NEXT获取write的下一个定义，也就是libc中的定义，如果想在mock中
// 调用真实的write系统调用不能直接用write，因为write已经被mock了，这样会导致一直递归下去
// 所以这里通过获取真实的write调用的地址，从而难道write的调用入口，这样既可以在mock中调用
// 真实的write调用了
write_func_t old_write_func =
    reinterpret_cast<write_func_t>(dlsym(RTLD_NEXT, "write"));

// 要mock的对象
extern "C" ssize_t write(int fildes, const void *buf, size_t nbyte) {
  errno = EIO;
  return -1;
}
```

​&emsp; &emsp;另外一种就是Linux下独有的，通过gcc的--wrap选项可以指定要wrap的系统调用，那么相应的就回去调用带有`__wrap`前缀的对应系统调用实现，比如--wrap=write，那么在链接的时候就会链接到 `__wrap_write`，而真实的write调用变成了`__real_write`。完整代码例子如下:

```C++
// 测试程序
int main() {
  int fd = ::open("txt", O_RDWR|O_CREAT, 0777);
  assert(fd > 0);
  int ret = ::write(fd, "12345", 5);
  if (ret == -1) {
    perror("write:");
  }
  ret = ::close(fd);
  assert(ret == 0);
  return 0;
}

// mock对象
extern "C" ssize_t __real_write(int fildes, const void *buf, size_t nbyte);

extern "C" ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte) {
  __real_write(fildes, buf, nbyte);
  errno = EIO;
  return -1;
}
```

​&emsp; &emsp;最后一种就是给系统调用提供一份`mock`实现，并编译成动态库，然后通过`LD_LIBRARY_PATH`改变加载动态库的搜索路径让其优先搜索mock版本的动态库，或者是设置`LD_PRELOAD`环境变量，预先加载mock的动态库。



## 附录

* 本文所有代码见[github](https://github.com/zyfjeff/mock.git)
* [Advice on Mocking System Calls](https://stackoverflow.com/questions/2924440/advice-on-mocking-system-calls)