# Bootstrap

1. 载入Bootstrap启动yaml文件
2. 设置header prefix
3. 初始化stats、设置TagProducer、StatsMatcher、HistogramSettings等
4. 创建Server stats
5. 注册assert action、bug action
6. 设置healthy check为false
7. 



* UDP的filter是per work hread共享的，而TCP filter则是非共享的，是per 连接的，而http filter则是per 请求的。
* 