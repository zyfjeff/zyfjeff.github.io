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

> “If most computer people lack understanding and knowledge, then what they will select will also be lacking.” - Alan Kay
> "The software business is one of the few places we teach people to write before we teach them to read." - Tom Love (inventor of Objective C)
> "Code is read many more times than it is written." - Dave Cheney
> "Programming is, among other things, a kind of writing. One way to learn writing is to write, but in all other forms of writing, one also reads. 
We read examples both good and bad to facilitate learning. But how many programmers learn to write programs by reading programs?" - Gerald M. Weinberg
> "Skill develops when we produce, not consume." - Katrina Owen


5. Productivity vs Performance

遵循Go的idioms和guideliness，我们将会同时拥有性能和开发效率，在过于这是需要二选一的，而大家更多是选择了开发效率，并希望通过硬件的发展来提高性能。这也导致了为此设计的语言产生了大量低效的软件。

6. Correctness vs Performance

您想要编写针对正确性进行了优化的代码。不要根据您认为可能会更好的性能来做出编码决策。您必须进行基准测试以了解代码是否不够快以此来决定是否需要优化。

> "Make it correct, make it clear, make it concise, make it fast. In that order." - Wes Dyer
> "Good engineering is less about finding the "perfect" solution and more about understanding the tradeoffs and being able to explain them." - JBD
> "The correctness of the implementation is the most important concern, but there is no royal road to correctness. It involves diverse tasks such as thinking of invariants, testing and code reviews. Optimization should be done, but not prematurely." - Al Aho (inventor of AWK)
> "The basic ideas of good style, which are fundamental to write clearly and simply, are just as important now as they were 35 years ago. Simple, straightforward code is just plain easier to work with and less likely to have problems. As programs get bigger and more complicated, it's even more important to have clean, simple code." - Brian Kernighan
> "Problems can usually be solved with simple, mundane solutions. That means there's no glamorous work. You don't get to show off your amazing skills. You just build something that gets the job done and then move on. This approach may not earn you oohs and aahs, but it lets you get on with it." - Jason Fried

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



## Reference

* [Contiguous stacks](https://docs.google.com/document/d/1wAaf1rYoM4S4gtnPh0zOlGzWtrZFQ5suE8qr2sD8uWQ/pub)
* [How Stacks are Handled in Go](https://blog.cloudflare.com/how-stacks-are-handled-in-go/)