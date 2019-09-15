## Opentracing的接口规范

1. `Start a new Span` 需要有一个创建Span的接口
2. `Inject a SpanContext` 需要将SpanContext注入到SpanContext对象中，用于跨进程传输
3. `Extract a SpanContext`, 通过Carrier跨进程获取SpanContext信息

Baggage是存储在SpanContext中的一个键值对，它会在一条追踪链路上的所有span内全局传输
zipkin实现的b3-propagation 是一种SpanContext跨进程传递的一种实现方式，通过给http设置`X-B3-*`header来实现，只传递四个header信息:

1. X-B3-TraceId
2. X-B3-ParentSpanId
3. X-B3-SpanId
4. X-B3-Sampled


