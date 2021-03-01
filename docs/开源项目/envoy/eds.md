# EDS更新过程分析 

## 重要函数分析

* `HostSetImpl::updateHosts`(针对单个hostset进行全量更新)

    1. 更新过载因子
    2. 更新`hosts_`、`healthy_hosts`、`degraded_hosts`、`exluded_hosts`、`hosts_per_locality_`等等
    3. 更新locality weight
    4. rebuildLocalityScheduler 构建健康的healthy_locality_scheduler
    5. rebuildLocalityScheduler 构建degraded的degraded_loality_scheduler
    6. 回调PriorityUpdateCb，对更新后的信息进行统计

* `PrioritySetImpl::updateHosts` (包含多个优先级，每一个优先级一个hostset，可以单独更新某一个优先级)

**参数解析:**

    1. `uint32_t priority`                                                  // 要更新的hosts属于的优先级
    2. `UpdateHostsParams&& update_hosts_params`                            // 要更新的hosts的UpdateHostsParams结构(后面会重点解释)
    3. `LocalityWeightsConstSharedPtr locality_weights`                     // 更新后的locality weight结构
    4. `const HostVector& hosts_added`                                      // 要添加的hosts
    5. `const HostVector& hosts_removed`                                    // 要移除的hosts
    6. `absl::optional<uint32_t> overprovisioning_factor = absl::nullopt`   // 是否更新过载因子，不需要的话就

间接的调用了HostSetImpl::updateHosts，其中update_hosts_params包含了构造一个hostset所需要的全部信息


```cpp
void PrioritySetImpl::updateHosts(uint32_t priority, UpdateHostsParams&& update_hosts_params,
                                  LocalityWeightsConstSharedPtr locality_weights,
                                  const HostVector& hosts_added, const HostVector& hosts_removed,
                                  absl::optional<uint32_t> overprovisioning_factor) {
  // Ensure that we have a HostSet for the given priority.
  getOrCreateHostSet(priority, overprovisioning_factor);
  static_cast<HostSetImpl*>(host_sets_[priority].get())
      ->updateHosts(std::move(update_hosts_params), std::move(locality_weights), hosts_added,
                    hosts_removed, overprovisioning_factor);

  if (!batch_update_) {
    runUpdateCallbacks(hosts_added, hosts_removed);
  }
}
```

    1. 获取或者创建指定优先级的`HostSetImpl`结构
    2. 调用HostSetImpl的`updateHosts`方法
    3. 是否是batch_update，如果不是就再调用`runUpdateCallbacks`，使用者可以注册进行回调

!!! tip
    通过BatchUpdateScope进行update的，就是batch_update，是通过一个PriorityStateManagle来进行一次性更新的。

    * runUpdateCallbacks 只在HostSet中的主机完整的添加和删除才会触发，这里面的callback是通过addMemberUpdateCb来添加的，不区分优先级。
    * runReferenceUpdateCallbacks 是在HostSet中的主机进行局部的添加和删除才会触发，这里面的callback是通过addPriorityUpdateCb来添加的，是区分优先级的。


* `PrioritySetImpl::BatchUpdateScope::updateHosts`

**参数解析:**

1. `priority`
2. `update_hosts_params`
3. `locality_weights`
4. `hosts_added`
5. `hosts_removed`
6. `overprovisioning_factor`

```cpp
void PrioritySetImpl::BatchUpdateScope::updateHosts(
    uint32_t priority, PrioritySet::UpdateHostsParams&& update_hosts_params,
    LocalityWeightsConstSharedPtr locality_weights, const HostVector& hosts_added,
    const HostVector& hosts_removed, absl::optional<uint32_t> overprovisioning_factor) {
  // We assume that each call updates a different priority.
  ASSERT(priorities_.find(priority) == priorities_.end());
  priorities_.insert(priority);

  for (const auto& host : hosts_added) {
    all_hosts_added_.insert(host);
  }

  for (const auto& host : hosts_removed) {
    all_hosts_removed_.insert(host);
  }

  parent_.updateHosts(priority, std::move(update_hosts_params), locality_weights, hosts_added,
                      hosts_removed, overprovisioning_factor);
}
```

本质上和`PrioritySetImpl::updateHosts`没啥区别，参数都是透传的，只是额外保存了优先级、和要添加的hosts、要移除的hosts
这么做的目的是为了可以计算最终的新增hosts和删除的hosts，然后回调runUpdateCallbacks，因为`PrioritySetImpl::updateHosts`一次
只能更新一个优先级的hosts，通过`BatchUpdateScope`暴露出一个相同的接口来更新，每次更新记录新增的hosts和移除的hosts。


* `EdsClusterImpl::BatchUpdateHelper::batchUpdate`

**参数解析**

1. `PrioritySet::HostUpdateCb& host_update_cb` 更新hosts

**基本过程分析:**

1. 创建PriorityStateManager
2. 遍历所有的locality_lb_endpoint
3. 调用initializePriorityFor进行优先级的初始化
4. 遍历lb_endpoints，针对每一个主机通过registerHostForPriority进行主机的注册
5. 获取PriorityStateManager中的priorityState，
6. 遍历所有的`PriorityState`
7. 从`PriorityState`中取出每一个优先级内hosts进行`updateHostsPerLocality`



* `PriorityStateManager::updateClusterPrioritySet`
通过updateDynamicHostList拿到指定优先级下更新后的hosts列表、hosts_add、host_remove还等进行最终的更新

**参数解析:**

1. `const uint32_t priority`                                                  // 要更新的hosts所在优先级
2. `HostVectorSharedPtr&& current_hosts`                                      // 当前更新完成后的所有hosts
3. `const absl::optional<HostVector>& hosts_added`                            // 添加的hosts
4. `const absl::optional<HostVector>& hosts_removed`                          // 删除的hosts
5. `const absl::optional<Upstream::Host::HealthFlag> health_checker_flag`     //
6. `absl::optional<uint32_t> overprovisioning_factor`                         // overprovisioning_factor

**基本过程分析:**

1. 如果要更新的优先级已经存在，就获取locality weight map，否则就创建一个空的locality weight map
2. 创建hosts_per_locality结构，key是locality，value是hosts
3. 遍历所有要更新的hosts
4. 给每一个hosts设置health flag
5. 更新hosts_per_locality结构
6. 遍历hosts_per_locality
6. 判断是否包含本地locality，有的话就将本地locality的hosts添加到到per_locality vector中
7. 如果开启了locality weight lb的话就将本地locality weight map放到locality_weights中，记录所有的locality的weight，是一个vector
8. 创建HostsPerLocalityImpl对象
9. 调用update_cb_的updateHosts进行更新，否则就直接更新cluster对象的prioritySet().updateHosts方法进行更新


* `EdsClusterImpl::updateHostsPerLocality`

更新指定优先级的hosts，同时也会更新locality weight map、health flag、

**参数解析:**

1. `const uint32_t priority`                                          // 要更新的hosts的优先级，
2. `const uint32_t overprovisioning_factor`                           // 要更新的hosts的过载因子
3. `const HostVector& new_hosts`,                                     // 要更新的hosts
4. `LocalityWeightsMap& locality_weights_map`                         // 已经存在的机器的locality weight map
5. `LocalityWeightsMap& new_locality_weights_map`,                    // 要更新的hosts的Locality weight map
6. `PriorityStateManager& priority_state_manager`,                    // 用来构建更新hosts状态的
7. `std::unordered_map<std::string, HostSharedPtr>& updated_hosts`    // 用来存放

**基本过程分析:**

1. 从当前`priority_set_`中获取指定`priority`的hosts，没有就新创建
2. 拷贝一份当前`priority`的hosts，传递到`updateDynamicHostList`中进行更新
3. 如果的确有更新、或者overprovisioningFactor发生了改变、或者locality weight map发生了改变，
   就调用`priority_state_manager.updateClusterPrioritySet`对通过`updateDynamicHostList`后的hosts进行更新
   然后更新locality weights map

* `BaseDynamicClusterImpl::updateDynamicHostList`
对一个新增的的hosts，和已经存在的hosts进行对比，已经存在的根据需要进行原地更新，不存在的就认为是新增的，
最后得到新增的hosts列表(hosts_added_to_current_priority)，删除的hosts列表(hosts_removed_from_current_priority)，以及最后更新完的hosts列表(current_priority_hosts)。

**参数解析:**

1. `const HostVector& new_hosts`                                      // 要更新的hosts，是per priority
2. `HostVector& current_priority_hosts`                               // 当前优先级下存在的hosts
3. `HostVector& hosts_added_to_current_priority`                      // 更新后，添加到当前优先级的机器
4. `HostVector& hosts_removed_from_current_priority`                  // 更新后，从当前优先级移除的机器
5. `HostMap& updated_hosts`                                           // 用来过滤重复的hosts，保存已经更新过的hosts，最终替换all_hosts_
6. `const HostMap& all_hosts`                                         // 当前集群存在的所有主机



* `PrioritySetImpl::batchHostUpdate`

```C++
void PrioritySetImpl::batchHostUpdate(BatchUpdateCb& callback) {
  BatchUpdateScope scope(*this);

  // We wrap the update call with a lambda that tracks all the hosts that have been added/removed.
  callback.batchUpdate(scope);

  // Now that all the updates have been complete, we can compute the diff.
  HostVector net_hosts_added = filterHosts(scope.all_hosts_added_, scope.all_hosts_removed_);
  HostVector net_hosts_removed = filterHosts(scope.all_hosts_removed_, scope.all_hosts_added_);

  runUpdateCallbacks(net_hosts_added, net_hosts_removed);
}
```

调用`BatchUpdateCb`完成hosts的更新，得到所有增加的host和移除的host然后回调update callback即可


**基本过程分析:**

1. xDS接收到EDS配置，回调 `EdsClusterImpl::onConfigUpdate`
2. 构建`BatchUpdateHelper`，传递给`PrioritySet::batchHostUpdate`进行批量更新，更新当前EDS的PrioritySet

```cpp
void EdsClusterImpl::onConfigUpdate(const Protobuf::RepeatedPtrField<ProtobufWkt::Any>& resources,
                                    const std::string&) {
  ....
  // BatchUpdateHelper实现了PrioritySet::BatchUpdateCb
  BatchUpdateHelper helper(*this, cluster_load_assignment);
  // batchHostUpdate内部会调用helper->batchUpdate
  priority_set_.batchHostUpdate(helper);
}
```

3. `PrioritySet::batchHostUpdate`内部调用`batchUpdate`进行更新

4. 调用`EdsClusterImpl::BatchUpdateHelper::batchUpdate`
  
    1. 构建`PriorityStateManager`
    2. 通过PriorityStateManager构建PriorityState，也就是hosts集合和LocalityWieghtMap
    3. 遍历新配置中的所有优先级调用`EdsClusterImpl::updateHostsPerLocality`更新对应优先级的HostSetImpl
    4. 遍历所有当前配置中存在的优先级调用`EdsClusterImpl::updateHostsPerLocality`更新对应优先级的HostSetImpl(其实就是删除，因为新配置中没有这个优先级的任何host)
    5. 更新EdsClusterImpl中存在的all_host_列表
    6. 完成配置批量更新，初始化完成


5. `HostSetImpl::partitionHosts`

通过上面的过程可以看出，核心的方法就是`EdsClusterImpl::updateHostsPerLocality`，针对单个优先级的更新。

1. 遍历所有要增加的新hosts，也就是new_hosts
2. 判断updated_hosts中是否已经存在，避免重复，达到去重的效果
3. 从all_hosts中查找是否是已经存在的host
    1. 如果是，则清楚当前机器的`PENDING_DYNAMIC_REMOVAL`标志 (Why?)
    2. 如果存在健康检查机制、并且新添加的hosts和存在的host两者的健康检查的地址不同，就认为跳过host原地更新的过程(Why?)
        1. 如果跳过hosts原地更新，则设置max_host_weight
        2. 如果存在健康检查机制就设置HealthFlag为`Host::HealthFlag::FAILED_ACTIVE_HC`
        3. 如果当前集群还在warm阶段，就设置HealthFlag为`Host::HealthFlag::PENDING_ACTIVE_HC`，这是因为如果设置为`FAILED_ACTIVE_HC`就会认为hosts初始化完成了，wram阶段就结束了
          正常的warm是包含了健康检查的过程的。
        4. 将hosts添加到updated_hosts中
        5. 将hosts添加到final_hosts中
        6. 将hosts添加到hosts_added_to_current_priority，这是确定要添加的
    3. 如果不跳过原地更新，则添加到existing_hosts_for_current_priority中
    4. 更新max_host_weight
    5. 更新现存的hosts的health flag，判断是否会因为更新health flag导致hosts changed(主要的衡量标准就是更新前后的健康状况是否发生改变了)
    6. 判断metadata是否改变，并更新metadata
    7. 判断优先级是否改变，并更新优先级，如果优先级改变了，就将hosts添加到hosts_added_to_current_priority中 (why? 为什么会发生呢?)
    8. 更新权重
    9. 将hosts添加到final_hosts中
    10. 将hosts添加到updated_hosts中
4. 遍历当前优先级下存在的hosts
5. 如果发现在existing_hosts_for_current_priority中就从existing_hosts_for_current_priority和current_priority_hosts中删除
6. 如果existing_hosts_for_current_priority不为空就表明hosts changed了 (why?)
7. 如果drainConnectionsOnHostRemoval标志没有开启，就遍历剩下的current_priority_hosts，找到那些健康的主机，更新max_host_weight，
   并添加到final_hosts和updated_hosts中，并设置PENDING_DYNAMIC_REMOVAL标志
8. 设置当前集群最大的host weight
9. current_priority_hosts剩下的机器被当作remove的hosts，添加到hosts_removed_from_current_priority中
10. 将finall_host更新为current_priority_hosts
11. 返回当前hosts是否发生了改变
