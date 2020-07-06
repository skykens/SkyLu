# 单线程的KV服务器
基于memcached的协议实现的kv服务器，同时实现AOF持久化和重写机制。  

## 压测报告
以下测试没有开编译器优化，开了编译器优化后性能会更快一些。 

1. 数据：32k
2. 机器：Intel(R) Xeon(R) Platinum 8255C CPU @ 2.50GHz 8核 

![压测.png][1]


## 火焰图分析

![火焰图.png][2]



  [1]:image/32K包.png
  [2]:image/火焰图.png