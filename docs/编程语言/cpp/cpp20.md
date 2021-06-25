---
hide:
  - toc        # Hide table of contents
---

# C++20


## Module

Module example，需要Clang 10.0以上版本

```
/// helloworld.cc
module;
import <cstdio>;
export module helloworld;

export namespace helloworld {
    int global_data;

    void say_hello() {
        std::printf("Hello Module! Data is %d\n", global_data);
    }
}

/// main.cc
import helloworld;

int main() {
    helloworld::global_data = 123;
    helloworld::say_hello();
}
```

先将module编译成中间文件，然后再编译main.cc，需要添加相关的module参数

```
$ clang++ -std=c++2a -stdlib=libc++ -fimplicit-modules -fimplicit-module-maps -c helloworld.cc -Xclang -emit-module-interface -o helloworld.pcm
$ clang++ -std=c++2a -stdlib=libc++ -fimplicit-modules -fimplicit-module-maps -fprebuilt-module-path=. main.cc helloworld.cc

$ ./a.out
Hello Module! Data is 123
```

https://brevzin.github.io/c++/2020/07/06/split-view/