# MQTTService

模仿Mosquitto实现一个iOS客户端的MQTT网络库

这里是[MQTT协议中文文档](https://www.gitbook.com/book/mcxiaoke/mqtt-cn)。根据文档第三章的内容，逐步实现MQTT协议各种控制报文。但根据简易程度的不同调整了实现的小节顺序

参照Commit，实现顺序依次为：

  1.socket通讯的建立
  
  2.Connect Command的发送
  
  3.解析接收的消息，依据首字节高四位命令区分消息类型。
  
  4.PINGREQ/PINGRESP的处理（连接的keep alive）
  
  5.SUBSCRIBE/UNSUBSCRIBE的处理 （消息的订阅与取消订阅
  
  6.PUBLISH的实现
   > PUBLISH涉及不同Qos的处理。而且与上面的消息类型有所不同的是，PUBLISH是Broker和Client互推，需要针对每一种send以及ack都做出对应的处理。这一部分细分为以下几块
   - 不同Qos的PUBLISH的发送
   - Qos = 0：发送成功即可
   - Qos = 1：需要接收到PUBACK
   - Qos = 2：需要处理PUBREC/PUBREL/PUBCOMP三种情况
   
  -------
>>  尚未完成，更新中...
