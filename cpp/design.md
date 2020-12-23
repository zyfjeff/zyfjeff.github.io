## RAII List

```cpp
// RAII helper class to add an element to an std::list on construction and erase
// it on destruction, unless the cancel method has been called.
template <class T> class RaiiListElement {
public:
  RaiiListElement(std::list<T>& container, T element) : container_(container), cancelled_(false) {
    it_ = container.emplace(container.begin(), element);
  }
  virtual ~RaiiListElement() {
    if (!cancelled_) {
      erase();
    }
  }

  // Cancel deletion of the element on destruction. This should be called if the iterator has
  // been invalidated, eg. if the list has been cleared or the element removed some other way.
  void cancel() { cancelled_ = true; }

  // Delete the element now, instead of at destruction.
  void erase() {
    ASSERT(!cancelled_);
    container_.erase(it_);
    cancelled_ = true;
  }

private:
  std::list<T>& container_;
  typename std::list<T>::iterator it_;
  bool cancelled_;
};
```

## Mock技巧

1. Mock `ClassA::Create()`


``` cpp

class AInterface;

class A : public A::AInterface {
 public:
  static std::unique_ptr<A::AInterface> Create();
};

class B {
 public:
  void Method() {
    a_ = A::Create();
  }
 private:
  std::unique_ptr<A::AInterface> a_;
}
```

上面的类A通过静态方法创建实例，现在想要测试B，需要把A Mock掉。可以通过下面这种方式来Mock

```cpp
class B {
 public:
  void Method() {
    a_ = Create();
  }
  virtual std::unique_ptr<A::AInterface> Create();
 private:
  std::unique_ptr<A::AInterface> a_;
};

class ProdB : public B {
 public:
  virtual std::unique_ptr<A::AInterface> Create() override {
    return A::Create();
  }
};
```