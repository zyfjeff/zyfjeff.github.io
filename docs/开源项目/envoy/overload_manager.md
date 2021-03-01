# Overload manager分析

**核心概念:**

1. ResourceMonitors 内置了一系列要监控的资源，目前有的是FixedHeap

2. OverloadAction 触发资源的限制的后，可以进行的一系列action，提供了isActive来表明当前Action是否有效(有Trigger就是有效的)

    * envoy.overload_actions.stop_accepting_requests
    * envoy.overload_actions.disable_http_keepalive
    * envoy.overload_actions.stop_accepting_connections
    * envoy.overload_actions.shrink_heap

3. Trigger 每一个OverloadAction会包含一个触发器，里面设置了阀值，提供了isFired来表明当前触发器是否触发了

4. Resource 每一个资源Monitor和一个Resource关联起来

5. 相关的stats

    * failed_updates 资源更新失败的次数
    * skipped_updates 已经有资源在更新了，新的资源更新就跳过

**Setp1: 实现核心接口**

```cpp
class ResourceMonitor {
public:
  virtual ~ResourceMonitor() {}
  class Callbacks {
  public:
    virtual ~Callbacks() {}
    virtual void onSuccess(const ResourceUsage& usage) = 0;
    // 资源获取失败的时候，没办法更新资源就回调onFailure
    virtual void onFailure(const EnvoyException& error) = 0;
  };
  virtual void updateResourceUsage(Callbacks& callbacks) = 0;
```

每一类资源需要去实现这个接口，例如:FixedHeap

```cpp
class FixedHeapMonitor : public Server::ResourceMonitor {
public:
  FixedHeapMonitor(
      const envoy::config::resource_monitor::fixed_heap::v2alpha::FixedHeapConfig& config,
      std::unique_ptr<MemoryStatsReader> stats = std::make_unique<MemoryStatsReader>());

  void updateResourceUsage(Server::ResourceMonitor::Callbacks& callbacks) override;

private:
  const uint64_t max_heap_;
  std::unique_ptr<MemoryStatsReader> stats_;
};
```

**Setp2: 定期回调updateResourceUsage**

OverloadManagerImpl初始化的时候，会对各种资源的Monitor进行构造、以及OverloadAction等
然后开始启动timer，定期回调updateResourceUsage接口，然后更新thread_local状态。

```cpp

void OverloadManagerImpl::start() {
  ASSERT(!started_);
  started_ = true;

  tls_->set([](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
    return std::make_shared<ThreadLocalOverloadState>();
  });

  if (resources_.empty()) {
    return;
  }

  timer_ = dispatcher_.createTimer([this]() -> void {
    for (auto& resource : resources_) {
      resource.second.update();
    }

    timer_->enableTimer(refresh_interval_);
  });
  timer_->enableTimer(refresh_interval_);
}
```