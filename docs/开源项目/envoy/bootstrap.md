# Bootstrap启动过程分析

1. 载入Bootstrap启动yaml文件
2. 设置header prefix
3. 初始化stats、设置TagProducer、StatsMatcher、HistogramSettings等
4. 创建Server stats
5. 注册assert action、bug action
6. 设置healthy check为false


1. cluster manager包含了多阶段初始化，第一阶段要初始化的是static/DNS clsuter， 然后是预先定义的静态的EDS cluster，
   如果包含了CDS需要等待CDS收到一个response，或者是失败的响应，最后初始化CDS，接着开始初始化CDS提供的Cluster。
2. 如果集群提供了健康检查，Envoy还会进行一轮健康检查
3. 等到cluster manager初始化完毕后，RDS和LDS开始初始化，直到收到响应或者失败，在此之前Envoy是不会接受连接来处理流量的
4. 如果LDS的响应中潜入了RDS的配置，那么还需要等待RDS收到响应，这个过程被称为listener warming
5. 上述所有流程完毕后，listener开始接受流量。


>  上文中提到的收到响应或者失败，可以通过设置initial_fetch_timeout.来设置响应超时的时候，如果initial_fetch_timeout后还没有收到响应就跳过当前的初始化阶段。