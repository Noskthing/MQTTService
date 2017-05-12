/*
 Copyright (c) 2010,2011,2013 Roger Light <roger@atchoo.org>
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3. Neither the name of mosquitto nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MOSQUITTO_INTERNAL_H_
#define _MOSQUITTO_INTERNAL_H_


#ifdef WIN32
#  include <winsock2.h>
#endif

#ifdef WITH_TLS
#include <openssl/ssl.h>
#endif
#include <stdlib.h>

#if defined(WITH_THREADING) && !defined(WITH_BROKER)
#  include <pthread.h>
#else
#  include "dummypthread.h"
#endif

#ifdef WIN32
#	if _MSC_VER < 1600
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#	else
#		include <stdint.h>
#	endif
#else
#	include <stdint.h>
#endif

#include "mosquitto.h"
#include <stdbool.h>

#ifdef WITH_BROKER
struct mosquitto_client_msg;
#endif

enum mosquitto_msg_direction {
    mosq_md_in = 0,
    mosq_md_out = 1
};

enum mosquitto_msg_state {
    mosq_ms_invalid = 0,
    mosq_ms_publish_qos0 = 1,
    mosq_ms_publish_qos1 = 2,
    mosq_ms_wait_for_puback = 3,
    mosq_ms_publish_qos2 = 4,
    mosq_ms_wait_for_pubrec = 5,
    mosq_ms_resend_pubrel = 6,
    mosq_ms_wait_for_pubrel = 7,
    mosq_ms_resend_pubcomp = 8,
    mosq_ms_wait_for_pubcomp = 9,
    mosq_ms_send_pubrec = 10,
    mosq_ms_queued = 11
};

enum mosquitto_client_state {
    mosq_cs_new = 0,
    mosq_cs_connected = 1,
    mosq_cs_disconnecting = 2,
    mosq_cs_connect_async = 3,
    mosq_cs_connect_pending = 4
};

struct _mosquitto_packet{
    /* 报文类型 */
    uint8_t command;
    /* 是否计算过剩余长度 */
    uint8_t have_remaining;
    /* 固定报头中标示剩余长度的数值的位数*/
    uint8_t remaining_count;
    /* 报文标识符 */
    uint16_t mid;
    /* 辅助读取和写入剩余长度的参数 */
    uint32_t remaining_mult;
    /* 剩余长度 （可变报头和有效负载）*/
    uint32_t remaining_length;
    /* 包的总长度（包括固定报头） */
    uint32_t packet_length;
    /* packet读写操作时，记录剩余未完成部分的长度 */
    uint32_t to_process;
    /* 当前操作的位置 */
    uint32_t pos;
    /* 有效负载 */
    uint8_t *payload;
    struct _mosquitto_packet *next;
};


struct mosquitto_message_all{
    struct mosquitto_message_all *next;
    /* 当前message发送的时间戳 */
    time_t timestamp;
    /* 标识该条message是接收还是发送 */
    enum mosquitto_msg_direction direction;
    /* message当前的状态 */
    enum mosquitto_msg_state state;
    /* 重发标志位 */
    bool dup;
    /* message实体 */
    struct mosquitto_message msg;
};

struct mosquitto {
#ifndef WIN32
    /* socket */
    int sock;
#else
    SOCKET sock;
#endif
    //监听地址
    char *address;
    //客户端唯一标识符
    char *id;
    //用户名
    char *username;
    //密码
    char *password;
    //报文最大间隔时间，单位秒
    uint16_t keepalive;
    //会话的处理方式
    bool clean_session;
    /* 当前客户端的状态 */
    enum mosquitto_client_state state;
    /* 最后一条消息接收的时间 */
    time_t last_msg_in;
    /* 发送最后一条消息的时间 */
    time_t last_msg_out;
    /* 当ping_t不为0时代表着我们发送了一条PINGREQ等待PINGRES，值为发送PINGREQ的时间 */
    time_t ping_t;
    /* 可变报头的报文标识符 每次必须分配一个未使用的 */
    uint16_t last_mid;
    /* 当前接收的数据报 */
    struct _mosquitto_packet in_packet;
    /* 会有一个发送队列依次发送需要发送的数据报 */
    /* 当前需要发送的数据报 */
    struct _mosquitto_packet *current_out_packet;
    /* 指向发送队列的第一个数据报 */
    struct _mosquitto_packet *out_packet;
    //遗嘱消息
    struct mosquitto_message *will;
#ifdef WITH_TLS
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    char *tls_cafile;
    char *tls_capath;
    char *tls_certfile;
    char *tls_keyfile;
    int (*tls_pw_callback)(char *buf, int size, int rwflag, void *userdata);
    int tls_cert_reqs;
    char *tls_version;
    char *tls_ciphers;
    char *tls_psk;
    char *tls_psk_identity;
    bool tls_insecure;
#endif
    bool want_write;
#if defined(WITH_THREADING) && !defined(WITH_BROKER)
    pthread_mutex_t callback_mutex;
    pthread_mutex_t log_callback_mutex;
    pthread_mutex_t msgtime_mutex;
    pthread_mutex_t out_packet_mutex;
    pthread_mutex_t current_out_packet_mutex;
    pthread_mutex_t state_mutex;
    pthread_mutex_t message_mutex;
    pthread_t thread_id;
#endif
#ifdef WITH_BROKER
    bool is_bridge;
    struct _mqtt3_bridge *bridge;
    struct mosquitto_client_msg *msgs;
    struct _mosquitto_acl_user *acl_list;
    struct _mqtt3_listener *listener;
    time_t disconnect_t;
    int pollfd_index;
    int db_index;
    struct _mosquitto_packet *out_packet_last;
#else
    /*  */
    void *userdata;
    /* 是否处在回调方法里 在写入数据的时候为避免死锁会跳过等待下一个轮循再写入 */
    bool in_callback;
    /* message等待返回的最大时间间隔 */
    unsigned int message_retry;
    /* 最后一次检查retry_check的时间 */
    time_t last_retry_check;
    /* 指向等待回复messages队列的最后一条message */
    struct mosquitto_message_all *messages;
    /* 连接成功触发的回调 */
    void (*on_connect)(struct mosquitto *, void *userdata, int rc);
    /* 断开连接触发的回调 */
    void (*on_disconnect)(struct mosquitto *, void *userdata, int rc);
    /* 发布消息触发的回调 */
    void (*on_publish)(struct mosquitto *, void *userdata, int mid);
    void (*on_message)(struct mosquitto *, void *userdata, const struct mosquitto_message *message);
    /* 收到suback触发的回调*/
    void (*on_subscribe)(struct mosquitto *, void *userdata, int mid, int qos_count, const int *granted_qos);
    void (*on_unsubscribe)(struct mosquitto *, void *userdata, int mid);
    void (*on_log)(struct mosquitto *, void *userdata, int level, const char *str);
    //void (*on_error)();
    char *host;
    int port;
    int queue_len;
    // 监听地址
    char *bind_address;
    /* 延迟重连的时间  实际会根据不同逻辑在这个基础上计算出新的延迟时间 */
    unsigned int reconnect_delay;
    /* 延迟重连的最大时间间隔 */
    unsigned int reconnect_delay_max;
    /* 延迟重连的时间是否退避 */
    bool reconnect_exponential_backoff;
    /* 当前是否是多线程 */
    bool threaded;
    /* 指向发送队列的最后一个数据报 */
    struct _mosquitto_packet *out_packet_last;
    /* 指向等待回复messages队列的最后一条message */
    struct mosquitto_message_all *messages_last;
    /* 
     Qos > 0 等待回复的message
     Qos = 1时，mosq_md_out
     Qos = 2时，mosq_md_in
     */
    int inflight_messages;
    /*
     最大处于等待回复的message数量
     0 代表不限制数量
     当达到限制的时候，会先将message状态置为mosq_ms_invalid
     等待一条消息被确认后发送
     */
    int max_inflight_messages;
#endif
};

#endif
