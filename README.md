# 超新星计划项目-MQ 
为期一个月制作的消息队列。主要实现了发布订阅模型、pull模式、磁盘持久化和部分集群部署的功能。
Dirserver目前只是用来维护Broker状态和将broker信息发送给生产者、消费者提供消费。可以启动多个Dir来进行冗余避免一个进程挂了影响所有通信。
Broker 采用的是pull模式，需要消费者主动pull，同时内部维护每个消费者的提交情况。定时持久化到磁盘上。 
消费者能够通过Dir来找到需要订阅的broker（即broker主题增加了也可以及时发现）
生产者是一个线程类，采用RAII原则封装。 

## 目录结构
```$xslt
/
   /example 
      /mqbroker 消息队里中的Broker
      /mqbusd 消息队列中的生产者和消费者。附带测试程序。根据config.yaml配置生成对应的生产者、消费者
      /dirserver 服务发现和服务注册。 

       /*******以下与消息队列这些没有关系******/
      /http  简单的HTTP服务器，实现了CGI接口但是还没有验证
      /memcached  实现简单的memcached协议，目前用memaslap，性能报告在readme里面贴出来了
      /raft_server raft的服务端 raft算法的基本实现 
        /******************************/

   /skylu  网络库
      /base 基本的一些数据结构和常用的组件 
      /net 网络库的主要内容
```

##相关依赖
```
yaml-cpp


```

