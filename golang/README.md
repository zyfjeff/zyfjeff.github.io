## Basic

* golang的类型转换可以是窄化的，例如下面这个例子:

```
var x int64 = 64000
var y int16 = int16(x)
```


* range会复制目标数据，受直接影响的是数组，可改用数组指针或切片类型
* 字典切片都是引用类型，拷贝开销低
* 编译器会进行逃逸分析将栈上变量分配在堆上
* go build -gcflags "-l -m" 禁用函数内联(-l)，输出优化信息(-m)
* []byte和string的头部结构相同可以通unsafe.Pointer来互相转换
* golang在编译器层面会对[]byte转换为string进行map查询的时候进行优化，避免string拷贝
* golang在编译器层面会对string转换[]byte，进行for range迭代时，直接取字节赋值给局部变量
* string.Join 一次性分配内存等同于bytes.Buffer先Grow事先准备足够的内存
* slice类型不支持比较操作，仅能判断是否为nil，
* slice的len、cap、底层数组的长度，当超过cap的时候会按照cap进行2倍的扩容(并非总是2倍，对于较大的slice则是1/4)
* 不能对nil字典进行写操作，但是却能读
* 字典对象本身就是指针包装的，传参的时候无须再次取地址
* 对于海量小对象应该直接用值拷贝，而非指针，这有助于减少需要扫描的对象数量，大幅缩短垃圾回收时间
* 空结构都指向runtime.zerobase
* 不能将基础类型和其对应的指针类型同时匿名嵌入到结构中(本质上两者的隐式名字相同)
* 如何选择方法的receiver的类型
    1. 要修改实例的状态用`*T`
    2. 无须修改状态的小对象或固定值，建议用`T`
    3. 大对象建议用`*T`，以减少复制成本
    4. 引用类型、字符串、函数等指针包装对象，直接用T
    5. 若包含了`Mutex`等同步字段，用`*T`，避免因复制构造成锁操作而无效
    6. 其他无法确定的情况都用`*T`
* 方法集，通过reflect.TypeOf拿到反射的对象，然后通过NumMethod可以得到方法集的数量，通过Method拿到方法对象，通过方法对象的
  Name方法拿到方法名，通过Type方法拿到方法的声明
    1. 类型T的方法集包含了所有receiver T的方法
    2. 类型`*T`方法集包含了所有`receiver T + *T`方法
    3. 匿名嵌入S，T方法集包含了所有receiver S的方法
    4. 匿名嵌入`*S`，T方法集包含了所有`receiver S + *S`方法
    5. 匿名嵌入`*S`或S,`*T`方法集包含了所有`receiver S + *S`的方法
* Method Expression 和Method Value
* 接口通常以er作为后缀名
* 超集接口可以隐式转换为子集
* 将对象赋值给接口的变量时，会复制该对象，解决办法就是将对象指针赋值给接口。
* 只有将接口变量内部的两个指针(itab，data)都为nil时，接口才等于nil
* 默认协程的堆栈大小时2KB，可以扩大。
* rumtime.GOMAXPROCS，runtime.NumCPU
* 一次性的事件使用close来触发，连续或多样性事件，可传递不同数据标志来实现，还可以使用sync.Cond来实现单播火广播事件
* 向已关闭的通道发送数据会引发panic，从已关闭的通道接收数据，返回已缓冲数据或零值，无论收发，nil通道都会阻塞，重复关闭通道会引发panic。
* 通常使用类型转换来获取单向通道，并分别赋予操作双方，不能在单向通道上做逆向操作，同样close不能作用于接收端，无法将单向通道重新转换回去。
* 无论是否执行reciver，所有延迟调用都会被执行
* 连续调用panic，仅最后一个会被recover捕获
* `runtime.GC()` 主动垃圾回收、`GODEBUG="gctrace=1,schedtrace=1000,scheddetail=1"`
* `sync.Mutex`不能复制会导致失效，所以当包含了`sync.Mutex`的匿名字段的时候，需要将其实现为指针型的receiver，或者嵌入`*sync.Mutex`
* 包内每一个源码文件都可以定义一到多个初始化函数，但是编译器不保证执行次序，只能保证所有的初始化函数都是单一线程执行的，且仅执行一次
* 所有保存在internal目录下的包(包括自身)仅能被其父目录下的包(含所有层次的子目录)访问
* `io.MultiWriter`可以接收多个writer(只要实现了`io.writer`即可)，通过`io.MultiWriter`可以将数据写入到多个writer中
* 执行一个shell命令(exec.Command)并拿到结果
  1. `cmd.CombinedOutput`
  2. `cmd.Stdout` 和 `cmd.Stderr`
  3. `cmd.StdoutPipe` 和 `cmd.StderrPipe`
  4. `cmd.Env`设置程序执行的环境变量

* https://blog.drewolson.org/dependency-injection-in-go 依赖注入 DIC
* go中的struct tag可以控制json encode的输出格式

```
type Person struct {
	Firstname  string `json:"first"`
	Middlename string `json:"middle,omitempty"` // omitempty可以省略
	Lastname   string `json:"last"`

	SSID int64 `json:"-"` // 不将这个字段编码输出

	City    string `json:"city,omitempty"`
	Country string `json:"country"`

	Telephone int64 `json:"tel,string"` // 输出类型为string
}
```

* interface slice

没办法把一个slice赋值给一个interface slice

```golang
var dataSlice []int = foo()
var interfaceSlice []interface{} = dataSlice
```

https://github.com/golang/go/wiki/InterfaceSlice

## profiling

1. CPU profiling runtime每隔10ms中断自己，然后记录下当前协程的stack trace
2. Memory profiling 只记录堆分配场景下的stack trace，栈分配则不会记录，也是采样的方式，每1000次分配就采样一次，这个比率可以改变。
3. 因为Memory profiling是基于堆分配了但是没有使用的内存进行采样的，所以用Memory profiling来探测程序的内存使用量是很难的。
4. Block profiling和记录协程花在等待共享资源上的时间，这个对于探测程序是否含有并发瓶颈很有帮助。
  * 发送和接收一个unbuffered的channel
  * 给一个full channel发送消息，或者从一个空的channel上接收消息
  * 试图调用sync.Mutex的Lock方法，但是被其他协程锁住
5. Mutex profiling

> Do not enable more than one kind of profile at a time.

关闭CPU变频，使CPU始终处于高频
```
$ sudo bash
# for i in /sys/devices/system/cpu/cpu[0-7]
do
      echo performance > $i/cpufreq/scaling_governor
      done
#
```
6.  `--inuse_objects `显示分配的内存数量而不是大小
7. pprof 基本使用(cpuprofile)

```
var cpuprofile = flag.String("cpuprofile", "", "write cpu profile to file")

func main() {
    flag.Parse()
    if *cpuprofile != "" {
        f, err := os.Create(*cpuprofile)
        if err != nil {
            log.Fatal(err)
        }
        pprof.StartCPUProfile(f)
        defer pprof.StopCPUProfile()
    }
```

## Programing Pattern

* 触发信号后，程序退出

程序通过一个stop信号来控制程序的生命的周期，在程序的最后调用WaitSignal来等待信号到来。

```golang
// WaitSignal awaits for SIGINT or SIGTERM and closes the channel
func WaitSignal(stop chan struct{}) {
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)
	<-sigs
	close(stop)
	_ = log.Sync()
}
```

## Go Pattern

* PubSub

* Worker Scheduler

* Replicated service client

* Protocol multiplexer

```golang
type ProtocolMux interface {
  Init(Service)
  Call(Msg) Msg
}

type Service interface {
  ReadTag(Msg) int64
  Send(Msg)
  Recv() Msg
}

type Mux struct {
  srv Service
  send  chan Msg
  mu sync.Mutex
  pending map[int64]chan<- Msg
}
```

## Concurrency

* sync.Pool

```golang

myPool := &sync.Pool{
    // 自定义对象的创建
    New: func() interface{} {
        fmt.Println("Creating new instance.")
        return struct{}{}
    },
}

// 创建一个对象，但是没有放到Pool中进行复用
myPool.Get()
// 创建一个对象
instance := myPool.Get()
// 将创建的对象放到Pool中复用
myPool.Put(instance)
```

* `runtime.GOMAXPROCS(runtime.NumCPU())` 指的是OS线程数，就是M的角色

* Golang调度器中的三个实体
  1. Goroutine(G)
  2. OS thread or machine (M)
  3. Context or processor (P)

* 避免往close的channel写入

```go
// 将channel的write和close控制在一个作用域内，保证不会出现write一个close的channel
chanOwner := func() <-chan int {
    results := make(chan int, 5)
    go func() {
        defer close(results)
        for i := 0; i <= 5; i++ {
            results <- i
        }
    }()
    return results
}

// 返回只读channel，限制channel的功能
consumer := func(results <-chan int) {
    for result := range results {
        fmt.Printf("Received: %d\n", result)
    }
    fmt.Println("Done receiving!")
}

results := chanOwner()
consumer(results)
```

* or-channel 用来select不定数量的channel，通过递归的方式来实现

```go
var or func(channels ...<-chan interface{}) <-chan interface{}
or = func(channels ...<-chan interface{}) <-chan interface{} {
    switch len(channels) {
    case 0:
        return nil
    case 1:
        return channels[0]
    }

    orDone := make(chan interface{})
    go func() {
        defer close(orDone)

        switch len(channels) {
        case 2:
            select {
            case <-channels[0]:
            case <-channels[1]:
            }
        default:
            select {
            case <-channels[0]:
            case <-channels[1]:
            case <-channels[2]:
            case <-or(append(channels[3:], orDone)...):
            }
        }
    }()
    return orDone
}
```

* 协程中发生的错误也应该通过channel暴露出来，将error和结果封装在一起

```golang
type Result struct {
    Error error
    // 协程通过channel返回的结果
}
```

* FanIn的实现，将多个channel汇聚到一个channel输出

```go
fanIn := func(
    done <-chan interface{},
    channels ...<-chan interface{},
) <-chan interface{} {
    var wg sync.WaitGroup
    multiplexedStream := make(chan interface{})

    multiplex := func(c <-chan interface{}) {
        defer wg.Done()
        for i := range c {
            select {
            case <-done:
                return
            case multiplexedStream <- i:
            }
        }
    }

    // Select from all the channels
    wg.Add(len(channels))
    for _, c := range channels {
        go multiplex(c)
    }

    // Wait for all the reads to complete
    go func() {
        wg.Wait()
        close(multiplexedStream)
    }()

    return multiplexedStream
}
```

* orDone channel

```go
orDone := func(done, c <-chan interface{}) <-chan interface{} {
    valStream := make(chan interface{})
    go func() {
        defer close(valStream)
        for {
            select {
            case <-done:
                return
            case v, ok := <-c:
                if ok == false {
                    return
                }
                select {
                case valStream <- v:
                case <-done:
                }
            }
        }
    }()
    return valStream
}
```

* tee channel

```go
tee := func(
    done <-chan interface{},
    in <-chan interface{},
) (_, _ <-chan interface{}) { <-chan interface{}) {
    out1 := make(chan interface{})
    out2 := make(chan interface{})
    go func() {
        defer close(out1)
        defer close(out2)
        for val := range orDone(done, in) {
            var out1, out2 = out1, out2
            for i := 0; i < 2; i++ {
              select {
                case <-done:
                // 第一次写入后会变成nil，后面再写入就会block了，就轮到下一个channel写入了
                case out1<-val:
                    out1 = nil
                case out2<-val:
                    out2 = nil
                }
            }
        }
    }()
    return out1, out2
}
```

* bridge channel

```go
bridge := func(
    done <-chan interface{},
    // 类型是<-chan interface{}的chan
    chanStream <-chan <-chan interface{},
) <-chan interface{} {
    valStream := make(chan interface{})
    go func() {
        defer close(valStream)
        for {
            var stream <-chan interface{}
            select {
            // 获取一个chan
            case maybeStream, ok := <-chanStream:
                if ok == false {
                    return
                }
                stream = maybeStream
            case <-done:
                return
            }
            // 遍历这个chan
            for val := range orDone(done, stream) {
                select {
                case valStream <- val:
                case <-done:
                }
            }
        }
    }()
    return valStream
}
```

## Garbage Collection

垃圾回收的三个阶段:

* Mark Setup - STW
打开写屏障，因为应用程序的协程和垃圾回收器的协程是并发运行的，为了保证数据的完整性，需要打开写屏障。打开写屏障需要让所有协程
都停止工作。

* Marking - Concurrent
* Mark Termination - STW



## 如何保证文件锁不会释放


```golang

go 里创建一个 file 时，会默认设置一个 Finalizer，当这个 File 回收时触发，关闭文件 fd，防止 fd 泄露。

在进程不需要文件锁之前，必须要保证文件锁的 file 对象一直是 reachable。可以使用 runtime.KeepAlive。如：

func main() {
    r := runner{}
    if err := r.lock(); err != nil {
        fmt.Printf("%s\n", err)
        return
    }
    defer runtime.KeepAlive(r.f)

    r.run()
}
```