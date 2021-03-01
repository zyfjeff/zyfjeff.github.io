---
hide:
  - toc        # Hide table of contents
---
# Tip of the Week #45: Avoid Flags, Especially in Library Code

> Originally posted as TotW #45 on June 3, 2013
> *by Titus Winters

> “What I really want is the behavior of my code to be controlled by a global variable that cannot be statically predicted,
> whose usage is incompletely logged, and which can only be removed from my code with great difficulty.” – Nobody, Ever

生产代码中标志的常见用法，特别是在库中，是一个错误。除非真的有必要，否则不要在那里使用`flag，我们说了。

标志是全局变量，只会更糟：你不知道这个变量在代码中在哪里被访问，一个Flag可能不仅仅在启动的时候被设置，而且可以通过任意方式稍后修改。
如果你在你的二进制中运行了一个server，通常无法保证您的flag的值在每次周期都保持不变，也不保证在值改变时会有任何通知，也没有任何发现
这种变化的方法。

如果你可以在生产环境下直接记录你的每一个二进制的直接调用，将其存储在日志中，这是OK的，但是大多数环境是无法这样工作的。对于库代码来说，这种
不确定性特别危险。你怎么知道某个特定的功能的使用何时真的结束? 简单的答案是: 你做不到。

此外Flags也使得处理dead code变的更加有挑战。在迁移到一个新的backends的时候，你认为删除遗留代码只是删除不必要的构建依赖关系，并发布历史记录中最令人满意的`git rm`。
你错了，如果你的遗留二进制文件有数百个由生产代码定义和引用的Flag，那么简单地删除死代码将给您的发布工程师带来巨大的问题：在这样的更改之后几乎没有程序会启动。

这是所有这一切中最糟糕的部分？ 2012年初在谷歌进行的一项分析发现，大多数`C++` flag，从未实际变化。

但是，Flag在某些场合是合适的：比如用flags来控制backstrace的打印，Feature Flag，更广泛地说，用于将名称/值输入传递给二进制文件并仅在`main()`中使用的标志比位置参数更易于维护。
即使考虑到这些警告，现在我们都应该好好审视下我们对Flag的使用。下次你想在你的库中添加一个Flag的时候，花点时间寻找一个更好的方法。例如可以显示的传递配置
这几乎总是更容易、合理的推理，但是也更容易维护。又或者是将数字Flag转换为编译时常量。如果在代码评审中遇到新的Flag，请抵制。每一个Flag的引入都应该是合理的。

