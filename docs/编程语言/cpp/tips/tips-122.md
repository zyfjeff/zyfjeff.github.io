## Tip of the Week #122: Test Fixtures, Clarity, and Dataflow

> Originally published as totw/122 on 2016-08-30
> By Titus Winters (titus@google.com)
> Updated 2017-10-20
> Quicklink: abseil.io/tips/122
> Be obscure clearly. — E.B. White

测试代码与生产代码有何不同？一方面，测试未经测试：当您编写分散在多个文件中并具有数百行`SetUp`的混乱的意大利面条代码时，谁能确保测试真正在测试所需的内容呢？
您的代码审阅者常常不得不假设`Setup`是有意义的，并且最多只能对每个单独的测试用例进行逻辑检查。在这种情况下，如果发生某些变化，您的测试很可能会失败，但是很少知道是否正确。

另一方面，如果您使每个测试都尽可能简单明了，则通过检查，理解其逻辑并检查更高质量的测试逻辑将更容易发现它是正确的。让我们看一些实现该目标的简单方法。

### Dataflow in Fixtures

考虑下面的例子:

```cpp
class FrobberTest : public ::testing::Test {
 protected:
  void ConfigureExampleA() {
    example_ = "Example A";
    frobber_.Init(example_);
    expected_ = "Result A";
  }

  void ConfigureExampleB() {
    example_ = "Example B";
    frobber_.Init(example_);
    expected_ = "Result B";
  }

  Frobber frobber_;
  string example_;
  string expected_;
};

TEST_F(FrobberTest, CalculatesA) {
  ConfigureExampleA();
  string result = frobber_.Calculate();
  EXPECT_EQ(result, expected_);
}

TEST_F(FrobberTest, CalculatesB) {
  ConfigureExampleB();
  string result = frobber_.Calculate();
  EXPECT_EQ(result, expected_);
}
```

在这个相当简单的示例中，我们的测试跨越了30行代码，我们很容易想象10倍于这个值的不那么简单的例子:任何一个屏幕都无法容纳这么多。
想要验证代码正确的读者或代码审阅者必须进行如下扫描：

* OK这是`FrobberTest`，在这里定义了……哦，这个文件，好了。
* `ConfigureExampleA`…这是`FrobberTest`方法。它对某些成员变量起作用。这些是什么类型的？他们如何初始化？ OK，`Frobber`和两个字符串。有`Setup`吗？好，默认构造。
* 回到测试：好的，我们计算出一个结果，并将其与期望的结果进行比较…………我们又在其中存储了什么？”

与以更简单的样式编写的等效代码进行比较：

```cpp
TEST(FrobberTest, CalculatesA) {
  Frobber frobber;
  frobber.Init("Example A");
  EXPECT_EQ(frobber.Calculate(), "Result A");
}

TEST(FrobberTest, CalculatesB) {
  Frobber frobber;
  frobber.Init("Example B");
  EXPECT_EQ(frobber.Calculate(), "Result B");
}
```

采用这种风格，即使在一个拥有数百个测试的世界中，我们也能通过本地信息准确地知道发生了什么。

### Prefer Free Functions

在前面的示例中，所有变量初始化都很简洁，在实际测试中，情况并非总是如此。但是，关于数据流和避免固定装置的相同想法可能适用。考虑以下protobuf示例：

```cpp
class BobberTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bobber1_ = PARSE_TEXT_PROTO(R"(
        id: 17
        artist: "Beyonce"
        when: "2012-10-10 12:39:54 -04:00"
        price_usd: 200)");
    bobber2_ = PARSE_TEXT_PROTO(R"(
        id: 21
        artist: "The Shouting Matches"
        when: "2016-08-24 20:30:21 -04:00"
        price_usd: 60)");
  }

  BobberProto bobber1_;
  BobberProto bobber2_;
};

TEST_F(BobberTest, UsesProtos) {
  Bobber bobber({bobber1_, bobber2_});
  SomeCall();
  EXPECT_THAT(bobber.MostRecent(), EqualsProto(bobber2_));
}
```

同样，集中式重构会导致很多间接操作：声明和初始化是分开的，并且可能与实际使用相去甚远，此外，由于`SomeCall()`在中间，而且我们使用的是`fixture`和`fixture`成员变量，
如果不检查`SomeCall()`的细节，就无法确保在初始化和`EXPECT_THAT`验证之间`bobber1_`和`bobber2_`没有被修改。可能需要查看更多的代码。

考虑如下代码代替:

```cpp
BobberProto RecentCheapConcert() {
  return PARSE_TEXT_PROTO(R"(
      id: 21
      artist: "The Shouting Matches"
      when: "2016-08-24 20:30:21 -04:00"
      price_usd: 60)");
}
BobberProto PastExpensiveConcert() {
  return PARSE_TEXT_PROTO(R"(
      id: 17
      artist: "Beyonce"
      when: "2012-10-10 12:39:54 -04:00"
      price_usd: 200)");
}

TEST(BobberTest, UsesProtos) {
  Bobber bobber({PastExpensiveConcert(), RecentCheapConcert()});
  SomeCall();
  EXPECT_THAT(bobber.MostRecent(), EqualsProto(RecentCheapConcert()));
}
```

将初始化移到普通函数中可以清楚地表明没有隐藏的数据流。精心选择的helper名称意味着您可能甚至无需向上查看代码即可确认测试的正确性，甚至无需查看helper方法的具体实现细节。

### Five Easy Steps

通常，您可以按照以下步骤提高测试的可读性:

* 尽量避免使用fixtures，有时候不是。
* 如果你在使用fixtures，那么请避免使用fixture的成员变量，以类似于全局变量的方式开始对它们进行操作太容易了：数据流很难跟踪，因为夹杂中的任何代码路径都可能会修改成员。
* 如果您需要对变量进行复杂的初始化，这回使的每个测试难以阅读，请考虑一个辅助函数（不是fixture的一部分），该函数实现了具体的初始化并直接返回对象。
* 如果必须使用包含成员变量的fixtures，请尝试避免受用直接对这些成员进行操作的方法：尽可能将它们作为参数传递，以使数据流清晰。
* 尝试在编写测试名称之前先编写测试：如果您从令人愉快的用法入手，那么您的API通常会更好，并且您的测试几乎总是更加清晰。