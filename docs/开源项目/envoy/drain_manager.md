# Drain manager分析


1. 基础接口类

    ```cpp
    // 网络层用来决策什么时候执行drain
    class DrainDecision {
    public:
        virtual ~DrainDecision() {}
        // 用于判断一个连接是否要closed并且进行drain处理
        virtual bool drainClose() const PURE;
    };

    class DrainManager : public Network::DrainDecision {
    public:
        virtual void startDrainSequence(std::function<void()> completion) PURE;
        virtual void startParentShutdownSequence() PURE;
    };
    ```

2. DrainManagerImpl 实现

```cpp
class DrainManagerImpl : Logger::Loggable<Logger::Id::main>, public DrainManager {
public:
  DrainManagerImpl(Instance& server, envoy::api::v2::Listener::DrainType drain_type);

  // Server::DrainManager
  bool drainClose() const override;
  void startDrainSequence(std::function<void()> completion) override;
  void startParentShutdownSequence() override;

private:
  bool draining() const { return drain_tick_timer_ != nullptr; }
  void drainSequenceTick();

  Instance& server_;
  const envoy::api::v2::Listener::DrainType drain_type_;
  Event::TimerPtr drain_tick_timer_;
  std::atomic<uint32_t> drain_time_completed_{};
  Event::TimerPtr parent_shutdown_timer_;
  std::function<void()> drain_sequence_completion_;
};

// 就是一个timer，指定的时间到达后就开始执行drain callback
void DrainManagerImpl::startDrainSequence(std::function<void()> completion) {
  drain_sequence_completion_ = completion;
  ASSERT(!drain_tick_timer_);
  drain_tick_timer_ = server_.dispatcher().createTimer([this]() -> void { drainSequenceTick(); });
  drainSequenceTick();
}

// 不停的计数和打印
void DrainManagerImpl::drainSequenceTick() {
  ENVOY_LOG(trace, "drain tick #{}", drain_time_completed_.load());
  ASSERT(drain_time_completed_.load() < server_.options().drainTime().count());
  ++drain_time_completed_;

  if (drain_time_completed_.load() < server_.options().drainTime().count()) {
    drain_tick_timer_->enableTimer(std::chrono::milliseconds(1000));
  } else if (drain_sequence_completion_) {
    drain_sequence_completion_();
  }
}

// 在hotrestart的时候，子进程通知父进程closing listener然后开始执行drain
void InstanceImpl::drainListeners() {
  ENVOY_LOG(info, "closing and draining listeners");
  listener_manager_->stopListeners();
  drain_manager_->startDrainSequence(nullptr);
}
```

ListenerImpl实现了Network::DrainDecision接口

```cpp
bool ListenerImpl::drainClose() const {
  // When a listener is draining, the "drain close" decision is the union of the per-listener drain
  // manager and the server wide drain manager. This allows individual listeners to be drained and
  // removed independently of a server-wide drain event (e.g., /healthcheck/fail or hot restart).
  return local_drain_manager_->drainClose() || parent_.server_.drainManager().drainClose();
}
```


http的conn manager每次在处理请求的时候会根据listenerImpl的drianClosed的结果来决定是否进行drain处理

```cpp
  // See if we want to drain/close the connection. Send the go away frame prior to encoding the
  // header block.
  if (connection_manager_.drain_state_ == DrainState::NotDraining &&
      connection_manager_.drain_close_.drainClose()) {

    // This doesn't really do anything for HTTP/1.1 other then give the connection another boost
    // of time to race with incoming requests. It mainly just keeps the logic the same between
    // HTTP/1.1 and HTTP/2.
    connection_manager_.startDrainSequence();
    connection_manager_.stats_.named_.downstream_cx_drain_close_.inc();
    ENVOY_STREAM_LOG(debug, "drain closing connection", *this);
  }
```

如果要进行drain处理就开始执行Drain

```cpp
void ConnectionManagerImpl::startDrainSequence() {
  ASSERT(drain_state_ == DrainState::NotDraining);
  drain_state_ = DrainState::Draining;
  // http1的话什么都不做，http2则会发送GOAWAY进行优雅关闭
  codec_->shutdownNotice();
  drain_timer_ = read_callbacks_->connection().dispatcher().createTimer(
      [this]() -> void { onDrainTimeout(); });
  drain_timer_->enableTimer(config_.drainTimeout());
}

void ConnectionManagerImpl::onDrainTimeout() {
  ASSERT(drain_state_ != DrainState::NotDraining);
  codec_->goAway();
  drain_state_ = DrainState::Closing;
  checkForDeferredClose();
}
```