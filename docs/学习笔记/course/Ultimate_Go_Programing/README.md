---
hide:
  - toc        # Hide table of contents
---

# Ultimate Go Programing
## Lesson 1: Design Guidelines

### Philosophy

1. Open Your Mind:

	* Technology changes quickly but people's minds change slowly.
	* Easy to adopt new technology but hard to adopt new ways of thinking.

2. Interesting Questions - What do they mean to you?

	* Is it a good program?
	* Is it an efficient program?
	* Is it correct?
	* Was it done on time?
	* What did it cost?

3. Aspire To

	* Be a champion for quality, efficiency and simplicity.
	* Have a point of view.
	* Value introspection and self-review.

4. Reading Code

	!!! tip
		“If most computer people lack understanding and knowledge, then what they will select will also be lacking.” - Alan Kay
		
		"The software business is one of the few places we teach people to write before we teach them to read." - Tom Love (inventor of Objective C)
		
		"Code is read many more times than it is written." - Dave Cheney
		
		"Programming is, among other things, a kind of writing. One way to learn writing is to write, but in all other forms of writing, one also reads. 
		
		We read examples both good and bad to facilitate learning. But how many programmers learn to write programs by reading programs?" - Gerald M. Weinberg
		
		"Skill develops when we produce, not consume." - Katrina Owen

5. Productivity vs Performance

	遵循Go的idioms和guideliness，我们将会同时拥有性能和开发效率，在过于这是需要二选一的，而大家更多是选择了开发效率，并希望通过硬件的发展来提高性能。这也导致了为此设计的语言产生了大量低效的软件。

6. Correctness vs Performance

	您想要编写针对正确性进行了优化的代码。不要根据您认为可能会更好的性能来做出编码决策。您必须进行基准测试以了解代码是否不够快以此来决定是否需要优化。
	!!! tip
		"Make it correct, make it clear, make it concise, make it fast. In that order." - Wes Dyer
		
		"Good engineering is less about finding the "perfect" solution and more about understanding the tradeoffs and being able to explain them." - JBD
		
		"The correctness of the implementation is the most important concern, but there is no royal road to correctness. It involves diverse tasks such as thinking of invariants, testing and code reviews. Optimization should be done, but not prematurely." - Al Aho (inventor of AWK)
		
		"The basic ideas of good style, which are fundamental to write clearly and simply, are just as important now as they were 35 years ago. Simple, straightforward code is just plain easier to work with and less likely to have problems. As programs get bigger and more complicated, it's even more important to have clean, simple code." - Brian Kernighan
		
		"Problems can usually be solved with simple, mundane solutions. That means there's no glamorous work. You don't get to show off your amazing skills. You just build something that gets the job done and then move on. This approach may not earn you oohs and aahs, but it lets you get on with it." - Jason Fried

7. Code Reviews

	没有设计哲学，您就无法看一段代码，函数或算法，并确定它是好是坏。这四个主要类别是代码审查的基础，应按此顺序排列优先级：完整性，可读性，简单性和性能。您必须有意识地并且有充分的理由能够解释您选择的类别。

* Integrity
We need to become very serious about reliability.

    1. Integrity is about every allocation, read and write of memory being accurate, consistent and efficient. The type system is critical to making sure we have this micro level of integrity.
    2. Integrity is about every data transformation being accurate, consistent and efficient. Writing less code and error handling is critical to making sure we have this macro level of integrity.

Write Less Code
Error Handling

* Readability

我们必须构造我们的系统以使其更易于理解

这是关于编写简单的代码，这些代码易于阅读和理解，而无需花费精力。同样重要的是，它不隐藏每行代码，功能，程序包及其运行的整体生态系统的成本/影响。

Code Must Never Lie

* Simplicity
我们必须了解，简单性很难设计，而且构建起来很复杂。

这是关于隐藏复杂性。必须将许多维护和设计变得简单，因为这可能导致更多的问题。它可能会引起可读性问题，并可能导致性能问题。


> "Simplicity is a great virtue but it requires hard work to achieve it and education to appreciate it. And to make matters worse: complexity sells better." - Edsger W. Dijkstra
> "Everything should be made as simple as possible, but not simpler." - Albert Einstein
> "You wake up and say, I will be productive, not simple, today." - Dave Cheney

封装是我们40年来一直试图解决的问题。 Go对该软件包采取了一种新的方法。提升封装水平，并在语言级别提供更丰富的支持。

> "The purpose of abstraction is not to be vague, but to create a new semantic level in which one can be absolutely precise - Edsger W. Dijkstra

* Performance
我们必须减少计算以获得所需的结果。

Rules of Performance:

    * Never guess about performance.
    * Measurements must be relevant.
    * Profile before you decide something is performance critical.
    * Test to know you are correct.

* Micro-Optimizations

微观优化是要尽可能地压缩每个代码快性能。当以此优先级编写代码时，很难编写可读，简单或惯用的代码。您正在编写巧妙的代码，这些代码可能需要`unsafe`的软件包，或者可能需要放入汇编中。


## Lesson2: Language Syntax

### Variable

* When variables are being declared to their zero value, use the keyword var.
* When variables are being declared and initialized, use the short variable declaration operator.

* 小心`:=`赋值

```go
package main

import (
	"fmt"
	"os"
)

func main() {
	var data []string

	killswitch := os.Getenv("KILLSWITCH")

	if killswitch == "" {
		fmt.Println("kill switch is off")
		// data被当作全新的变量，覆盖了上面的data
		data, err := getData()

		if err != nil {
			panic("ERROR!")
		}

		fmt.Printf("Data was fetched! %d\n", len(data))
	}

	for _, item := range data {
		fmt.Println(item)
	}
}

func getData() ([]string, error) {
	// Simulating getting the data from a datasource - lets say a DB.
	return []string{"there","are","no","strings","on","me"}, nil
}
```

#### Reference

[What's in a name?](https://talks.golang.org/2014/names.slide#1)

### Struct

* Memory alignment

> All memory is allocated on an alignment boundary to minimize memory defragmentation. To determine the alignment boundary Go is using for your architecture, you can run the unsafe.Alignof function. The alignment boundary in Go for the 64bit Darwin platform is 8 bytes. So when Go determines the memory allocation for our structs, it will pad bytes to make sure the final memory footprint is a multiple of 8. The compiler will determine where to add the padding.

> 为什么要填充1个字节呢？,这个目的是为了让整体的大小是字的倍数，字长一本都是2、4、8、16这样的2的幂次方，对于64位的CPU来说，这里只需要填充1个字节就可以满足这个要求。

CPU每次读取内存都是按照一个字来读取，不同的CPU架构所代表的大小不同，对于64位的CPU来说，一个字就是8个字节，CPU每次读和写都只能操作一个字的内存，因此为了高效的读取变量，我们应该让变量
占用的内存控制在一个字内，而不是跨两个字，这样会导致CPU多读取一次。为此我们需要对内存进行padding，以保证变量不会跨多个字存放。比如下面这个结构体。

```go
type Example struct{
    BoolValue bool
    IntValue  int16
    FloatValue float32
}
```

上面的`struct`最终会padding一个字节，总占用为8个字节，通过代码[paddding.go](Lesson2/alignment.go)可以看出，`BoolValue`的地方填充了一个字节。

> 为什么是在`BoolValue`处填充一个字节呢?，而不是在`IntValue`的旁边填充？ 或者是在`FloatValue`的旁边填充呢? 
> 可以试着枚举一下，如果是在`IntValue`处填充1个字节，那么如果字长是2字节，那么`IntValue`就会跨多个字存放(一半在前一个字，一半在后一个字)，为了解决这个问题，只能是在`BoolValue`处填充。
> 大家可以试着枚举下，假设位长位2、4、8的时候，在哪里填充可以避免跨多个字存放，最后的结论就是在`BoolValue`处存放最佳。


* Anonymous struct

Go中是不允许隐式类型转换的，两个不同的struct即使是具有完全相同的字段和顺序，也不能进行隐式转换。但是通过匿名struct就可以做了。具体代码见[anonymous_struct.go](Lesson2/anonymous_struct.go)

> "Implicit conversion of types is the Halloween special of coding. Whoever thought of them deserves their own special hell." - Martin Thompson
> 我们应该尽可能去避免使用隐式转换。


* Embedded Types
结构体类型可以包含匿名或嵌入式字段。这也称为嵌入类型，当我们将一个类型嵌入到结构中时，该类型的名称将充当随后嵌入字段的字段名称。

```go
type Admin struct {
    User
    Level string
}
```

这并非继承，而是一种组合模式，接着我们来看看如何创建带有嵌入式字段的`struct`

```go
func main() {
    admin := &Admin{
		// 和创建普通的struct一样，使用类型名作为字段名
        User: User{
            Name:  "john smith",
            Email: "john@email.com",
        },
        Level: "super",
    }

    SendNotification(admin)
}

// Output
User: Sending User Email To john smith<john@email.com>
```

通过这种方式的组合，使得Admin实现User所有的接口。

* Reference
[Understanding Type in Go](https://www.ardanlabs.com/blog/2013/07/understanding-type-in-go.html)
[Object Oriented Programming in Go](https://www.ardanlabs.com/blog/2013/07/object-oriented-programming-in-go.html)
[Padding is hard](https://dave.cheney.net/2015/10/09/padding-is-hard)


### Pointer

指针语义和值语义，前者是共享的，存在副作用，涉及到data race、逃逸分析等，而后者无副作用，但是存在拷贝开销。

在Go中是goroutine中是没办法指向另外一个goroutine的栈的，这是因为goroutine的栈是会增长的，如果发生增长会导致栈被拷贝到另外一个更大的栈空间上，这就导致了指针失效了。下面这段代码演示了栈增长的情况。

```go
package main

// Number of elements to grow each stack frame.
// Run with 10 and then with 1024
const size = 1024

// main is the entry point for the application.
func main() {
	s := "HELLO"
	stackCopy(&s, 0, [size]int{})
}

// stackCopy recursively runs increasing the size
// of the stack.
func stackCopy(s *string, c int, a [size]int) {
	println(c, s, *s)

	c++
	if c == 10 {
		return
	}

	stackCopy(s, c, a)
}
```

> 与 GCC 相似，在 Golang 的 goroutine 的实现中也应用了类似的技术。在 1.3 版本及以前采用的是分段栈的实现，在初始时会对每个 goroutine 分配 8KB 的内存，而在 goroutine 内部每个函数的调用时，会检查栈空间是否足够使用，
> 若不够则调用 morestack 进行额外的栈空间申请，申请完毕后连接到旧栈空间上，在函数结束时会调用 lessstack 来回收多余的栈空间。由于栈的缩减是一个相对来说开销较大的逻辑，尤其在一个较深的递归中，会有较多的 morestack 和 lessstack 调用，
> 这种问题被成为热分裂问题。Golang 1.4 通过栈复制法来解决这个问题，在栈空间不足时，不会申请一个新的栈空间块链接到老的栈空间快上，而是创建一个原来两倍大小的栈空间块，并将旧栈信息拷贝至新栈中。这样对于栈的缩减，没有多余的开销，
> 同时在第二次拓展栈时，也无需再次申请空间。针对栈复制中指针的问题，由于垃圾回收机制的存在，可以找到哪部分的栈使用了指针，通过对应可以将指针地址进行相应的更新。


* 逃逸分析

1. 当函数中返回的值被函数外所引用的时候，会导致这个值进行了逃逸，分配在堆上，例如下面这个例子

```go
package main

type user struct {
	name  string
	email string
}

func main() {
	u1 := createUser()

	println("u1", &u1)
}

// 这里返回的指针指向栈上分配的user，这会导致逃逸分析生效
// 将user分配在堆上，避免因为函数结束栈被清理导致指针失效。
// 避免内联，否则就没有逃逸分析了
//go:noinline
func createUser() *user {
	u := user{
		name:  "Bill",
		email: "bill@ardanlabs.com",
	}

	println("U", &u)
	return &u
}
```

逃逸分析的结果如下[escape1.go](Lesson2/escape/escape1.go):

```bash
./escape.go:17:6: cannot inline createUserV1: marked go:noinline
./escape.go:31:6: cannot inline createUserV2: marked go:noinline
./escape.go:8:6: cannot inline main: function too complex: cost 133 exceeds budget 80
./escape.go:32:2: u escapes to heap:
./escape.go:32:2:   flow: ~r0 = &u:
./escape.go:32:2:     from &u (address-of) at ./escape.go:38:9
./escape.go:32:2:     from return &u (return) at ./escape.go:38:2
./escape.go:32:2: moved to heap: u
```

2. 当编译器发现一个值的大小无法在栈上存放的时候会将其分配在堆上


3. 当编译器无法在编译时知道值的大小时就选择在堆上进行分配

```go
package main

import (
	"bytes"
)

func main() {
    size := 10
    // size的值是编译时无法知道的，如果这里改成10，就不会有逃逸分析了
	b := make([]byte, size)
	c := bytes.NewBuffer(b)
	c.WriteString("test")
}
```

逃逸分析的结果如下[escape2.go](Lesson2/escape/escape2.go):

```bash
./escape2.go:7:6: cannot inline main: function too complex: cost 84 exceeds budget 80
./escape2.go:10:22: inlining call to bytes.NewBuffer func([]byte) *bytes.Buffer { return &bytes.Buffer literal }
./escape2.go:9:11: make([]byte, size) escapes to heap:
./escape2.go:9:11:   flow: {heap} = &{storage for make([]byte, size)}:
./escape2.go:9:11:     from make([]byte, size) (non-constant size) at ./escape2.go:9:11
./escape2.go:9:11: make([]byte, size) escapes to heap
./escape2.go:10:22: &bytes.Buffer literal does not escape
```

4. 当变量赋值给interface或者函数的时候会导致逃逸

```go
package main

import (
	"bytes"
)

//go:noinline
func InterfaceMethod(value interface{}) {
}

func main() {
	size := 10
	b := make([]byte, size)
	c := bytes.NewBuffer(b)
	c.WriteString("test")
	// 导致逃逸分析
	InterfaceMethod(c)
}

```

逃逸分析的结果如下[escape3.go](Lesson2/escape/escape3.go):

```bash
./escape3.go:8:6: cannot inline InterfaceMethod: marked go:noinline
./escape3.go:11:6: cannot inline main: function too complex: cost 145 exceeds budget 80
./escape3.go:14:22: inlining call to bytes.NewBuffer func([]byte) *bytes.Buffer { return &bytes.Buffer literal }
./escape3.go:8:22: value does not escape
./escape3.go:13:11: make([]byte, size) escapes to heap:
./escape3.go:13:11:   flow: {heap} = &{storage for make([]byte, size)}:
./escape3.go:13:11:     from make([]byte, size) (non-constant size) at ./escape3.go:13:11
./escape3.go:13:11: make([]byte, size) escapes to heap
./escape3.go:14:22: &bytes.Buffer literal does not escape
```

5. 赋值给指针导致的逃逸

```go
func BenchmarkAssignmentIndirect(b *testing.B) {
	type X struct {
		p *int
	}
	for i := 0; i < b.N; i++ {
		var i1 int
		x1 := &X{
			p: &i1, // GOOD: i1 does not escape
		}
		_ = x1

		var i2 int
		x2 := &X{}
		x2.p = &i2 // BAD: Cause of i2 escape
	}
}
```

逃逸分析的结果如下[example1_test.go](Lesson2/escape/flaws/example1/example1_test.go)

```bash
./example1_test.go:5:6: cannot inline BenchmarkAssignmentIndirect: unhandled op DCLTYPE
./example1_test.go:16:7: i2 escapes to heap:
./example1_test.go:16:7:   flow: {heap} = &i2:
./example1_test.go:16:7:     from &i2 (address-of) at ./example1_test.go:18:10
./example1_test.go:16:7:     from x2.p = &i2 (assign) at ./example1_test.go:18:8
./example1_test.go:5:34: b does not escape
./example1_test.go:16:7: moved to heap: i2
./example1_test.go:11:9: &X literal does not escape
./example1_test.go:17:9: &X literal does not escape
```

如果是赋值给指针的指针也是会导致逃逸的。

```go
package main

func main() {
	i := 0
	pp := new(*int)
	*pp = &i // BAD: i escapes
	_ = pp
}
```

逃逸分析的结果如下[escape4.go](Lesson2/escape/escape3.go)

```bash
./escape4.go:3:6: can inline main with cost 20 as: func() { i := 0; pp := new(*int); *pp = &i; _ = pp }
./escape4.go:4:2: i escapes to heap:
./escape4.go:4:2:   flow: {heap} = &i:
./escape4.go:4:2:     from &i (address-of) at ./escape4.go:6:8
./escape4.go:4:2:     from *pp = &i (assign) at ./escape4.go:6:6
./escape4.go:4:2: moved to heap: i
./escape4.go:5:11: new(*int) does not escape
```


6. 赋值给


对于接口的使用意见:

Use an interface when:

1. users of the API need to provide an implementation detail.
2. API’s have multiple implementations they need to maintain internally.
3. parts of the API that can change have been identified and require decoupling.

Don’t use an interface:

1. for the sake of using an interface.
2. to generalize an algorithm.
3. when users can declare their own interfaces.

> `go build -gcflags "-m -m"` 添加gcflags可以显示处哪些变量进行了逃逸


### Garbage Collection

垃圾回收几个阶段:

1. Mark Setup - STW

在这个阶段需要找到一个安全点，让所有的goroutine都停下来，在1.14之前，Go中的函数调用是一个安全点，当gorountine执行函数调用的时候就停止，但是这个存在一个问题，如果一个goroutine
一直在做密集的计算，没有进行函数调用，这会导致GC的采集停止不前。在1.14后Go中引入了抢占来解决这个问题。

[runtime: non-cooperative goroutine preemption](https://github.com/golang/go/issues/24543)

2. Marking - Concurrent

在标记阶段，垃圾回收会控制自己所占用的CPU为整体的1/4(其他的CPU用于程序运行)，在这个阶段，GC会扫描goroutine的栈，找到栈中指向堆的指针进行标记。标记阶段的吞吐量为`1MB/ms * 1/4 * CPU核心数`
如果在标记阶段，应用分配内存的速度大于标记的速度，这个时候会启用`Mark Assist`占用更多的CPU来协助进行标记。但是不会占用太多的`Mark Assist`，与其这样占用太多`Mark Assist`，还不如今早开启下一次GC回收。
所以GC会控制`Mark Assist`的数量。


3. Mark Termination - STW

标记阶段的最后工作是Mark Termination，关闭内存屏障，停止后台标记以及辅助标记，做一些清理工作，整个过程也需要STW，大概需要60-90微秒。在此之后，所有的P都能继续为应用程序G服务了。

4. Sweeping - Concurrent

在标记工作完成之后，剩下的就是清理过程了，清理过程的本质是将没有被使用的内存块整理回收给上一个内存管理层级(mcache -> mcentral -> mheap -> OS)，清理回收的开销被平摊到应用程序的每次内存分配操作中，
直到所有内存都Sweeping完成。当然每个层级不会全部将待清理内存都归还给上一级，避免下次分配再申请的开销，比如Go1.12对mheap归还OS内存做了优化，使用NADV_FREE延迟归还内存。



* GC percentage
runtime中有一个配置选项叫做 GC Percentage，默认值是100。这个值代表了下一次回收开始之前，有多少新的堆内存可以分配。GC Percentage设置为100意味着，基于回收完成之后被标记为生存的堆内存数量，下一次回收的开始必须在有100%以上的新内存分配到堆内存时启动。
如果新分配的内存并没有到达100%就触发了下一次GC，这个可能是因为应用内存分配速度太快，GC不希望分配太多的`Mark Assist`，因此尽快的启动了下一次GC。


* GC Trcae

1. `GODEBUG=gctrace=1` 开启GC Trace
2. `GODEBUG=gctrace=1,gcpacertrace=1` 开启更详细的GC Trace

```shell
gc 1405 @6.068s 11%: 0.058+1.2+0.083 ms clock, 0.70+2.5/1.5/0+0.99 ms cpu, 7->11->6 MB, 10 MB goal, 12 P
gc 1406 @6.070s 11%: 0.051+1.8+0.076 ms clock, 0.61+2.0/2.5/0+0.91 ms cpu, 8->11->6 MB, 13 MB goal, 12 P
gc 1407 @6.073s 11%: 0.052+1.8+0.20 ms clock, 0.62+1.5/2.2/0+2.4 ms cpu, 8->14->8 MB, 13 MB goal, 12 P

字段含义如下:
// General
gc 1405     : The 1405 GC run since the program started
@6.068s     : Six seconds since the program started
11%         : Eleven percent of the available CPU so far has been spent in GC

// Wall-Clock
0.058ms     : STW        : Mark Start       - Write Barrier on
1.2ms       : Concurrent : Marking
0.083ms     : STW        : Mark Termination - Write Barrier off and clean up

// CPU Time
0.70ms      : STW        : Mark Start
2.5ms       : Concurrent : Mark - Assist Time (GC performed in line with allocation)
1.5ms       : Concurrent : Mark - Background GC time
0ms         : Concurrent : Mark - Idle GC time
0.99ms      : STW        : Mark Term

// Memory
7MB         : Heap memory in-use before the Marking started
11MB        : Heap memory in-use after the Marking finished
6MB         : Heap memory marked as live after the Marking finished
10MB        : Collection goal for heap memory in-use after Marking finished

// Threads
12P         : Number of logical processors or threads used to run Goroutines
```

GC调优的意见:

1. Maintain the smallest heap possible.
2. Find an optimal consistent pace.
3. Stay within the goal for every collection.
4. Minimize the duration of every collection, STW and Mark Assist.


### Compiler And Runtime Optimizations

1. Non-scannable objects
Garbage collector does not scan underlying buffers of slices, channels and maps when element type does not contain pointers (both key and value for maps). 
垃圾回收期不会扫描元素不是指针类型的map、slices、channnel。下面这个map就不会影响GC的采集时间。

```go
type Key [64]byte // SHA-512 hash
type Value struct {
	Name      [32]byte
	Balance   uint64
	Timestamp int64
}
m := make(map[Key]Value, 1e8)
```

2. Function Inlining

只有小的、短的函数才会内联，函数中要小于40个表达式、并且不包含复杂的语句，比如loop、labels、closures、panic、recover、select、switch等

3. `go build -x`

显示build的过程

4. `go build -gcflags="-S"`

显示golang中间汇编结果

5. `go tool objdump -s main.main hello`

二进制反汇编

6. `go tool nm escape1`

查看二进制符号信息

7. `GOSSAFUNC=main go build && open ssa.html`

SSA 代表 static single-assignment，是一种IR(中间表示代码)，要保证每个变量只被赋值一次。

8. `go build -gcflags="-m"`

逃逸分析

9. `go build -gcflags="-l -N"`

禁止优化和禁止内联



### Constans

在Go中是不能在不同的数字类型的变量之间做操作的，比如不能用float64和int之间做操作，需要强制类型转换，但是有的时候我们发现我们可以做类似`2 * time.Second`、`1 << ('t' + 2.0)`
这样的操作，这是因为他们都是常量，并非是变量。

* 常量存在默认类型

```go
fmt.Printf("%T %v\n", 0, 0)
fmt.Printf("%T %v\n", 0.0, 0.0)
fmt.Printf("%T %v\n", 'x', 'x')
fmt.Printf("%T %v\n", 0i, 0i)
	
// 输出结果
int 0
float64 0
int32 120
complex128 (0+0i)
```


* 不同类型的常量操作，会进行类型转换

转换规则按照integer, rune, floating-point, complex.的先后顺序

```go
var answer = 3 * 0.33	// 按照上面的规则，integer会转换为floating-point，最终结果就是浮点数了

```

* 数字常量可以说是integer, floating-point, complex and rune等四种kind，此外还有bool、string两种kind类型的常量。
* 常量不是变量
* 常量只存在于编译时
* 无类型的常量可以隐式转换为有类型的常量，但是变量不行需要强制转换

```go
type Duration int64
// Common durations. There is no definition for units of Day or larger
// to avoid confusion across daylight savings time zone transitions.
const (
        Nanosecond  Duration = 1
        Microsecond          = 1000 * Nanosecond
        Millisecond          = 1000 * Microsecond
        Second               = 1000 * Millisecond
        Minute               = 60 * Second
        Hour                 = 60 * Minute
)
// Add returns the time t+d.
func (t Time) Add(d Duration) Time

func main() {

	// Use the time package to get the current date/time.
	now := time.Now()

	// 这里的5转换成常量Duration了
	// Subtract 5 nanoseconds from now using a literal constant.
	literal := now.Add(-5)

	// Subtract 5 seconds from now using a declared constant.
	const timeout = 5 * time.Second // time.Duration(5) * time.Duration(1000000000)
	constant := now.Add(-timeout)

	// Subtract 5 nanoseconds from now using a variable of type int64.
	minusFive := int64(-5)
	variable := now.Add(minusFive)

	// example4.go:50: cannot use minusFive (type int64) as type time.Duration in argument to now.Add

	// Display the values.
	fmt.Printf("Now     : %v\n", now)
	fmt.Printf("Literal : %v\n", literal)
	fmt.Printf("Constant: %v\n", constant)
	fmt.Printf("Variable: %v\n", variable)
}
```

* 无类型的常量有Kind，但是没有类型

```go
	// 无类型常量，精度理论上无限制
	// Untyped Constants.
	const ui = 12345    // kind: integer
	const uf = 3.141592 // kind: floating-point

	// 下面就是有类型的常量了，这是有精度限制的，取决于常量类型
	// Typed Constants still use the constant type system but their precision
	// is restricted.
	const ti int = 12345        // type: int
	const tf float64 = 3.141592 // type: float64
	// 这里声明uint8的值为1000就超过限制了，会编译报错，但是改成无类型的常量就没有这个限制了
	// ./constants.go:XX: constant 1000 overflows uint8
	// const myUint8 uint8 = 1000
	
	// 有类型和无类型进行算术运算会进行类型转换。
	const one int8 = 1
	const two = 2 * one // int8(2) * int8(1)
```

* integer常量精度至少256bits

```go
const (
	// Max integer value on 64 bit architecture.
	maxInt = 9223372036854775807

	// Much larger value than int64.
	bigger = 9223372036854775808543522345

	// 有类型的常量受到了类型的精度限制，无类型常量则没有限制
	// Will NOT compile
	// biggerInt int64 = 9223372036854775808543522345
)
```


### Array

面向数据的设计原则:

* If you don't understand the data, you don't understand the problem.
* All problems are unique and specific to the data you are working with.
* Data transformations are at the heart of solving problems. Each function, method and work-flow must focus on implementing the specific data transformations required to solve the problems.
* If your data is changing, your problems are changing. When your problems are changing, the data transformations needs to change with it.
* Uncertainty about the data is not a license to guess but a directive to STOP and learn more.
* Solving problems you don't have, creates more problems you now do.
* If performance matters, you must have mechanical sympathy for how the hardware and operating system work.
* Minimize, simplify and REDUCE the amount of code required to solve each problem. Do less work by not wasting effort.
* Code that can be reasoned about and does not hide execution costs can be better understood, debugged and performance tuned.
* Coupling data together and writing code that produces predictable access patterns to the data will be the most performant.
* Changing data layouts can yield more significant performance improvements than changing just the algorithms.
* Efficiency is obtained through algorithms but performance is obtained through data structures and layouts.

我们在设计数据结构的时候需要考虑数据的存储形式，应该尽可能的考虑到对底层硬件平台的依赖，比如需要考虑到缓存，设计的数据结构需要对缓存友好，
一般来说，链表的是缓存不友好的，而数组这种连续内存的数据结构是缓存友好的，可以充分利用缓存来加速。

* CPU的缓存主要是通过将主内存中的数据缓存在cache line上
* Cache line目前一般是32个字节或者是64个字节，这个取决于对应的硬件平台
* CPU核心不会直接访问主内存，他们往往只能访问本地的缓存
* 数据和指令都可以存在缓存中
* 高速缓存行按L1-> L2-> L3的顺序排列，因为新的高速缓存行需要存储在高速缓存中。
* 硬件喜欢沿着Cache line线性的访问数据和指令
* 主内存建立在相对较快的廉价内存上。高速缓存建立在非常快速且昂贵的内存上。
* 访问主内存的速度非常慢，我们需要缓存。
	* 从主存储器访问一个字节将导致读取并缓存整个缓存行。
	* 在高速缓存行中写入一个字节需要写入整个高速缓存行。
* 小 等于 快
	* 适合放入缓存中紧凑数据结构是最快的
	* 仅遍历缓存的数据是最快的

* 可预测的访问模式最重要
	* 只要可行，尽可能使用线性数组遍历
	* 提供常规的内存访问模式
	* 硬件可以对所需的内存做出更好的预测

* 缓存未命中也会导致TLB缓存未命中
	* 将虚拟地址转换为物理地址需要Cache
	* 需要等待OS告诉我们真正要访问的内存在哪里

```
3GHz(3 clock cycles/ns) * 4 instructions per cycle = 12 instructions per ns!

1 ns ............. 1 ns .............. 12 instructions  (one) 
1 µs .......... 1000 ns .......... 12,000 instructions  (thousand)
1 ms ..... 1,000,000 ns ...... 12,000,000 instructions  (million)
1 s .. 1,000,000,000 ns .. 12,000,000,000 instructions  (billion)

L1 - 64KB Cache (Per Core)
	4 cycles of latency at 1.3 ns
	Stalls for 16 instructions

L2 - 256KB Cache (Per Core)
	12 cycles of latency at 4 ns
	Stalls for 48 instructions

L3 - 8MB Cache
	40 cycles of latency at 13.3 ns
	Stalls for 160 instructions

Main Memory
	100 cycle of latency at 33.3 ns
	Stalled for 400 instructions
```

```
L1 cache reference ......................... 0.5 ns ...................  6 ins
Branch mispredict ............................ 5 ns ................... 60 ins
L2 cache reference ........................... 7 ns ................... 84 ins
Mutex lock/unlock ........................... 25 ns .................. 300 ins
Main memory reference ...................... 100 ns ................. 1200 ins           
Compress 1K bytes with Zippy ............. 3,000 ns (3 µs) ........... 36k ins
Send 2K bytes over 1 Gbps network ....... 20,000 ns (20 µs) ........  240k ins
SSD random read ........................ 150,000 ns (150 µs) ........ 1.8M ins
Read 1 MB sequentially from memory ..... 250,000 ns (250 µs) .......... 3M ins
Round trip within same datacenter ...... 500,000 ns (0.5 ms) .......... 6M ins
Read 1 MB sequentially from SSD* ..... 1,000,000 ns (1 ms) ........... 12M ins
Disk seek ........................... 10,000,000 ns (10 ms) ......... 120M ins
Read 1 MB sequentially from disk .... 20,000,000 ns (20 ms) ......... 240M ins
Send packet CA->Netherlands->CA .... 150,000,000 ns (150 ms) ........ 1.8B ins
```

* range的语义

```go

// All material is licensed under the Apache License Version 2.0, January 2004
// http://www.apache.org/licenses/LICENSE-2.0

// Sample program to show how the for range has both value and pointer semantics.
package main

import "fmt"

func main() {

	// Using the pointer semantic form of the for range.
	friends := [5]string{"Annie", "Betty", "Charley", "Doug", "Edward"}
	fmt.Printf("Bfr[%s] : ", friends[1])
	// 这种是指针语义，通过下标来访问friends，不产生临时变量
	for i := range friends {
		friends[1] = "Jack"

		if i == 1 {
			fmt.Printf("Aft[%s]\n", friends[1])
		}
	}

	// Using the value semantic form of the for range.
	friends = [5]string{"Annie", "Betty", "Charley", "Doug", "Edward"}
	fmt.Printf("Bfr[%s] : ", friends[1])
	// 这种是值语义，v每次拷贝friends中的元素，修改friends不会影响v
	for i, v := range friends {
		friends[1] = "Jack"

		if i == 1 {
			fmt.Printf("v[%s]\n", v)
		}
	}

	// Using the value semantic form of the for range but with pointer
	// semantic access. DON'T DO THIS.
	friends = [5]string{"Annie", "Betty", "Charley", "Doug", "Edward"}
	fmt.Printf("Bfr[%s] : ", friends[1])
	// 不要这种写法，会存在data race的，因为v每次拷贝的是friends中元素的指针，修改 friends会影响v
	for i, v := range &friends {
		friends[1] = "Jack"

		if i == 1 {
			fmt.Printf("v[%s]\n", v)
		}
	}
}
```

### slice

slice、channel、map、function、interface都是引用类型，这些数据结构内部都有指针，默认值是nil。

* nil和empty不一样

```go
// 这里的s是nil，nil表示slice内部的指针、size、cap等都是0
var s []string
// 这里的s是empty，表示内部指针指向了一个全局的位置、size和cap都是0
s := []string {}
```

* 通过make来创建slice可以指定length和cap，如果只指定length的话，cap默认等于length

```go
func main() {

	// Create a slice with a length of 5 elements.
	fruits := make([]string, 5)
	fruits[0] = "Apple"
	fruits[1] = "Orange"
	fruits[2] = "Banana"
	fruits[3] = "Grape"
	fruits[4] = "Plum"

	// 会存在访问越界
	// You can't access an index of a slice beyond its length.
	fruits[5] = "Runtime error"

	// Error: panic: runtime error: index out of range

	fmt.Println(fruits)
}
```

* slice在cap小于1024的时候总是按照100%来增长，到了1024后则按照25%来增长。

具体细节见[cap.go](Lesson2/cap.go)

* 在slice的基础上可以再创建slice，创建的slice和之前的slice是共享底层的存储

```go
func main() {
	orgSlice := make([]string, 5, 8)
	orgSlice[0] = "Apple"
	orgSlice[1] = "Orange"
	orgSlice[2] = "Banana"
	orgSlice[3] = "Grape"
	orgSlice[4] = "Plum"
	fmt.Printf("cap: %d, length: %d\n", cap(orgSlice), len(orgSlice))

	// 可以通过orgSlice[2:4:cap]来指定cap，将cap和length设置为相同，这样就可以在
	// append的时候避免对原来的slice进行修改。其中cap不能超过原来slice的最大cap范围
	slice2 := orgSlice[2:4]
	// length为2，cap为6，这里需要小心了，因为cap和length不相同，因此在append的时候
	// 不会进行拷贝，而是直接在原来的基础上操作，这个是存在副作用的，会影响到orgSlice，例如下面这个例子
	fmt.Printf("cap: %d, length: %d\n", cap(slice2), len(slice2))

	//  Append会导致orgSlice中的元素被覆盖
	slice2 = append(slice2, "test")
	fmt.Printf("slice2: %v\n", slice2)
	fmt.Printf("orgSlice: %v %d\n", orgSlice, len(orgSlice))

	slice3 := orgSlice[2:]
	fmt.Printf("cap: %d, length: %d\n", cap(slice3), len(slice3))
	// 这里append不会影响orgSlice，因为orgSlice和slice3两个结束位置都是相同的，
	// 这里的append只会在未使用的区域添加新的元素，orgSlice感知不到。后续orgSlice
	// 如果也进行append的化，会导致两个Slice的内容相互覆盖了。
	slice3 = append(slice3, "test3")
	fmt.Printf("slice3: %v\n", slice3)
	fmt.Printf("orgSlice: %v %d\n", orgSlice, len(orgSlice))

}

// 输出
cap: 8, length: 5
cap: 6, length: 2
slice2: [Banana Grape test]
orgSlice: [Apple Orange Banana Grape test]
cap: 6, length: 3
slice3: [Banana Grape test test3]
orgSlice: [Apple Orange Banana Grape test] 5
```


* 引用slice中的元素时需要小心因为append带来的副作用 

```go
type user struct {
	likes int
}

func main() {

	// Declare a slice of 3 users.
	users := make([]user, 3)

	// Share the user at index 1.
	// 这里引用了slice中的元素
	shareUser := &users[1]

	// Add a like for the user that was shared.
	// 操作引用本身也会导致slice中对应元素发生变化
	shareUser.likes++

	// Display the number of likes for all users.
	for i := range users {
		fmt.Printf("User: %d Likes: %d\n", i, users[i].likes)
	}

	// Add a new user.
	// append会导致copy的发生，那么之前对slice中元素的引用和append后的slice是两个独立的slice，互不影响。
	users = append(users, user{})

	// Add another like for the user that was shared.
	// 这是在操作append前的slice
	shareUser.likes++

	// Display the number of likes for all users.
	fmt.Println("*************************")
	for i := range users {
		fmt.Printf("User: %d Likes: %d\n", i, users[i].likes)
	}

	// Notice the last like has not been recorded.
}

```

* copy函数只会拷贝两个slice中的最小长度。当两个slice存在重叠的时候，copy函数也可以正确工作

```go
// Insert inserts the value into the slice at the specified index,
// which must be in range.
// The slice must have room for the new element.
func Insert(slice []int, index, value int) []int {
    // Grow the slice by one element.
    slice = slice[0 : len(slice)+1]
    // Use copy to move the upper part of the slice out of the way and open a hole.
    copy(slice[index+1:], slice[index:])
    // Store the new value.
    slice[index] = value
    // Return the result.
    return slice
}
```

* 小心slice的迭代，值语义和指针语义

```go
package main

import "fmt"

func main() {

	// Using the value semantic form of the for range.
	friends := []string{"Annie", "Betty", "Charley", "Doug", "Edward"}
	// 值语义，这里会对friends进行拷贝，因此在迭代过程中修改friends不会影响迭代结果的
	// v每次都会对friends中的元素进行拷贝
	for _, v := range friends {
		friends = friends[:2]
		fmt.Printf("v[%s]\n", v)
	}

	// Using the pointer semantic form of the for range.
	friends = []string{"Annie", "Betty", "Charley", "Doug", "Edward"}
	// 指针语义，friends并不会拷贝，因此迭代器中修改friends会影响迭代
	for i := range friends {
		friends = friends[:2]
		fmt.Printf("v[%s]\n", friends[i])
	}
}
```

* 小心slice迭代，始终只有一个迭代器变量

```go
type Dog struct {
    Name string
    Age int
}

func main() {
    jackie := Dog{
        Name: "Jackie",
        Age: 19,
    }

    fmt.Printf("Jackie Addr: %p\n", &jackie)

    sammy := Dog{
        Name: "Sammy",
        Age: 10,
    }

    fmt.Printf("Sammy Addr: %p\n", &sammy)

    dogs := []Dog{jackie, sammy}

    fmt.Println("")
	// dog每次迭代都会拷贝一次
    for _, dog := range dogs {
        fmt.Printf("Name: %s Age: %d\n", dog.Name, dog.Age)
        fmt.Printf("Addr: %p\n", &dog)	// 这里输出的地址总是一样的

        fmt.Println("")
	}
	
	allDogs := []*Dog{}

	for _, dog := range dogs {
		// 这里会存在问题，因此存的都是指针，，但是dog变量只有一个，只是每次进行copy，因此这里最终append的都是最后一个元素
		allDogs = append(allDogs, &dog)
	}

	for _, dog := range allDogs {
		fmt.Printf("Name: %s Age: %d\n", dog.Name, dog.Age)
	}
}
```

* string其实就是slice的一个只读版本，也包含了指针和size，但是因为是只读的，所以没有cap字段

```go
package main

import (
	"fmt"
	"unicode/utf8"
)

func main() {

	// Declare a string with both chinese and english characters.
	s := "世界 means world"

	// UTFMax is 4 -- up to 4 bytes per encoded rune.
	var buf [utf8.UTFMax]byte

	// Iterate over the string.
	// 默认遍历string是按照rune来遍历的，一个rune是一个可变大小。
	// i指向这个rune在string中的offset
	for i, r := range s {

		// Capture the number of bytes for this rune.
		rl := utf8.RuneLen(r)

		// Calculate the slice offset for the bytes associated
		// with this rune.
		si := i + rl

		// Copy of rune from the string to our buffer.
		copy(buf[:], s[i:si])

		// Display the details.
		fmt.Printf("%2d: %q; codepoint: %#6x; encoded bytes: %#v\n", i, r, r, buf[:rl])
	}
}
```

### map

* map的迭代总是无序的
* map的key必须是可hash的、而且是可比较的，slice没办法作为key，因为不可比较

```go
package main

import "fmt"

// user represents someone using the program.
type user struct {
	name    string
	surname string
}

// users defines a set of users.
type users []user

func main() {

	// Declare and make a map that uses a slice as the key.
	// 这里的users是slice，是不可比较的，不能作为key
	u := make(map[users]int)

	// ./example3.go:22: invalid map key type users

	// Iterate over the map.
	for key, value := range u {
		fmt.Println(key, value)
	}
}

```

* map中的元素是不可寻址的

```go
package main

// player represents someone playing our game.
type player struct {
	name  string
	score int
}

func main() {

	// Declare a map with initial values using a map literal.
	players := map[string]player{
		"anna":  {"Anna", 42},
		"jacob": {"Jacob", 21},
	}

	// Trying to take the address of a map element fails.
	anna := &players["anna"]
	anna.score++

	// ./example4.go:23:10: cannot take the address of players["anna"]

	// Instead take the element, modify it, and put it back.
	player := players["anna"]
	player.score++
	players["anna"] = player
}
```

* 空map和nil是不同的

```go
	// 空map
	users := make(map[string]user)
	// nil
	var users map[string]user
```


### method

* 方法本质上是个带有receive的函数
* receiver会给方法绑定一个类型，可以是值语义也可以是指针语义
* 值语义意味着每次方法调用都是通过副本进行操作的
* 指针语义意味着每次方法调用都是共享相同的实例
* 坚持给定类型的单一语义并保持一致
* 用值语义还是指针语义
	1. 如果类型是一个map、func、chan，不要使用指针语义，如果类型是slice，并且没有reslice或者重新分配slice的需求，也不要使用指针语义
	2. 如果方法需要修改操作，那么必须使用指针语义
	3. 如果类型包含了锁、文件fd等不可拷贝的资源则必须要用指针语义
	4. 如果类型是大型的数据结构或者数组，那么为了效率应该使用指针
	5. 如果类型是数组或切片，并且其元素是指针，而且可能会被修改，则最好使用指针语义，因为它将使读者更加清楚意图。
	6. 如果类型是小的数组或者struct中包含了一些基本类型，并且没有指针，也没有可修改的字段。仅仅是一些基本类型，那么使用值语义可以减少gc的压力。
	7. 最后如有疑问请使用指针语义


### interface

* 接口本身就是引用类型，不需要通过指针来共享
* 如何判断一个类型是否实现了某个接口?

 1. 对于一个指针来说，其方法集包含了值语义和指针语义作为reciver实现的方法
 2. 对于一个值来说，其方法集仅限于使用值语义作为reciver实现的方法

下面这个例子中，user作为值来说，其方法集只有使用值作为receiver的方法，但是User的Notify是用指针作为receiver来实现的，因此user并没有实现Notify接口，
把它换成指针类型就可以了。

```go
type User struct {
    Name string
    Email string
}

func SendNotification(notify Notifier) error {
    return notify.Notify()
}
func (u *User) Notify() error {
    log.Printf("User: Sending User Email To %s<%s>\n",
        u.Name,
        u.Email)

    return nil
}

func main() {
    user := User{
        Name:  "janet jones",
        Email: "janet@email.com",
    }

    SendNotification(user)
}

// Output:
cannot use user (type User) as type Notifier in function argument:
      User does not implement Notifier (Notify method has pointer receiver)
```

* 嵌入式类型其包含的方法集和外部类型是什么关系?

```go
// Admin包含了嵌入式类型User，因此User实现的Notify接口，Admin也实现了。
type Admin struct {
    User
    Level string
}

func main() {
    admin := &Admin{
        User: User{
            Name:  "john smith",
            Email: "john@email.com",
        },
        Level: "super",
    }

	SendNotification(admin)
	// 也可以这样来调用，因为嵌入类型在外部类型中就是一个字段名为类型名的字段。
	// admin.User.Notify()

}

// Output
User: Sending User Email To john smith<john@email.com>
```

> 当我们嵌入一个类型时，该类型的方法成为外部类型的方法，但是当它们被调用时，该方法的接收者是内部类型，而不是外部类型。

给定一个结构类型S和一个名为T的类型，那么该结构体S的方法集为:

 1. 如果S包含匿名字段T，则`S`和`*S`的方法集会包括以T作为receiver的方法。
 2. `*S`还额外包含了以`*T`作为receiver的方法


* 外部类型和嵌入式类型实现了相同的interface怎么办?

```go
func (a *Admin) Notify() error {
    log.Printf("Admin: Sending Admin Email To %s<%s>\n",
        a.Name,
        a.Email)

    return nil
}

func main() {
    admin := &Admin{
        User: User{
            Name:  "john smith",
            Email: "john@email.com",
        },
        Level: "super",
    }
	// 外部类型所实现的方法优先覆盖嵌入式类型
    SendNotification(admin)
}

// Output
Admin: Sending Admin Email To john smith<john@email.com>
```

* interface会保存值，在调用的时候，使用保存的值来调用

```go
package main

import "fmt"

type printer interface {
	print()
}

type user struct {
	name string
}

func (u user) print() {
	fmt.Println("User Name:", u.name)
}

func main() {
	u := user{"Bill"}
	// 这里会对u进行拷贝，并保存在interface中
	entities := []printer{
		u,
		&u,
	}

	// 这里修改u，并不会影响已经拷贝的u
	u.name = "Bill_CHG"

	for _, e := range entities {
		e.print()
	}
}
```

> 当使用值接收器（值语义）实现接口时，可以在接口内部存储值和地址的副本。但是，当使用指针接收器（指针语义）实现接口时，只能存储地址的副本。

```go
package main

import "fmt"

type notifier interface {
	notify()
}

type duration int

func (d *duration) notify() {
	fmt.Println("Sending Notification in", *d)
}

func main() {
	duration(42).notify()
}
// 使用指针作为receiver的时候，是不能将值传递给interface的，必须传递的是一个可以取地址的变量。
//  duration(42)是一个常量是没有地址的，只存在于编译时
./prog.go:16:14: cannot call pointer method on duration(42)
./prog.go:16:14: cannot take the address of duration(42)
```

* interface是可以比较的，比较的是接口内部存储的数据，而不是接口本身。使用指针语义时，将比较地址。使用值语义时，将比较值。

* 深入interface

```go
// All material is licensed under the Apache License Version 2.0, January 2004
// http://www.apache.org/licenses/LICENSE-2.0

// Sample program that explores how interface assignments work when
// values are stored inside the interface.
package main

import (
	"fmt"
	"unsafe"
)

// notifier provides support for notifying events.
type notifier interface {
	notify()
}

// user represents a user in the system.
type user struct {
	name string
}

// notify implements the notifier interface.
func (u user) notify() {
	fmt.Println("Alert", u.name)
}

func inspect(n *notifier, u *user) {
	// 一个interface两个字大小，第一个字存储方法集，第二个字存储值
	word := uintptr(unsafe.Pointer(n)) + uintptr(unsafe.Sizeof(&u))
	// 可以看到interface始终存储地址，只是这个地址指向的内容到底是值拷贝后的对象，还是指针拷贝后的对象。
	value := (**user)(unsafe.Pointer(word))
	fmt.Printf("Addr User: %p  Word Value: %p  Ptr Value: %v\n", u, *value, **value)
}

func main() {

	// Create a notifier interface and concrete type value.
	var n1 notifier
	u := user{"bill"}

	// Store a copy of the user value inside the notifier
	// interface value.
	n1 = u

	// We see the interface has its own copy.
	// Addr User: 0x1040a120  Word Value: 0x10427f70  Ptr Value: {bill}
	// 通过输出结果可值，interface中存储的值和赋值过来的值不一样，因此其指向的是拷贝后的值
	inspect(&n1, &u)

	// Make a copy of the interface value.
	n2 := n1

	// We see the interface is sharing the same value stored in
	// the n1 interface value.
	// Addr User: 0x1040a120  Word Value: 0x10427f70  Ptr Value: {bill}
	// 指针赋值后，大家都是指向相同的值
	inspect(&n2, &u)

	// Store a copy of the user address value inside the
	// notifier interface value.
	n1 = &u

	// We see the interface is sharing the u variables value
	// directly. There is no copy.
	// Addr User: 0x1040a120  Word Value: 0x1040a120  Ptr Value: {bill}
	// 当传递指针的时候，interface中存储的就是地址了。
	inspect(&n1, &u)
}
// Output
Addr User: 0xc000010200  Word Value: 0xc000068f68  Ptr Value: {bill}
Addr User: 0xc000010200  Word Value: 0xc000068f68  Ptr Value: {bill}
Addr User: 0xc000010200  Word Value: 0xc000010200  Ptr Value: {bill}
```

### reflection

1. 反射是interface到反射对象的转换

```go
package main

import (
    "fmt"
    "reflect"
)

func main() {
	var x float64 = 3.4
	// TypeOf的参数是interface{},任意输入都会转换为interface{}
	// 然后通过反射获取到类型信息
	fmt.Println("type:", reflect.TypeOf(x))
	fmt.Println("value:", reflect.ValueOf(x).String())
	v := reflect.ValueOf(x)
	fmt.Println("type:", v.Type())
	fmt.Println("kind is float64:", v.Kind() == reflect.Float64)
	fmt.Println("value:", v.Float())
}

func TypeOf(i interface{}) Type
```

获取反射对象的值或者给对象设置值时，这些方法的参数或者返回值的类型是可容纳该值的最大类型上：
例如，所有有符号整数的是int64。也就是说，反射对象的Int方法返回一个int64，而SetInt值接收一个int64作为参数，例如下面这个例子:

```go
package main

import (
	"fmt"
	"reflect"
)

func main() {
	var x uint8 = 'x'
	v := reflect.ValueOf(x)
	fmt.Println("type:", v.Type())                            // uint8.
	fmt.Println("kind is uint8: ", v.Kind() == reflect.Uint8) // true.
	// 获取值的时候，Uint返回的是uint64，因此这里需要转型
	x = uint8(v.Uint())                                       // v.Uint returns a uint64.
}

```

通过反射获取到的类型是其底层的真实类型，而不是类型别名

```go
type MyInt int
var x MyInt = 7
// v.Kind == reflect.Int
v := reflect.ValueOf(x)
```

2. 反射也可以从反射对象转换为interface

```go
package main

import (
	"fmt"
	"reflect"
)

func main() {
	var x uint8 = 'x'
	v := reflect.ValueOf(x)
	// 将反射对象变成了interface，然后传递给了Println
    fmt.Println(v.Interface())
}
```

3. 要修改反射对象，该值必须可设置。

可设置是反射对象的属性，并非所有反射对象都具有它。

```go
package main

import (
	"reflect"
	"fmt"
)

func main() {
	var x float64 = 3.4
	// 这里是将x传递给了ValueOf进行拷贝才有了反射对象v，因为通过v修改值也只是对拷贝进行了修改，并不是修改x本身
	// 因此v不具有可设置值的属性
	v := reflect.ValueOf(x)
	// settability of v: false
	fmt.Println("settability of v:", v.CanSet())
	// panic: reflect.Value.SetFloat using unaddressable value
	v.SetFloat(7.1) // Error: will panic.
}
```

改成下面这样就可以设置值了

```go
	var x float64 = 3.4
	// 反射对象p本身是不可设置的，其指向的元素才是可设置的。因为p的类型是*float64
	// 指针的值是没办法修改的，修改指针的值只会导致指向另外一个对象
	// 通过Elem可以获取到其指向的值，也就是*p，*p才是可以设置的。
	p := reflect.ValueOf(&x) // Note: take the address of x.
	// type of p: *float64
	// settability of p: false
	fmt.Println("type of p:", p.Type())
	fmt.Println("settability of p:", p.CanSet())

	v := p.Elem()
	fmt.Println("settability of v:", v.CanSet())
	v.SetFloat(7.1)
	fmt.Println(v.Interface())
	fmt.Println(x)
```

此外，还可以通过反射来修改struct的值，struct中的大写开头的字段才是导出字段，是可以被修改的，其他的字段是不具备可设置属性的

```go
type T struct {
    A int
    B string
}
t := T{23, "skidoo"}
s := reflect.ValueOf(&t).Elem()
typeOfT := s.Type()
for i := 0; i < s.NumField(); i++ {
	// 获取到字段
    f := s.Field(i)
    fmt.Printf("%d: %s %s = %v\n", i,
        typeOfT.Field(i).Name, f.Type(), f.Interface())
}
```


## exporting

* package是go的基本编译单元，
* 代码被编译到package中，并最终链接在一起
* 标识符根据字母大小写导出（或保持未导出）
* 通过import导入package，就可以访问这个package中已经导出的标识符了
* 任何包都可以使用未导出类型的值，但是使用起来很烦人

```go
 
package counters

// alertCounter is an unexported type that
// contains an integer counter for alerts.
type alertCounter int

// NewAlertCounter creates and returns objects of
// the unexported type alertCounter.
func NewAlertCounter(value int) alertCounter {
	return alertCounter(value)
}

// 间接访问未导出的标识符
package main

import (
	"fmt"
	"test/counters"
)

func main() {
	// Create a variable of the unexported type using the
	// exported NewAlertCounter function from the package counters.
	counter := counters.NewAlertCounter(10)

	fmt.Printf("Counter: %d\n", counter)
}

```

* struct的字段名或者方法名如果是小写是无法被外部直接访问的
* 如果struct中嵌入的类型是未导出的，则无法直接初始化，需要显示的访问嵌入式类型中导出的字段来初始化

```go
package animals

// animal represents information about all animals.
type animal struct {
	Name string
	Age  int
}

// Dog represents information about dogs.
type Dog struct {
	animal
	BarkStrength int
}

package main

import (
	"fmt"
	"test/animals"
)

func main() {
	// Create an object of type Dog from the animals package.
	// This will NOT compile.
	dog := animals.Dog{
		// 无法编译
		animal: animals.animal{
			Name: "Chole",
			Age:  1,
		},
		BarkStrength: 10,
	}

	fmt.Printf("Counter: %#v\n", dog)

	// Create an object of type Dog from the animals package.
	dog := animals.Dog{
		BarkStrength: 10,
	}
	// 显示访问导出字段来进行初始化
	dog.Name = "Chole"
	dog.Age = 1

	fmt.Printf("Counter: %#v\n", dog)
}

```

### Composition

* 行为的组合，而不是数据的组合
* 组合超越了类型嵌入
* 考虑将行为定义成一个个独立的intreface，然后通过接口组合形成功能更大的接口

```go
// NailDriver represents behavior to drive nails into a board.
type NailDriver interface {
	DriveNail(nailSupply *int, b *Board)
}

// NailPuller represents behavior to remove nails into a board.
type NailPuller interface {
	PullNail(nailSupply *int, b *Board)
}

// NailDrivePuller represents behavior to drive and remove nails into a board.
type NailDrivePuller interface {
	NailDriver
	NailPuller
}
```

* 确保每个函数或方法对于它们接受的接口类型都是非常特定的。仅接受您在该函数或方法中使用的行为的接口类型。这将有助于确定所需的较大接口类型。
* 类型嵌入不是子类型、也不是子类。

```go
// 面向对象的这种继承的风格
type Animal struct {
	Name string
	IsMamal bool
}

func (a Animal) Speak() {}

type Dog struct {
	Animal
	PackFactor int
}
func (d Dog) Speak() {}

// 基于行为的风格
type Speaker interface {
	Speak()
}

type Dog struct {
	Name string
	IsMamal bool
	PackFactor int
}

func (d Dog) Speak() {}

```

* 是否有必要添加一个接口，checklist如下:

	1. package声明了一个接口，该接口与其具体类型的整个API相匹配。
	2. factory函数返回的类型是内部未导出的类型
	3. 可以删除该接口，并且对于API用户而言，没有任何更改。
	4. 接口未将API与更改分离

满足上面条件则没有必要声明接口，下面这个checklist则是需要使用接口的场景:
	1. API的用户需要提供实现细节的时候
	2. APi有多个实现的时候
	3. 识别出API中可以更改的部分并需要将其去耦合

* intreface conversion

```go
package main

import "fmt"

// Mover provides support for moving things.
type Mover interface {
	Move()
}

// Locker provides support for locking and unlocking things.
type Locker interface {
	Lock()
	Unlock()
}

// MoveLocker provides support for moving and locking things.
type MoveLocker interface {
	Mover
	Locker
}

// bike represents a concrete type for the example.
type bike struct{}

// Move can change the position of a bike.
func (bike) Move() {
	fmt.Println("Moving the bike")
}

// Lock prevents a bike from moving.
func (bike) Lock() {
	fmt.Println("Locking the bike")
}

// Unlock allows a bike to be moved.
func (bike) Unlock() {
	fmt.Println("Unlocking the bike")
}

func main() {


	var ml MoveLocker
	var m Mover

	// bike实现了move、lock、unlock，满足MoveLocker接口
	ml = bike{}

	// 可以隐式转换为接口的子集。
	// Move接口是MoveLocker接口的子集
	m = ml

	// 但是反过来不可以。
	ml = m
	
	// 将接口转换为具体的值
	b := m.(bike)
	ml = b
}

```

* Runtime Type Assertions

```go
// car represents something you drive.
type car struct{}

// String implements the fmt.Stringer interface.
func (car) String() string {
	return "Vroom!"
}

mvs fmt.Stringer := car{}

if v, is := mvs.(car); is {
	fmt.Printf("Type assertion success")
}
```

## Error Handing

1. 当错误的上下文比较复杂的时候，通过创建自定义的错误类型类似承载

```go
type SyntaxError struct {
    msg    string // description of error
    Offset int64  // error occurred after reading Offset bytes
}

func (e *SyntaxError) Error() string { return e.msg }
```

2. 对于一些静态的、简单的错误可以直接使用标准库中的error

```go
func Sqrt(f float64) (float64, error) {
    if f < 0 {
        return 0, errors.New("math: square root of negative number")
    }
}
```

3. 统一定义Package级别的错误(Err前缀，这是go定义错误的命名规范)


```go
var (
    ErrInvalidUnreadByte = errors.New("bufio: invalid use of UnreadByte")
    ErrInvalidUnreadRune = errors.New("bufio: invalid use of UnreadRune")
    ErrBufferFull        = errors.New("bufio: buffer full")
    ErrNegativeCount     = errors.New("bufio: negative count")
)

data, err := b.Peek(1)
if err != nil {
    switch err {
    case bufio.ErrNegativeCount:
        // Do something specific.
        return
    case bufio.ErrBufferFull:
        // Do something specific.
        return
    default:
        // Do something generic.
        return
    }
}
```

4. 小心error的比较

error是个interface，intreface的比较要看其内部存储的是值还是指针，实际比较的时候是用内部存储的类型来比较的，如果存储的指针，那么总是不相同，
如果存储的是值会进行值的比较。

```go
package main

import "errors"
import "fmt"

func main() {
  // errors.New返回的interface内部存储的是指针
  a := errors.New("same thing");
  b := errors.New("same thing");

  if a == b {
    fmt.Printf("same")
  } else {
    fmt.Printf("no")
  }
}


type ErrNumber int64

func (e ErrNumber) Error() string {
	return "error number"
}

func main() {
	// 比较的是ErrNumber的值，因为ErrNumber是值类型存储在intreface中
	var err1 error = ErrNumber(5)
	var err2 error = ErrNumber(5)

	if err1 == err2 {
		fmt.Printf("same")
	} else {

		fmt.Printf("no")
	}

}

```

> 一旦我们使用指针作为receve就标志着我们只能存储指针到interface中，因此这个时候比较interface就总是比较指针了。
> 这种情况下，可以预先在pacakge级别定义好一系列的错误。这样就可以进行比较了，因为此时的接口都是指向相同的错误变量

5. 小心error的赋值

error是个interface，一个interface通常来说内部有两个指针，一个指针指向类型，一个指针指向值，当我们将一个自定义的error类型的nil指针赋值给error的时候，
实际上其类型部分已经不是nil了，只是值的部分是nil而已。一个intreface如果是nil的话，就必须内部的所有指针都是nil。 

```go

type ErrNumber struct {
	number int64
}

func (e *ErrNumber) Error() string {
	return "error number"
}

func main() {

	var err *ErrNumber = nil
	var err2 error = err

	// 这里会输出not nil，因为err2的类型部分指向了ErrNumber，只是值的部分是nil而已。
	if err2 != nil {
		fmt.Printf("not nil")
	} else {
		fmt.Printf("nil")
	}
}

```

6. `github.com/pkg/errors`

log和error是需要一起处理的，error的地方都是需要记录日志的，记录的日志需要能够帮助我们debug问题。

7. 面向失败编程，而不是成功，因此Go没有实现异常。

8. 错误处理方式的进化之路

```go
// Get fetches and unmarshals the JSON blob for the key k into v.
// If the key is not found, Get reports a "key not found" error.
func (tx *Tx) Get(k string, v interface{}) (err error)

// 第一种方式，不推荐，错误信息不够，而且也强耦合错误类型
var ErrNotFound = errors.New("taildb: key not found")
var val Value
if err := tx.Get("my-key", &val); err == taildb.ErrNotFound {
	// no such key
} else if err != nil {
	// something went very wrong
} else {
	// use val
}

// 第二种方式，通过定义错误类型，可以承载更多的错误上下文，但是仍然存在问题
// 当有人在这个错误的基础上又添加了错误，那么在得到错误的时候就不知道到底是何种类型了
type KeyNotFoundError struct {
	Name string
}

func (e KeyNotFoundError) Error() string {
	return fmt.Errorf("taildb: key %q not found")
}

var val Value
err := tx.Get("my-key", &val)
if err != nil {
	if _, isNotFound := err.(taildb.KeyNotFoundError); isNotFound {
		// no such key
	} else {
		// something went very wrong
	}
} else {
	// use val
}

func accessCheck(tx *taildb.Tx, key string) error {
	var val Value
	if err := tx.Get(key, &val); err != nil {
		// 错误类型再次被封装了，调用accessCheck的地方就没办法拿到真实的错误了。
		return fmt.Errorf("access check: %v", err)
	}
	if !val.AccessGranted {
		return errAccessDenied
	}
	return nil
}


// 第三种方式 通过xerrors库可以保留底层的错误类型，这样我们在调用的地方就可以进行转换了。
// xerrors在1.13的时候将会成为标准库的一部分，届时通过fmt.Errorf("%w")同样可以实现相同的效果。
if err := tx.Get(key, &val); err != nil {
	return xerrors.Errorf("access check: %w", err)
}

var val Value
if err := accessCheck(tx, "my-key"); err != nil {
	var notFoundErr taildb.KeyNotFoundError
	if xerrors.As(err, &notFoundErr) {
		// no such key
	} else {
		// something went very wrong
	}
} else {
	// use val
}



// 第四种方式，对xerrors的使用做了优化
var ErrNotFound = errors.New("key not found")
Inside taildb we can write:

func (tx *Tx) Get(k string, v interface{}) (err error) {
	// ...
	if noSuchKey {
		return xerrors.Errorf("taildb: %q: %w", k, ErrNotFound)
	}
}

var val Value
if err := accessCheck(tx, "my-key"); xerrors.Is(err, taildb.ErrNotFound) {
	// no such key
} else if err != nil {
	// something went very wrong
} else {
	// use val
}
```

## Pacakgeing

```bash
Kit                     Application

├── CONTRIBUTORS        ├── cmd/
├── LICENSE             ├── internal/
├── README.md           │   └── platform/
├── cfg/                └── vendor/
├── examples/
├── log/
├── pool/
├── tcp/
├── timezone/
├── udp/
└── web/
```

* `vendor/` 所有依赖的三方库的包都需要copy到这个目录下
* `cmd` 所有的项目二进制都放在这个目录下，这个目录下对于每一个二进制程序有单独的目录。
* `internal` 需要被多个程序用到的package属于这个目录，使用名称`internal/`的一个好处是，项目从编译器中获得了额外的保护。
			 此项目外部的任何软件包都不能从`internal/`内部导入软件包。因此，这些软件包仅在此项目内部。
* `internal/platform` 基础但特定于项目的软件包位于`internal/platform/`文件夹中。这些软件包将为数据库，身份验证甚至邮件处理等提供支持。



## Goroutines And Concurrency
Goroutines是由Go调度程序创建并独立运行的函数。 Go调度程序负责goroutine的管理和执行。

* Goroutines简称为G、Goroutines运行在逻辑处理器上，简称为P，而这个逻辑的处理器和OS提供的Thread绑定在一起，这个Thread简称为M，而这个Thread则有OS负责绑定到一个处理器上运行。

* `GODEBUG=schedtrace=1000` 输出go runtime的调度trace信息，每隔1000微妙

```bash
SCHED 0ms: gomaxprocs=1 idleprocs=0 threads=2 spinningthreads=0 idlethreads=0
runqueue=0 [1]

SCHED 1009ms(程序运行到现在的时间): gomaxprocs=1(配置的逻辑处理器数量) idleprocs=0 (有多少处理器是闲置的)
threads=3(总共有三个线程运行，其中二个是服务go runtime的，另外一个才是绑定到处理器上运行) spinningthreads=0 idlethreads=1(有多少个线程闲置) runqueue=0(有多少协程在全局运行队列) [9](有多少个协程在本地运行队列)
```

```bash
SCHED 2002ms: gomaxprocs=2 idleprocs=0 threads=4 spinningthreads=0
idlethreads=1 runqueue=0 [4 4]

2002ms        : 在程序运行了2s左右输出的trace信息
gomaxprocs=2  : 配置了2个逻辑处理器
threads=4     : 有四个线程在运行，2个服务于go runtime，还有2个服务于处理器
idlethreads=1 : 有1个线程是闲置的
idleprocs=0   : 有0个处理器是处于闲置状态
runqueue=0    : 有0个协程在全局运行队列中
[4 4]         : 每一个处理器上的本地运行队列中都有4个协程在等待被运行。
```

* `GODEBUG=schedtrace=1000,scheddetail=1` 显示等详细的调度trace信息

```bash
SCHED 4028ms: gomaxprocs=2 idleprocs=0 threads=4 spinningthreads=0
idlethreads=1 runqueue=2 gcwaiting=0 nmidlelocked=0 stopwait=0 sysmonwait=0
P0: status=1 schedtick=10 syscalltick=0 m=3 runqsize=3 gfreecnt=0
P1: status=1 schedtick=10 syscalltick=1 m=2 runqsize=3 gfreecnt=0
M3: p=0 curg=4 mallocing=0 throwing=0 gcing=0 locks=0 dying=0 helpgc=0 spinning=0 blocked=0 lockedg=-1
M2: p=1(表示这个线程绑定在哪个处理器上了) curg=10 mallocing=0 throwing=0 gcing=0 locks=0 dying=0 helpgc=0 spinning=0 blocked=0 lockedg=-1
M1: p=-1 curg=-1 mallocing=0 throwing=0 gcing=0 locks=1 dying=0 helpgc=0 spinning=0 blocked=0 lockedg=-1
M0: p=-1 curg=-1 mallocing=0 throwing=0 gcing=0 locks=0 dying=0 helpgc=0 spinning=0 blocked=0 lockedg=-1
G1: status=4(semacquire) m=-1 lockedm=-1
G2: status=4(force gc (idle)) m=-1 lockedm=-1
G3: status=4(GC sweep wait) m=-1 lockedm=-1
G4: status=2(sleep) m=3 lockedm=-1
G5: status=1(sleep) m=-1 lockedm=-1
G6: status=1(stack growth) m=-1 lockedm=-1
G7: status=1(sleep) m=-1 lockedm=-1
G8: status=1(sleep) m=-1 lockedm=-1
G9: status=1(stack growth) m=-1 lockedm=-1
G10: status=2(sleep) m=2(表示这个协程此时在哪个线程中运行) lockedm=-1
G11: status=1(sleep) m=-1 lockedm=-1
G12: status=1(sleep) m=-1 lockedm=-1
G13: status=1(sleep) m=-1 lockedm=-1
G17: status=4(timer goroutine (idle)) m=-1 lockedm=-1
```

P表示处理器、M表示线程、G表示协程。

```bash
status: http://golang.org/src/runtime/
Gidle,            // 0
Grunnable,        // 1 runnable and on a run queue
Grunning,         // 2 running
Gsyscall,         // 3 performing a syscall
Gwaiting,         // 4 waiting for the runtime
Gmoribund_unused, // 5 currently unused, but hardcoded in gdb scripts
Gdead,            // 6 goroutine is dead
Genqueue,         // 7 only the Gscanenqueue is used
Gcopystack,       // 8 in this state when newstack is moving the stack
```

* `GOMAXPROCS` 控制go协程可以在多少个core上运行。

* Concurrency Pattern
	
	1. Generator

```go
    c := boring("boring!") // Function returning a channel.
    for i := 0; i < 5; i++ {
        fmt.Printf("You say: %q\n", <-c)
    }
    fmt.Println("You're boring; I'm leaving.")

	func boring(msg string) <-chan string { // Returns receive-only channel of strings.
		c := make(chan string)
		go func() { // We launch the goroutine from inside the function.
			for i := 0; ; i++ {
				c <- fmt.Sprintf("%s %d", msg, i)
				time.Sleep(time.Duration(rand.Intn(1e3)) * time.Millisecond)
			}
		}()
		return c // Return the channel to the caller.
	}
```

> 上面的代码存在协程泄漏，需要考虑加入context

	2. Fan in

```go
func merge(cs ...<-chan int) <-chan int {
    var wg sync.WaitGroup
    out := make(chan int)

    // Start an output goroutine for each input channel in cs.  output
    // copies values from c to out until c is closed, then calls wg.Done.
    output := func(c <-chan int) {
        for n := range c {
            out <- n
        }
        wg.Done()
    }
    wg.Add(len(cs))
    for _, c := range cs {
        go output(c)
    }

    // Start a goroutine to close out once all the output goroutines are
    // done.  This must start after the wg.Add call.
    go func() {
        wg.Wait()
        close(out)
    }()
    return out
}
```


	3. Fan out

```go
func fanOutSem() {
	emps := 2000
	ch := make(chan string, emps)

	g := runtime.GOMAXPROCS(0)
	sem := make(chan bool, g)

	for e := 0; e < emps; e++ {
		go func(emp int) {
			sem <- true
			{
				time.Sleep(time.Duration(rand.Intn(200)) * time.Millisecond)
				ch <- "paper"
				fmt.Println("employee : sent signal :", emp)
			}
			<-sem
		}(e)
	}

	for emps > 0 {
		p := <-ch
		emps--
		fmt.Println(p)
		fmt.Println("manager : recv'd signal :", emps)
	}

	time.Sleep(time.Second)
	fmt.Println("-------------------------------------------------------------")
}

```

	4. Drop

```go
func drop() {
	const cap = 100
	ch := make(chan string, cap)

	go func() {
		for p := range ch {
			fmt.Println("employee : recv'd signal :", p)
		}
	}()

	const work = 2000
	for w := 0; w < work; w++ {
		select {
		case ch <- "paper":
			fmt.Println("manager : sent signal :", w)
		default:
			fmt.Println("manager : dropped data :", w)
		}
	}

	close(ch)
	fmt.Println("manager : sent shutdown signal")

	time.Sleep(time.Second)
	fmt.Println("-------------------------------------------------------------")
}
```

	5. pooling

```go
func pooling() {
	ch := make(chan string)

	g := runtime.GOMAXPROCS(0)
	for e := 0; e < g; e++ {
		go func(emp int) {
			for p := range ch {
				fmt.Printf("employee %d : recv'd signal : %s\n", emp, p)
			}
			fmt.Printf("employee %d : recv'd shutdown signal\n", emp)
		}(e)
	}

	const work = 100
	for w := 0; w < work; w++ {
		ch <- "paper"
		fmt.Println("manager : sent signal :", w)
	}

	close(ch)
	fmt.Println("manager : sent shutdown signal")

	time.Sleep(time.Second)
	fmt.Println("-------------------------------------------------------------")
}
```

	5. Pipeline

```go
func gen(nums ...int) <-chan int {
    out := make(chan int)
    go func() {
        for _, n := range nums {
            out <- n
        }
        close(out)
    }()
    return out
}

func sq(in <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        for n := range in {
            out <- n * n
        }
        close(out)
    }()
    return out
}

func main() {
    // Set up the pipeline and consume the output.
    for n := range sq(sq(gen(2, 3))) {
        fmt.Println(n) // 16 then 81
    }
}

```

## Data Race

* `go build -race/go test -race` 开启race检测
* map默认自带race检测
* 对于接口的read/write是存在data race的，因为interface是个双字大小的类型，赋值的时候不是原子的，需要修改指向的类型，还需要修改指向的值。


## Channels

channels允许goroutines通过信号语义相互通信，Channels通过使用发送/接收数据或通过识别各个Channels上的状态变化来完成此信号。
不要以Channels是队列的思想来设计软件，而要专注于信号语义来简化同步。

* 使用channels编排和协调goroutine
	1. 关注channels提供的信令语义，而不是数据共享。
	2. 信号分为有数据和无数据的。
	3. 对于使用channels来作为数据共享的场景需要质疑

* Unbuffered channels
	1. 接收发生在发送之前
	2. 100%保证信号到达
	3. 对于何时收到信号是未知的，因为你发送的时候，接收端可能还没有在接收

* Buffered channels
	1. 发送发生在接收前
	2. 减少了信号之间的阻塞延迟，多次信号发送之间是没有延迟的
	3. 不保证信号被接收了，可能一直在队列中
		* 缓冲区越大，保证越少
		* 缓冲区为1的话，可以保证只有一个信号被延迟发送。

* Closing channels
	1. Close发生在接收前
	2. 是一种没有数据的信号
	3. 用于取消或者是deadline是最佳的

* NIL channels
	1. 发送和接收是阻塞的
	2. 关闭了信号
	3. 非常适合速率限制或短期停工

* 往Closed的channel发送信号会导致panic，但是接收是可以的，会立即返回。

* close channel或者是对struct{}类型的channel进行send属于无数据的信号，这类信号通常用于stop、cannecel等场景。也可以使用Context

Channels内部是一个hchan结构，这个结构大致如下:

```go
type hchan struct {
	buf  CircularQueue
	sendx uint64
	recvx uint64
	lock mutex
	sendq sudog
	recvq sudog
}

type sudog struct {
	G Coroutine
	elem T
	...
}
```

一个circular queue，chan的读和写实际就是操作sendx、recvx、chan本身其实就是一个指向hchan的指针。当chan是buffer的channel的时候写入和读取就是简单的加锁然后移动sendx、recvx
当chan为unbuffer或者buffer满的时候发生写入或者buffer空的时候发生读取的时候都会导致阻塞，这个时候go runtime会调用gopark把当前协程的状态设置为waitting，然后从当前协程中移除
放入全局队列中。然后这个协程所对应的OS Thread会继续从可运行队列中运行下一个协程。然后把当前协程和要写入的值放入一个类为sudog的send queue中。当有receive从协程接收的时候会
从send queue中出队，把值放到circular queue中或者如果是一个unbuffered的chan则直接赋值给receiver，最后调用go runtime中的goready将当前协程设置为runable。等待调度到OS Thread中
被运行。同样当receive出现阻塞的时候，过程和send类似。

## Context

* Context提供了key-value的映射，key和value都是`interface{}`类型，key必须具备相等性比较，而value则允许被多个协程安全的使用。 
* 到server的请求应该创建一个Context
* 从Server中发出的请求应该可以接收Context参数
* 进来的请求和出去的请求之间必须能否传递Context
* 取消Context后，从该Context派生的所有Context也会被取消
* 不要将上下文存储在结构类型中；而是将上下文明确传递给需要它的每个函数
* 即使函数允许，也不要传递nil Context。如果不确定使用哪个上下文，请传递context.TODO。
* 仅将上下文值用于请求范围的进程和API间的传递，而不用于将可选参数传递给函数。
* 可以将相同的上下文传递给在不同goroutine中运行的函数。上下文对于由多个goroutine同时使用是安全的。

## Testing

* 使用httptest来做mockClient

```go
func init() {
	handlers.Routes()
}

// TestSendJSON testing the sendjson internal endpoint.
func TestSendJSON(t *testing.T) {
	url := "/sendjson"
	statusCode := 200

	t.Log("Given the need to test the SendJSON endpoint.")
	{
		r := httptest.NewRequest("GET", url, nil)
		w := httptest.NewRecorder()
		http.DefaultServeMux.ServeHTTP(w, r)

		testID := 0
		t.Logf("\tTest %d:\tWhen checking %q for status code %d", testID, url, statusCode)
		{
			if w.Code != 200 {
				t.Fatalf("\t%s\tTest %d:\tShould receive a status code of %d for the response. Received[%d].", failed, testID, statusCode, w.Code)
			}
			t.Logf("\t%s\tTest %d:\tShould receive a status code of %d for the response.", succeed, testID, statusCode)

			var u struct {
				Name  string
				Email string
			}

			if err := json.NewDecoder(w.Body).Decode(&u); err != nil {
				t.Fatalf("\t%s\tTest %d:\tShould be able to decode the response.", failed, testID)
			}
			t.Logf("\t%s\tTest %d:\tShould be able to decode the response.", succeed, testID)

			if u.Name == "Bill" {
				t.Logf("\t%s\tTest %d:\tShould have \"Bill\" for Name in the response.", succeed, testID)
			} else {
				t.Errorf("\t%s\tTest %d:\tShould have \"Bill\" for Name in the response : %q", failed, testID, u.Name)
			}

			if u.Email == "bill@ardanlabs.com" {
				t.Logf("\t%s\tTest %d:\tShould have \"bill@ardanlabs.com\" for Email in the response.", succeed, testID)
			} else {
				t.Errorf("\t%s\tTest %d:\tShould have \"bill@ardanlabs.com\" for Email in the response : %q", failed, testID, u.Email)
			}
		}
	}
}

```


* 使用httptest来做mockServer

```go
// mockServer returns a pointer to a server to handle the mock get call.
func mockServer() *httptest.Server {
	f := func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(200)
		w.Header().Set("Content-Type", "application/xml")
		fmt.Fprintln(w, feed)
	}

	return httptest.NewServer(http.HandlerFunc(f))
}

// TestDownload validates the http Get function can download content and
// the content can be unmarshaled and clean.
func TestDownload(t *testing.T) {
	statusCode := http.StatusOK

	server := mockServer()
	defer server.Close()

	t.Log("Given the need to test downloading content.")
	{
		testID := 0
		t.Logf("\tTest %d:\tWhen checking %q for status code %d", testID, server.URL, statusCode)
		{
			resp, err := http.Get(server.URL)
			if err != nil {
				t.Fatalf("\t%s\tTest %d:\tShould be able to make the Get call : %v", failed, testID, err)
			}
			t.Logf("\t%s\tTest %d:\tShould be able to make the Get call.", succeed, testID)

			defer resp.Body.Close()

			if resp.StatusCode != statusCode {
				t.Fatalf("\t%s\tTest %d:\tShould receive a %d status code : %v", failed, testID, statusCode, resp.StatusCode)
			}
			t.Logf("\t%s\tTest %d:\tShould receive a %d status code.", succeed, testID, statusCode)

			var d Document
			if err := xml.NewDecoder(resp.Body).Decode(&d); err != nil {
				t.Fatalf("\t%s\tTest %d:\tShould be able to unmarshal the response : %v", failed, testID, err)
			}
			t.Logf("\t%s\tTest %d:\tShould be able to unmarshal the response.", succeed, testID)

			if len(d.Channel.Items) == 1 {
				t.Logf("\t%s\tTest %d:\tShould have 1 item in the feed.", succeed, testID)
			} else {
				t.Errorf("\t%s\tTest %d:\tShould have 1 item in the feed : %d", failed, testID, len(d.Channel.Items))
			}
		}
	}
}

```

## Compile args

```go
panic: Aw, snap
 goroutine 1 [running]:
 main.main()
         /home/johnpili/go/src/company.com/event-document-pusher/main.go:42 +0x3e

> go build -trimpath

panic: Aw, snap
 goroutine 1 [running]:
 main.main()
         src/company.com/event-document-pusher/main.go:42 +0x3e
```

## Reference

* [Contiguous stacks](https://docs.google.com/document/d/1wAaf1rYoM4S4gtnPh0zOlGzWtrZFQ5suE8qr2sD8uWQ/pub)
* [How Stacks are Handled in Go](https://blog.cloudflare.com/how-stacks-are-handled-in-go/)
* [gc](https://github.com/qcrao/Go-Questions/blob/master/GC/GC.md)
* [Go Escape Analysis Flaws](https://docs.google.com/document/d/1CxgUBPlx9iJzkz9JWkb6tIpTe5q32QDmz8l0BouG0Cw/edit)

## Reading TODO
* https://brunocalza.me/how-buffer-pool-works-an-implementation-in-go/
* https://www.youtube.com/watch?v=f6kdp27TYZs
* https://talks.golang.org/2013/advconc.slide#1
