---
hide:
  - toc        # Hide table of contents
  - navigation
---

# Cluster Manager初始化

## 核心接口介绍

* `ClusterManagerFactory`

    通过这个接口可以从Proto文件中创建`ClusterManager`，创建通用的HTTP、 TCP等连接池、创建Cluster、CDS。`ProdClusterManagerFactory`实现了这个接口。
    通过这个Factory就可以创建整个`ClusterManager`所需要的一些必须的组件。

* `ClusterManager`

    管理整个Cluster，通过这个接口来新增、更新、删除集群，也可以获取到所有的`Clusters`，但是需要注意的时候，这个接口中的一些方法只能在主线程操作并非是线程安全的，
    只有`getThreadLocalCluster`方法是线程安全的，我们可以在worker线程中安全的通过这个接口获取`ThreadLocalCluster`结构。

* `ThreadLocalCluster`
    
    这个接口是真正暴露给worker线程的，这个表示的是一个`Cluster`，通过这个接口可以实现创建连接池、获取`PrioritySet`、`ClusterInfo`等信息。`ClusterEntry`实现了这个接口。

* `Cluster`

    这个接口表示的就是一个真正的Cluster对象，包含了`Cluster`对象本身以及相关的`HealthChecker`、`Outlier::Detector`、`PrioritySet`。
    通过这个接口可以来初始化这个集群。初始化的过程其实就是根据对应集群的类型获取这个集群下的机器节点填充到`PrioritySet`中。

* `LoadBalancer`

    这个是Load Balancer的接口，用于从集群中根据配置的策略选择主机，

## 初始化过程

1. 启动的时候，载入bootstrap配置，创建`ProdClusterManagerFactory`

    ```C++ hl_lines="5-9 15"
    void InstanceImpl::initialize(const Options& options,
                                  Network::Address::InstanceConstSharedPtr local_address,
                                  ComponentFactory& component_factory, ListenerHooks& hooks) {
      .......
      cluster_manager_factory_ = std::make_unique<Upstream::ProdClusterManagerFactory>(
          *admin_, Runtime::LoaderSingleton::get(), stats_store_, thread_local_, dns_resolver_,
          *ssl_context_manager_, *dispatcher_, *local_info_, *secret_manager_,
          messageValidationContext(), *api_, http_context_, grpc_context_, router_context_,
          access_log_manager_, *singleton_manager_);

      // Now the configuration gets parsed. The configuration may start setting
      // thread local data per above. See MainImpl::initialize() for why ConfigImpl
      // is constructed as part of the InstanceImpl and then populated once
      // cluster_manager_factory_ is available.
      config_.initialize(bootstrap_, *this, *cluster_manager_factory_);
      .....
    }
    ```

    上面代码中的第15行会通过`ProdClusterManagerFactory`开始创建`ClusterManager`

2. 通过`ProdClusterManagerFactory`的`clusterManagerFromProto`方法创建`ClusterManager`


    ```C++ hl_lines="3-5 13"
    ClusterManagerPtr ProdClusterManagerFactory::clusterManagerFromProto(
        const envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      return ClusterManagerPtr{new ClusterManagerImpl(
          bootstrap, *this, stats_, tls_, runtime_, local_info_, log_manager_, main_thread_dispatcher_,
          admin_, validation_context_, api_, http_context_, grpc_context_, router_context_)};
    }

    void MainImpl::initialize(const envoy::config::bootstrap::v3::Bootstrap& bootstrap,
                              Instance& server,
                              Upstream::ClusterManagerFactory& cluster_manager_factory) {
      ....
      ENVOY_LOG(info, "loading {} cluster(s)", bootstrap.static_resources().clusters().size());
      cluster_manager_ = cluster_manager_factory.clusterManagerFromProto(bootstrap);
      ....
    }
    ```

3. `ClusterManagerImpl`构造函数内部开始创建cds、载入primary集群、static集群等操作。


    cluster的load分为两个阶段，第一阶段会载入所有的primary集群，第二阶段才会载入secondary集群。`is_primary_cluster`用来判断一个集群是否是`primary`集群。
    目前除了EDS类型的集群并且不是基于文件发现的方式，其他的都是`primary`集群。

    ```C++
      // Cluster loading happens in two phases: first all the primary clusters are loaded, and then all
      // the secondary clusters are loaded. As it currently stands all non-EDS clusters and EDS which
      // load endpoint definition from file are primary and
      // (REST,GRPC,DELTA_GRPC) EDS clusters are secondary. This two phase
      // loading is done because in v2 configuration each EDS cluster individually sets up a
      // subscription. When this subscription is an API source the cluster will depend on a non-EDS
      // cluster, so the non-EDS clusters must be loaded first.
      auto is_primary_cluster = [](const envoy::config::cluster::v3::Cluster& cluster) -> bool {
        return cluster.type() != envoy::config::cluster::v3::Cluster::EDS ||
              (cluster.type() == envoy::config::cluster::v3::Cluster::EDS &&
                cluster.eds_cluster_config().eds_config().config_source_specifier_case() ==
                    envoy::config::core::v3::ConfigSource::ConfigSourceSpecifierCase::kPath);
      };
    ```

    1. 开始载入primary集群

        优先载入primary集群，因为Secondary集群的初始化可能会依赖primary集群，比如EDS类型的Cluster就需要依赖一个primary类型的xds集群来提供xds server的地址。

        ```C++
          // Build book-keeping for which clusters are primary. This is useful when we
          // invoke loadCluster() below and it needs the complete set of primaries.
          for (const auto& cluster : bootstrap.static_resources().clusters()) {
            if (is_primary_cluster(cluster)) {
              primary_clusters_.insert(cluster.name());
            }
          }

        // Load all the primary clusters.
          for (const auto& cluster : bootstrap.static_resources().clusters()) {
            if (is_primary_cluster(cluster)) {
              loadCluster(cluster, "", false, active_clusters_);
            }
          }
        ```

    2. 开启创建adx通道，后续要在这个通道上创建cds api

        ```C++
          // Now setup ADS if needed, this might rely on a primary cluster.
          // This is the only point where distinction between delta ADS and state-of-the-world ADS is made.
          // After here, we just have a GrpcMux interface held in ads_mux_, which hides
          // whether the backing implementation is delta or SotW.
          if (dyn_resources.has_ads_config()) {
            if (dyn_resources.ads_config().api_type() ==
                envoy::config::core::v3::ApiConfigSource::DELTA_GRPC) {
              ads_mux_ = std::make_shared<Config::NewGrpcMuxImpl>(
                  Config::Utility::factoryForGrpcApiConfigSource(*async_client_manager_,
                                                                dyn_resources.ads_config(), stats, false)
                      ->create(),
                  main_thread_dispatcher,
                  *Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
                      Config::Utility::getAndCheckTransportVersion(dyn_resources.ads_config()) ==
                              envoy::config::core::v3::ApiVersion::V3
                          // TODO(htuch): consolidate with type_to_endpoint.cc, once we sort out the future
                          // direction of that module re: https://github.com/envoyproxy/envoy/issues/10650.
                          ? "envoy.service.discovery.v3.AggregatedDiscoveryService.DeltaAggregatedResources"
                          : "envoy.service.discovery.v2.AggregatedDiscoveryService."
                            "DeltaAggregatedResources"),
                  Config::Utility::getAndCheckTransportVersion(dyn_resources.ads_config()), random_, stats_,
                  Envoy::Config::Utility::parseRateLimitSettings(dyn_resources.ads_config()), local_info);
            } else {
              ads_mux_ = std::make_shared<Config::GrpcMuxImpl>(
                  local_info,
                  Config::Utility::factoryForGrpcApiConfigSource(*async_client_manager_,
                                                                dyn_resources.ads_config(), stats, false)
                      ->create(),
                  main_thread_dispatcher,
                  *Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
                      Config::Utility::getAndCheckTransportVersion(dyn_resources.ads_config()) ==
                              envoy::config::core::v3::ApiVersion::V3
                          // TODO(htuch): consolidate with type_to_endpoint.cc, once we sort out the future
                          // direction of that module re: https://github.com/envoyproxy/envoy/issues/10650.
                          ? "envoy.service.discovery.v3.AggregatedDiscoveryService."
                            "StreamAggregatedResources"
                          : "envoy.service.discovery.v2.AggregatedDiscoveryService."
                            "StreamAggregatedResources"),
                  Config::Utility::getAndCheckTransportVersion(dyn_resources.ads_config()), random_, stats_,
                  Envoy::Config::Utility::parseRateLimitSettings(dyn_resources.ads_config()),
                  bootstrap.dynamic_resources().ads_config().set_node_on_first_message_only());
            }
          } else {
            ads_mux_ = std::make_unique<Config::NullGrpcMuxImpl>();
          }
        ```

    3. 开始载入静态的Secondary集群

      基于文件的这种Secondary类型的Cluster是一个例外，可以不依赖任何Primary Clusetr就可以初始化，也不依赖xds，因此在这里归为Primary类型的Cluster优先初始化。

        ```C++
          // After ADS is initialized, load EDS static clusters as EDS config may potentially need ADS.
          for (const auto& cluster : bootstrap.static_resources().clusters()) {
            // Now load all the secondary clusters.
            if (cluster.type() == envoy::config::cluster::v3::Cluster::EDS &&
                cluster.eds_cluster_config().eds_config().config_source_specifier_case() !=
                    envoy::config::core::v3::ConfigSource::ConfigSourceSpecifierCase::kPath) {
              loadCluster(cluster, "", false, active_clusters_);
            }
          }
        ```

    4. 创建LocalClusterParams，通过LocalClusterParams

        如果定义了local cluster会在这里进行初始化，local cluster目的为了做zone aware的load balancer。构建load balancer的时候需要用到。

        ```C++
          absl::optional<ThreadLocalClusterManagerImpl::LocalClusterParams> local_cluster_params;
          if (local_cluster_name_) {
            auto local_cluster = active_clusters_.find(local_cluster_name_.value());
            if (local_cluster == active_clusters_.end()) {
              throw EnvoyException(
                  fmt::format("local cluster '{}' must be defined", local_cluster_name_.value()));
            }
            local_cluster_params.emplace();
            local_cluster_params->info_ = local_cluster->second->cluster().info();
            local_cluster_params->load_balancer_factory_ = local_cluster->second->loadBalancerFactory();
            local_cluster->second->setAddedOrUpdated();
          }
        ```


    5. 创建`ThreadLocalClusterManagerImpl`，这个结构是Thread Local的，可以安全的被work线程访问。

        虽然我们有`ClusterManager`接口，但是这个接口是提供个主线程的，用来添加、更新、删除集群的，并非是线程安全的。而且我们在worker线程也不需要做集群的添加、更新和删除。
        因此这里有了`ThreadLocalClusterManagerImpl`，他实现了`ThreadLocalCluster`接口，并提供了一些有限的一些接口提供给worker线程来访问我们的Cluster。

        ```C++
          // Once the initial set of static bootstrap clusters are created (including the local cluster),
          // we can instantiate the thread local cluster manager.
          tls_->set([this, local_cluster_name](
                        Event::Dispatcher& dispatcher) -> ThreadLocal::ThreadLocalObjectSharedPtr {
            return std::make_shared<ThreadLocalClusterManagerImpl>(*this, dispatcher, local_cluster_name);
        ```


    6. 创建cds api
        
        这里通过我们的`ClusterManagerFactory`创建cds api，在初始化Secondary Cluster的时候会用到cds api。

        ```C++
          // We can now potentially create the CDS API once the backing cluster exists.
          if (dyn_resources.has_cds_config()) {
            cds_api_ = factory_.createCds(dyn_resources.cds_config(), *this);
            init_helper_.setCds(cds_api_.get());
          } else {
            init_helper_.setCds(nullptr);
          }
        ```


    7. 开始对所有静态集群进行初始化

        在最后遍历我们的所有Primary集群，对这些集群进行初始化。

        ```C++
          // Proceed to add all static bootstrap clusters to the init manager. This will immediately
          // initialize any primary clusters. Post-init processing further initializes any thread
          // aware load balancer and sets up the per-worker host set updates.
          for (auto& cluster : active_clusters_) {
            init_helper_.addCluster(*cluster.second);
          }
        ```

4. Primary Cluster初始化完成后会进行RTDS的初始化

    因为后续的其他组件在进行初始化的时候会依赖RTDS，因此需要先初始化RTDS

    ```C++
      clusterManager().setPrimaryClustersInitializedCb(
          [this]() { onClusterManagerPrimaryInitializationComplete(); });
      void InstanceImpl::onClusterManagerPrimaryInitializationComplete() {
        // If RTDS was not configured the `onRuntimeReady` callback is immediately invoked.
        Runtime::LoaderSingleton::get().startRtdsSubscriptions([this]() { onRuntimeReady(); });
      }
    ```

5. RTDS初始化完成后开始进行Secondary Cluster的初始化
    
    在进行Secondary Cluster的时候会通过`maybeFinishInitialize`启动cds，然后等待`Secondary Cluster`完成初始化。
    
    !!! note
        `maybeFinishInitialize`的初始化见下文具体分析

    ```C++
    void InstanceImpl::onRuntimeReady() {
      // Begin initializing secondary clusters after RTDS configuration has been applied.
      // Initializing can throw exceptions, so catch these.
      try {
        clusterManager().initializeSecondaryClusters(bootstrap_);
      } catch (const EnvoyException& e) {
        ENVOY_LOG(warn, "Skipping initialization of secondary cluster: {}", e.what());
        shutdown();
      }
      ......
    }

    void ClusterManagerImpl::initializeSecondaryClusters(
    const envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      init_helper_.startInitializingSecondaryClusters();

      const auto& cm_config = bootstrap.cluster_manager();
      if (cm_config.has_load_stats_config()) {
        const auto& load_stats_config = cm_config.load_stats_config();

        load_stats_reporter_ = std::make_unique<LoadStatsReporter>(
            local_info_, *this, stats_,
            Config::Utility::factoryForGrpcApiConfigSource(*async_client_manager_, load_stats_config,
                                                          stats_, false)
                ->create(),
            Config::Utility::getAndCheckTransportVersion(load_stats_config), dispatcher_);
      }
    }
    ```

## 核心方法和类分析


!!! note 
    无论是Primary cluster、还是Secondary Cluster，最终都是通过loadCluster把Cluster Protobuf变成Cluster对象。这两者的区别就是`added_via_api`，前者为false、后者为true。这个参数表明是否是通过API获取的。很明显Primary都不是通过API来获取的。

    ```C++
    ClusterManagerImpl::loadCluster(const envoy::config::cluster::v3::Cluster& cluster,
                                    const std::string& version_info, bool added_via_api,
                                    ClusterMap& cluster_map)
    ```

这个方法主要做了以下几件事:

1. 通过ClusterManagerFactory以及Cluster的Protobuf来创建`Cluster`和`ThreadAwareLoadBalancer`

    ```C++
      std::pair<ClusterSharedPtr, ThreadAwareLoadBalancerPtr> new_cluster_pair =
          factory_.clusterFromProto(cluster, *this, outlier_event_logger_, added_via_api);
      auto& new_cluster = new_cluster_pair.first;
      Cluster& cluster_reference = *new_cluster;
    ```

    `Cluster`是对集群的抽象，而`ThreadAwareLoadBalancer`则是对这个集群`Load Balancer`的抽象，这个load balancer是感知线程的。
    在实现自定义集群的时候需要自己来实现，目前Envoy中只有`Dynamic forward proxy`、`Aggregate`、`redis`等三种类似的集群是实现了`ThreadAwareLoadBalancer`接口，
    他们有自己专用的LoadBalancer，其他的集群用的都是Envoy内置的的几个标准Load Balancer实现。比如`Aggregate`集群的构造函数如下，他创建了`AggregateThreadAwareLoadBalancer`，
    属于这个集群特有的LoadBalancer

    ```C++
    std::pair<Upstream::ClusterImplBaseSharedPtr, Upstream::ThreadAwareLoadBalancerPtr>
    ClusterFactory::createClusterWithConfig(
        const envoy::config::cluster::v3::Cluster& cluster,
        const envoy::extensions::clusters::aggregate::v3::ClusterConfig& proto_config,
        Upstream::ClusterFactoryContext& context,
        Server::Configuration::TransportSocketFactoryContextImpl& socket_factory_context,
        Stats::ScopePtr&& stats_scope) {
      auto new_cluster =
          std::make_shared<Cluster>(cluster, proto_config, context.clusterManager(), context.runtime(),
                                    context.api().randomGenerator(), socket_factory_context,
                                    std::move(stats_scope), context.addedViaApi());
      auto lb = std::make_unique<AggregateThreadAwareLoadBalancer>(*new_cluster);
      return std::make_pair(new_cluster, std::move(lb));
    }

    ```

2. 设置healthChecker、outlierDetector等callback

    ```C++
      if (new_cluster->healthChecker() != nullptr) {
        new_cluster->healthChecker()->addHostCheckCompleteCb(
            [this](HostSharedPtr host, HealthTransition changed_state) {
              if (changed_state == HealthTransition::Changed &&
                  host->healthFlagGet(Host::HealthFlag::FAILED_ACTIVE_HC)) {
                postThreadLocalHealthFailure(host);
              }
            });
      }

      if (new_cluster->outlierDetector() != nullptr) {
        new_cluster->outlierDetector()->addChangedStateCb([this](HostSharedPtr host) {
          if (host->healthFlagGet(Host::HealthFlag::FAILED_OUTLIER_CHECK)) {
            postThreadLocalHealthFailure(host);
          }
        });
      }
    ```

    这么做的目的是因为，当开启健康检查和离群检测的时候，有出现异常机器需要通知所有的线程进行连接的处理，比如drain connection、close conntions然后从连接池中移除等操作。

3. 从ClusterMap中查找Cluster，如果已经存在就替换，没有则新增

    ```C++
      ClusterDataPtr result;
      auto cluster_entry_it = cluster_map.find(cluster_reference.info()->name());
      if (cluster_entry_it != cluster_map.end()) {
        result = std::exchange(cluster_entry_it->second,
                              std::make_unique<ClusterData>(cluster, version_info, added_via_api,
                                                            std::move(new_cluster), time_source_));
      } else {
        bool inserted = false;
        std::tie(cluster_entry_it, inserted) =
            cluster_map.emplace(cluster_reference.info()->name(),
                                std::make_unique<ClusterData>(cluster, version_info, added_via_api,
                                                              std::move(new_cluster), time_source_));
        ASSERT(inserted);
      }
    ```

4. 给RingHash、MaglevLoadBalancer等创建ThreadAware load balancer

    ```C++
      // If an LB is thread aware, create it here. The LB is not initialized until cluster pre-init
      // finishes. For RingHash/Maglev don't create the LB here if subset balancing is enabled,
      // because the thread_aware_lb_ field takes precedence over the subset lb).
      if (cluster_reference.info()->lbType() == LoadBalancerType::RingHash) {
        if (!cluster_reference.info()->lbSubsetInfo().isEnabled()) {
          cluster_entry_it->second->thread_aware_lb_ = std::make_unique<RingHashLoadBalancer>(
              cluster_reference.prioritySet(), cluster_reference.info()->stats(),
              cluster_reference.info()->statsScope(), runtime_, random_,
              cluster_reference.info()->lbRingHashConfig(), cluster_reference.info()->lbConfig());
        }
      } else if (cluster_reference.info()->lbType() == LoadBalancerType::Maglev) {
        if (!cluster_reference.info()->lbSubsetInfo().isEnabled()) {
          cluster_entry_it->second->thread_aware_lb_ = std::make_unique<MaglevLoadBalancer>(
              cluster_reference.prioritySet(), cluster_reference.info()->stats(),
              cluster_reference.info()->statsScope(), runtime_, random_,
              cluster_reference.info()->lbMaglevConfig(), cluster_reference.info()->lbConfig());
        }
      } else if (cluster_reference.info()->lbType() == LoadBalancerType::ClusterProvided) {
        cluster_entry_it->second->thread_aware_lb_ = std::move(new_cluster_pair.second);
      }
    ```


!!! note 
    Envoy的Cluster初始化是一个状态机、而且还是一个异步的过程，每一步的初始化需要等待初始化完成才能进行下一步的初始化，所以我们会看到代码中经常会去调用`maybeFinishInitialize`来判断当前是否完成了初始化，并进行状态机的更新。

    ```C++
    void ClusterManagerInitHelper::maybeFinishInitialize()
    ```

1. 如果还没有进行Primary Cluster等待载入以及在等待CdsApi初始化，则直接跳过。

    ```C++
      // Do not do anything if we are still doing the initial static load or if we are waiting for
      // CDS initialize.
      ENVOY_LOG(debug, "maybe finish initialize state: {}", enumToInt(state_));
      if (state_ == State::Loading || state_ == State::WaitingToStartCdsInitialization) {
        return;
      }
    ```

    因为如果处于这两个状态的话，表明还没有开始初始化Primary Cluster、或者是还没开始进行Secondary的初始化。


2. 只有等待Primary Cluster的初始化或者是等待Secondary Clusetr初始化这两类状态才需要继续进行判断

    ```C++
      ASSERT(state_ == State::WaitingToStartSecondaryInitialization ||
            state_ == State::CdsInitialized ||
            state_ == State::WaitingForPrimaryInitializationToComplete);
      ENVOY_LOG(debug, "maybe finish initialize primary init clusters empty: {}",
                primary_init_clusters_.empty());
      // If we are still waiting for primary clusters to initialize, do nothing.
      if (!primary_init_clusters_.empty()) {
        return;
      } else if (state_ == State::WaitingForPrimaryInitializationToComplete) {
        state_ = State::WaitingToStartSecondaryInitialization;
        if (primary_clusters_initialized_callback_) {
          primary_clusters_initialized_callback_();
        }
        return;
      }
    ```

    首先判断是否完成了Primary Cluster的初始化，Primary Cluster初始化完成的标志就是primary_init_clusters_为空，因为载入的时候会把所有的Primary CLuster存进去，
    然后遍历这个列表进行初始化，初始化完成的话则从这个列表中移除，因此这个列表为空就表明初始化完成了。 


    ```C++
    ClusterManagerInitHelper::addCluster(ClusterManagerCluster& cm_cluster) {
      ....
      if (cluster.initializePhase() == Cluster::InitializePhase::Primary) {
        // Remove the previous cluster before the cluster object is destroyed.
        primary_init_clusters_.remove_if(
            [name_to_remove = cluster.info()->name()](ClusterManagerCluster* cluster_iter) {
              return cluster_iter->cluster().info()->name() == name_to_remove;
            });
        primary_init_clusters_.push_back(&cm_cluster);
        cluster.initialize(initialize_cb);
    }


    void ClusterManagerInitHelper::removeCluster(ClusterManagerCluster& cluster) {
      .....
      // There is a remote edge case where we can remove a cluster via CDS that has not yet been
      // initialized. When called via the remove cluster API this code catches that case.
      std::list<ClusterManagerCluster*>* cluster_list;
      if (cluster.cluster().initializePhase() == Cluster::InitializePhase::Primary) {
        cluster_list = &primary_init_clusters_;
      } else {
        ASSERT(cluster.cluster().initializePhase() == Cluster::InitializePhase::Secondary);
        cluster_list = &secondary_init_clusters_;
      }

      // It is possible that the cluster we are removing has already been initialized, and is not
      // present in the initializer list. If so, this is fine.
      cluster_list->remove(&cluster);
    }
    ```

3. 如果Primary集群都初始化完成了，那接下来就看是否是在做Secondary cluster的初始化

    ```C++
      // If we are still waiting for secondary clusters to initialize, see if we need to first call
      // initialize on them. This is only done once.
      ENVOY_LOG(debug, "maybe finish initialize secondary init clusters empty: {}",
                secondary_init_clusters_.empty());
      if (!secondary_init_clusters_.empty()) {
        if (!started_secondary_initialize_) {
          ENVOY_LOG(info, "cm init: initializing secondary clusters");
          // If the first CDS response doesn't have any primary cluster, ClusterLoadAssignment
          // should be already paused by CdsApiImpl::onConfigUpdate(). Need to check that to
          // avoid double pause ClusterLoadAssignment.
          Config::ScopedResume maybe_resume_eds;
          if (cm_.adsMux()) {
            const auto type_urls =
                Config::getAllVersionTypeUrls<envoy::config::endpoint::v3::ClusterLoadAssignment>();
            maybe_resume_eds = cm_.adsMux()->pause(type_urls);
          }
          initializeSecondaryClusters();
        }
        return;
      }
    ```

    `secondary_init_clusters_`不为空表明Secondary cluster还没有开始初始化或者没初始化完成，这个时候如果`started_secondary_initialize_`为false，表明
    没有开始初始化。此时通过调用`initializeSecondaryClusters`开始正在的进行Secondary的初始化。

4. 初始化CDS，否则没有拿到所有的cluster没办法进行Seondary cluster的初始化

    ```C++
      // At this point, if we are doing static init, and we have CDS, start CDS init. Otherwise, move
      // directly to initialized.
      started_secondary_initialize_ = false;
      ENVOY_LOG(debug, "maybe finish initialize cds api ready: {}", cds_ != nullptr);
      if (state_ == State::WaitingToStartSecondaryInitialization && cds_) {
        ENVOY_LOG(info, "cm init: initializing cds");
        state_ = State::WaitingToStartCdsInitialization;
        cds_->initialize();
      } else {
        ENVOY_LOG(info, "cm init: all clusters initialized");
        state_ = State::AllClustersInitialized;
        if (initialized_callback_) {
          initialized_callback_();
        }
      }
    ```

    cds初始化完成后会发送xds请求给控制面获取所有的cluster，当收到所有的cluster的时候，就触发cds设置的callback，在callback里面会再次触发`maybeFinishInitialize`
    这个时候就走到了步骤3中的逻辑了。

    ```C++
    void ClusterManagerInitHelper::setCds(CdsApi* cds) {
      ASSERT(state_ == State::Loading);
      cds_ = cds;
      if (cds_) {
        cds_->setInitializedCb([this]() -> void {
          ASSERT(state_ == State::WaitingToStartCdsInitialization);
          state_ = State::CdsInitialized;
          maybeFinishInitialize();
        });
      }
    }
    ```


最后总结下，maybeFinishInitialize的几个调用时机:

1. 加载完所有的static cluster后会调用一次

    因为需要设置进入到Secondary cluster的初始化的状态，如果Primary Cluster已经初始化完成了，那就直接返回，并成功设置`WaitingToStartSecondaryInitialization`状态。
    否则就直接返回。


    ```C++
    void ClusterManagerInitHelper::onStaticLoadComplete() {
      ASSERT(state_ == State::Loading);
      // After initialization of primary clusters has completed, transition to
      // waiting for signal to initialize secondary clusters and then CDS.
      state_ = State::WaitingForPrimaryInitializationToComplete;
      maybeFinishInitialize();
    }
    ```

2. 一个集群初始化完成后，从列表中移除Cluster时。

    Cluster初始化完成后就会调用`removeCluster`将Cluster从列表中移除，所有的Cluster都初始化完才认为是初始化完成，因此每次一个Cluster初始化完成后就
    调用一次`maybeFinishInitialize()`判断是否完成了初始化。

    ```C++
    void ClusterManagerInitHelper::removeCluster(ClusterManagerCluster& cluster) {
      if (state_ == State::AllClustersInitialized) {
        return;
      }

      // There is a remote edge case where we can remove a cluster via CDS that has not yet been
      // initialized. When called via the remove cluster API this code catches that case.
      std::list<ClusterManagerCluster*>* cluster_list;
      if (cluster.cluster().initializePhase() == Cluster::InitializePhase::Primary) {
        cluster_list = &primary_init_clusters_;
      } else {
        ASSERT(cluster.cluster().initializePhase() == Cluster::InitializePhase::Secondary);
        cluster_list = &secondary_init_clusters_;
      }

      // It is possible that the cluster we are removing has already been initialized, and is not
      // present in the initializer list. If so, this is fine.
      cluster_list->remove(&cluster);
      ENVOY_LOG(debug, "cm init: init complete: cluster={} primary={} secondary={}",
                cluster.cluster().info()->name(), primary_init_clusters_.size(),
                secondary_init_clusters_.size());
      maybeFinishInitialize();
    }
    ```

3. 开始进行Secondary Cluster初始化

    Primary Cluster初始化完成后会在其callback中开始继续进行Secondary Cluster的初始化。

    ```C++

    clusterManager().setPrimaryClustersInitializedCb(
          [this]() { onClusterManagerPrimaryInitializationComplete(); });

    void InstanceImpl::onClusterManagerPrimaryInitializationComplete() {
      // If RTDS was not configured the `onRuntimeReady` callback is immediately invoked.
      Runtime::LoaderSingleton::get().startRtdsSubscriptions([this]() { onRuntimeReady(); });
    }

    void InstanceImpl::onRuntimeReady() {
      // Begin initializing secondary clusters after RTDS configuration has been applied.
      // Initializing can throw exceptions, so catch these.
      try {
        clusterManager().initializeSecondaryClusters(bootstrap_);
      } catch (const EnvoyException& e) {
        ENVOY_LOG(warn, "Skipping initialization of secondary cluster: {}", e.what());
        shutdown();
      }
      .......
    }
    ```

    ```C++
    void ClusterManagerInitHelper::startInitializingSecondaryClusters() {
      ASSERT(state_ == State::WaitingToStartSecondaryInitialization);
      ENVOY_LOG(debug, "continue initializing secondary clusters");
      maybeFinishInitialize();
    }
    ```

4. CDS初始化完成时

    CDS开始初始化，初始化完成后在其callback中再次调用

    ```C++
    void ClusterManagerInitHelper::maybeFinishInitialize() {
      // At this point, if we are doing static init, and we have CDS, start CDS init. Otherwise, move
      // directly to initialized.
      started_secondary_initialize_ = false;
      ENVOY_LOG(debug, "maybe finish initialize cds api ready: {}", cds_ != nullptr);
      if (state_ == State::WaitingToStartSecondaryInitialization && cds_) {
        ENVOY_LOG(info, "cm init: initializing cds");
        state_ = State::WaitingToStartCdsInitialization;
        cds_->initialize();
      
      .....
    }

    void ClusterManagerInitHelper::setCds(CdsApi* cds) {
      ASSERT(state_ == State::Loading);
      cds_ = cds;
      if (cds_) {
        cds_->setInitializedCb([this]() -> void {
          ASSERT(state_ == State::WaitingToStartCdsInitialization);
          state_ = State::CdsInitialized;
          maybeFinishInitialize();
        });
      }
    }

    ```

!!! note
    每一个Cluster初始化完成后都会在其callback中调用这个方法进行Cluster额外的初始化。在这个初始化中会添加一些callback
    最后触发thread local cluster的更新，以确保每一个thread都包含了最新的cluster内容了。

    ```C++
    void ClusterManagerImpl::onClusterInit(ClusterManagerCluster& cm_cluster);
    ```

1. 如果是一个warm cluster则会被转换成active cluster

    ```C++
      // This routine is called when a cluster has finished initializing. The cluster has not yet
      // been setup for cross-thread updates to avoid needless updates during initialization. The order
      // of operations here is important. We start by initializing the thread aware load balancer if
      // needed. This must happen first so cluster updates are heard first by the load balancer.
      // Also, it assures that all of clusters which this function is called should be always active.
      auto& cluster = cm_cluster.cluster();
      auto cluster_data = warming_clusters_.find(cluster.info()->name());
      // We have a situation that clusters will be immediately active, such as static and primary
      // cluster. So we must have this prevention logic here.
      if (cluster_data != warming_clusters_.end()) {
        clusterWarmingToActive(cluster.info()->name());
        updateClusterCounts();
      }
    ```

2. 开始初始化thread aware load balancer

    ```C++
      cluster_data = active_clusters_.find(cluster.info()->name());

      if (cluster_data->second->thread_aware_lb_ != nullptr) {
        cluster_data->second->thread_aware_lb_->initialize();
      }
    ```

3. 设置priority set callback

    这一步的目的是为了在host发生变化的时候做一些事情，比如`addMemberUpdateCb`会在host移除的时候做一些drain connection的操作。
    而`addPriorityUpdateCb`则需要触发ThreadLocalCluster的更新

    ```C++
      // Now setup for cross-thread updates.
      cluster.prioritySet().addMemberUpdateCb(
          [&cluster, this](const HostVector&, const HostVector& hosts_removed) -> void {
            if (cluster.info()->lbConfig().close_connections_on_host_set_change()) {
              for (const auto& host_set : cluster.prioritySet().hostSetsPerPriority()) {
                // This will drain all tcp and http connection pools.
                postThreadLocalDrainConnections(cluster, host_set->hosts());
              }
            } else {
              // TODO(snowp): Should this be subject to merge windows?

              // Whenever hosts are removed from the cluster, we make each TLS cluster drain it's
              // connection pools for the removed hosts. If `close_connections_on_host_set_change` is
              // enabled, this case will be covered by first `if` statement, where all
              // connection pools are drained.
              if (!hosts_removed.empty()) {
                postThreadLocalDrainConnections(cluster, hosts_removed);
              }
            }
          });

      cluster.prioritySet().addPriorityUpdateCb([&cm_cluster, this](uint32_t priority,
                                                                    const HostVector& hosts_added,
                                                                    const HostVector& hosts_removed) {
        // This fires when a cluster is about to have an updated member set. We need to send this
        // out to all of the thread local configurations.

        // Should we save this update and merge it with other updates?
        //
        // Note that we can only _safely_ merge updates that have no added/removed hosts. That is,
        // only those updates that signal a change in host healthcheck state, weight or metadata.
        //
        // We've discussed merging updates related to hosts being added/removed, but it's really
        // tricky to merge those given that downstream consumers of these updates expect to see the
        // full list of updates, not a condensed one. This is because they use the broadcasted
        // HostSharedPtrs within internal maps to track hosts. If we fail to broadcast the entire list
        // of removals, these maps will leak those HostSharedPtrs.
        //
        // See https://github.com/envoyproxy/envoy/pull/3941 for more context.
        bool scheduled = false;
        const auto merge_timeout = PROTOBUF_GET_MS_OR_DEFAULT(cm_cluster.cluster().info()->lbConfig(),
                                                              update_merge_window, 1000);
        // Remember: we only merge updates with no adds/removes — just hc/weight/metadata changes.
        const bool is_mergeable = hosts_added.empty() && hosts_removed.empty();

        if (merge_timeout > 0) {
          // If this is not mergeable, we should cancel any scheduled updates since
          // we'll deliver it immediately.
          scheduled = scheduleUpdate(cm_cluster, priority, is_mergeable, merge_timeout);
        }

        // If an update was not scheduled for later, deliver it immediately.
        if (!scheduled) {
          cm_stats_.cluster_updated_.inc();
          postThreadLocalClusterUpdate(
              cm_cluster, ThreadLocalClusterUpdateParams(priority, hosts_added, hosts_removed));
        }
      });
    ```

4. 生成更新参数，最后触发thread local cluster更新

    ```C++
      // Finally, post updates cross-thread so the per-thread load balancers are ready. First we
      // populate any update information that may be available after cluster init.
      ThreadLocalClusterUpdateParams params;
      for (auto& host_set : cluster.prioritySet().hostSetsPerPriority()) {
        if (host_set->hosts().empty()) {
          continue;
        }
        params.per_priority_update_params_.emplace_back(host_set->priority(), host_set->hosts(),
                                                        HostVector{});
      }
      // NOTE: In all cases *other* than the local cluster, this is when a cluster is added/updated
      // The local cluster must currently be statically defined and must exist prior to other
      // clusters being added/updated. We could gate the below update on hosts being available on
      // the cluster or the cluster not already existing, but the special logic is not worth it.
      postThreadLocalClusterUpdate(cm_cluster, std::move(params));
    ```


!!! note
    一个Cluster初始化完成或者更新后，需要更新所有的Thread Local中，让所有的Thread可以拿到最新的Cluster

    ```C++
    void ClusterManagerImpl::postThreadLocalClusterUpdate(ClusterManagerCluster& cm_cluster,
                                                          ThreadLocalClusterUpdateParams&& params);
    ```

1. 设置集群的`AddedOrUpdated`位，表明已经更新了

    ```C++
      bool add_or_update_cluster = false;
      if (!cm_cluster.addedOrUpdated()) {
        add_or_update_cluster = true;
        cm_cluster.setAddedOrUpdated();
      }

    ```

2. 开始生成update hosts params、locality weight、overprovision_factor等需要参数

    各个thread中的Cluster Priority Set会根据这些参数来进行更新。

    ```C++
      for (auto& per_priority : params.per_priority_update_params_) {
        const auto& host_set =
            cm_cluster.cluster().prioritySet().hostSetsPerPriority()[per_priority.priority_];
        per_priority.update_hosts_params_ = HostSetImpl::updateHostsParams(*host_set);
        per_priority.locality_weights_ = host_set->localityWeights();
        per_priority.overprovisioning_factor_ = host_set->overprovisioningFactor();
      }
    ```

3. 开始在各个线程中运行 

    在ThreadLocal中获取到ThreadLocalClusterManagerImpl

    ```C++
      tls_.runOnAllThreads(
          [info = cm_cluster.cluster().info(), params = std::move(params), add_or_update_cluster,
          load_balancer_factory](OptRef<ThreadLocalClusterManagerImpl> cluster_manager) {
            ThreadLocalClusterManagerImpl::ClusterEntry* new_cluster = nullptr;
            if (add_or_update_cluster) {
              if (cluster_manager->thread_local_clusters_.count(info->name()) > 0) {
                ENVOY_LOG(debug, "updating TLS cluster {}", info->name());
              } else {
                ENVOY_LOG(debug, "adding TLS cluster {}", info->name());
              }

              new_cluster = new ThreadLocalClusterManagerImpl::ClusterEntry(*cluster_manager, info,
                                                                            load_balancer_factory);
              cluster_manager->thread_local_clusters_[info->name()].reset(new_cluster);
            }

            for (const auto& per_priority : params.per_priority_update_params_) {
              cluster_manager->updateClusterMembership(
                  info->name(), per_priority.priority_, per_priority.update_hosts_params_,
                  per_priority.locality_weights_, per_priority.hosts_added_,
                  per_priority.hosts_removed_, per_priority.overprovisioning_factor_);
            }

            if (new_cluster != nullptr) {
              for (auto& cb : cluster_manager->update_callbacks_) {
                cb->onClusterAddOrUpdate(*new_cluster);
              }
            }
          });
    ```