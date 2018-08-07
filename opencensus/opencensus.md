## stats领域中的基础概念
时间序列-> 就是一系列以时间为索引的数据点
每一个时间序列通过其metric和一些了的key-value对(也称为labels)来唯一标识

1. Measure 就是代表一种类型的记录，比如请求的延迟等，在某些系统中也称为Metrics
2. ViewDescriptor view的元数据信息，是一个值类型，描述了这个view对应的Measure信息，Aggregation信息、columes信息等
3. View 
4. ViewData 
5. Record 用于记录一系列的Measurement
6. MeasureRegistry 用于将一个记录的所有的Measuredescriptor注册进去，并提供了一系列的函数用于查询Measure对应的MeasureDescriptor
7. Measurement 是一个pair，包含了Measure和其对应的值
8. StatsManager 
9. MeasureDescriptor Measure的元数据信息，表示这个Measure的类型、名称、单位、描述信息等
10. AggregationWindow 聚合的周期，目前支持Cumulative 和 Interval两种方式
11. Aggregation 聚合方式，目前支持Count、Sum、Distribution等类型
12. BucketBoundaries Bucket的分布情况，目前支持Linear(线性的，按照指定宽度进行分布，Exponential 指数分布，Explicti 函数分布)
13. Distribution 传入一个BucketBoundaries，通过Add方法将值添加到Bucketboundaries，同时计算均值和平方差


MeasureregistryImpl -> RegisterImpl -> Measuredescriptor -> Measure

Measure注册的过程:
MeasureRegistry::RegisterInt
    > MeasuReregistryimpl->RegisterInt (Measureregistryimpl是一个单例，内部维护一堆Measuredescriptor还有Measure name到Measure Id的映射关系)
        -> MeasureDescriptor 构建一个Measure的描述结构，包含了name、units、description、type
        -> MeasureRegistryimpl::Registerimpl 将MeasureDescriptor进行注册
            -> MeasureRegistry::CreateMeasureId 给这个MeasureDescriptor创建MeasureId
            -> 将MeasureId还有Measure name也就是MeasureDescriptor的name进行映射关系的关联和注册
            -> 将MeasureDescriptorji进行注册
        -> Measure 通过MeasureRegistryimpl::Registerimpl的返回址构建Measure
        -> Statsmanager::AddMeasure 将Measure添加到StatsManager中，StatsManager是一个单例
            -> MeasureInfomation构造 
            -> 注册MeasureInfomation 和MeasureRegistryimpl中注册的Measure基本是一一对应的


Exporter注册的过程:
StatsExporter::RegisterPushHandler() 
    > StatsExporter::RegisterPushHandler 需要注册一个实现了opencensus::stats::StatsExporter::Handler的类实例
        -> StatsExporterImpl::RegisterPushHandler StatsExporterImpl是一个单例，
            -> 注册到内部的handler集合中
            -> 启动ExporterThread，内部定时执行Exporter
                -> 遍历所有的view
                -> 拿到view对应的ViewDescriptor和ViewData
                -> 遍历所有的Export传入ViewDescriptor和ViewData调用其ExportViewData方法


stats::View的构建过程
stats::View
    > stats::View
        -> stats::ViewDescriptor 传入view的名称、描述信息、measure的名称、聚合方法、column(维度、tag，可以理解中一个指标的多个维度)
        -> StatsManager::AddConsumer 将Viewdescriptor作为参数传入
            -> MeasureRegistryimpl::IdToIndex 拿到ViewDescriptor对应的measure_id_转换为对应的索引，通过这个索引拿到这个Measure对应的MeasureInfomation
            -> Measureinfomation::AddConsumer
                -> ViewInformation::Matches 找到和传入的ViewDescriptor相同的ViewInformation
                -> ViewInformation::AddConsumer 引用计数加1

将View注册到Exporter的过程:
ViewDescriptor::RegisterForExporter()
    > ViewDescriptor::RegisterForExporter
        -> AggregationWindow::Type 查看聚合函数的类型是否是kCumulative类型，如果是则添加进行注册
        -> StatsExporterImpl::AddView
        -> 注册到内部


Measure记录的过程:
stats::Record
    > stats::Record 提供Measure、时间、tag和value
        -> StatsManager::Record
            -> 通过Measure找到对应的index，通过index找到MeasureInfomation
            -> Measureinfomation::Record 遍历这个Measure对应的所有ViewInformation
                -> ViewInformation::Record 对应的ViewDescriptor拿到所有的tags,找到对应的tags将值放进去
                -> ViewDataImpl::Add 将对应类型的数据进行累加



## tracing
Sampler 线程安全的，是一个接口，提供了一个ShouldSample的方法用来表明是否应该进行采样
ProbabilitySampler Sampler的一种实现，概率性采样
AlwaysSampler 总是采样，默认实现就是return true
NeverSampler 不采样，默认实现就是return false
SpanContext 包含了TraceId、SpanId、TraceOptions等信息
SpanId 64位的标识符`uint8_t rep_[8]`全是0的情况则是无效的SpanId，SpanId的创建方式是从一块buf中取前8字节的方式形成
TraceOptions 用来表示一个trace的相关操作，这些操作会被传播到和这个trace相关的所有span中，目前仅支持IsSampled操作，内部也是通过一个64位的buf存储
TraceId 是一个128位的标识符，在同一个trace中是始终保持不变的 `uint8_t rep_[16]`，其创建也是从一块buf中取前16个字节的方式形成
AttributeValueRef 对属性值的一个引用屏蔽了一个valued到底是int、string、bool、float
