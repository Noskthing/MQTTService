//
//  mosquitto.h
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef mosquitto_h
#define mosquitto_h

#include <stdio.h>
#include <stdbool.h>


#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

/* Error values */
enum mosq_err_t {
    MOSQ_ERR_CONN_PENDING = -1,
    /* 执行成功 */
    MOSQ_ERR_SUCCESS = 0,
    /* 分配内存出现了问题 */
    MOSQ_ERR_NOMEM = 1,
    /* 当前内容与协议规定出现冲突 */
    MOSQ_ERR_PROTOCOL = 2,
    /* 当前环境是非法的（一般是mosq为空）*/
    MOSQ_ERR_INVAL = 3,
    /* 当前处于未连接状态 */
    MOSQ_ERR_NO_CONN = 4,
    /* 连接被拒绝 */
    MOSQ_ERR_CONN_REFUSED = 5,
    /* 未发现目标对象 */
    MOSQ_ERR_NOT_FOUND = 6,
    MOSQ_ERR_CONN_LOST = 7,
    MOSQ_ERR_TLS = 8,
    MOSQ_ERR_PAYLOAD_SIZE = 9,
    /* 不支持当前特性 */
    MOSQ_ERR_NOT_SUPPORTED = 10,
    MOSQ_ERR_AUTH = 11,
    MOSQ_ERR_ACL_DENIED = 12,
    MOSQ_ERR_UNKNOWN = 13,
    MOSQ_ERR_ERRNO = 14,
    MOSQ_ERR_EAI = 15
};

/* MQTT specification restricts client ids to a maximum of 23 characters */
#define MOSQ_MQTT_ID_MAX_LENGTH 23

struct mosquitto_message{
    /* 标识位 */
    int mid;
    /* 主题 */
    char *topic;
    /* 有效负载 */
    void *payload;
    /* 有效负载长度 */
    int payloadlen;
    /* Qos等级 */
    int qos;
    /* retain */
    bool retain;
};

struct mosquitto;

#pragma mark set methods
int mosquitto_tls_set(struct mosquitto *mosq, const char *cafile, const char *capath, const char *certfile, const char *keyfile, int (*pw_callback)(char *buf, int size, int rwflag, void *userdata));

#pragma mark init methods
struct mosquitto *mosquitto_new(const char *id, bool clean_session ,void *userdata);
int mosquitto_init(struct mosquitto *mosq, const char *id, bool clean_session, void *userdata);
int mosquitto_set_username_pwd(struct mosquitto *mosq, const char *username, const char *password);

#pragma mark connect
int mosquitto_connect(struct mosquitto *mosq, const char *host, int port, int keepalive);
int mosquitto_reconnect(struct mosquitto *mosq);
int mosquitto_loop_forever(struct mosquitto *mosq, int timeout, int max_packet);

#pragma mark disconnect 
int mosquitto_disconnect(struct mosquitto *mosq);

#pragma mark publish
int mosquitto_publish(struct mosquitto *mosq, int *mid, const char *topic, int paylodlen, const char *paylod, int qos, bool retain);

void mosquitto_destroy(struct mosquitto *mosq);
#endif /* mosquitto_h */




