# Linux C++ 服务端编程
## shared_ptr实现Copy-On-Writer

基本原理:

1. writer端，如果发现引用计数为1，这时可以安全的修改共享对象，而不必担心有人正在读它
2. 对于read端在读之前把引用计数加1，读完之后减1，这样就保证在读的期间其引用计数是大于1的，可以阻止并发写

Example:

```cpp
typedef std::vector<Foo> FooList;
typedef boost::shared_ptr<FooList> FoolListPtr;
MutexLock mutex;
FoolListPtr g_fools;


// Read
void traverse() {
  FooListPtr foos;
  {
    MutexLockGuard lock(mutex);
    foos = g_foos;
    assert(!g_fools.unique());
  }

  {
    // .....
    for(auto& elem : foos) {
      elem.doit();
    }
  }
}

// Writer端

void post(const Foo& f) {
  MutexLockGuard lock(mutex);
  // 此时有人正在读，所以需要copy一下
  if (!g_foos.unique()) {
    g_foos.reset(new FoolList(*g_foos));
  }

  // 此时肯定没有人在读了，整个写期间都是有锁的，所以可以确认这期间不会有读
  assert(g_foos.unique());
  g_foos->push_back(f);
}
```


* EventLoop的一个缺点就是，它是非抢占式的，会出现优先级反转的情况。可以通过多线程来免事件处理耗时导致event loop无法及时响应后续的事件。
