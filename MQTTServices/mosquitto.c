//
//  mosquitto.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "mosquitto.h"
#include "net_mosq.h"
#include "memory_mosq.h"
#include "client_mosq.h"
#include "logger.h"
#include "time_mosq.h"
#include "handle_mosq.h"
#include "will_mosq.h"

static int _mosquitto_reconnect(struct mosquitto *mosq, bool blocking);
static int _mosquitto_loop_rc_handle(struct mosquitto *mosq, int rc);
static int _mosquitto_connect_init(struct mosquitto *mosq, const char *host, int port, int keepalive, const char *bind_address);
void _mosquitto_destroy(struct mosquitto *mosq);

int mosquitto_init(struct mosquitto *mosq, const char *id, bool clean_session, void *userdata)
{
    if (!mosq) return MOSQ_ERR_INVAL;
    
    if (clean_session == false && id == NULL)
    {
        return MOSQ_ERR_INVAL;
    }
    
    _mosquitto_destroy(mosq);
    memset(mosq, 0, sizeof(struct mosquitto));
    
    if (userdata)
    {
        mosq->userdata = userdata;
    }
    else
    {
        mosq->userdata = mosq;
    }
    mosq->sock = INVALID_SOCKET;
    mosq->keepalive = 60;
    mosq->message_retry = 20;
    mosq->last_retry_check = 0;
    mosq->clean_session = clean_session;
    
    if (id)
    {
        if (strlen(id) == 0)
        {
            return MOSQ_ERR_INVAL;
        }
        mosq->id = _mosquitto_strdup(id);
    }
    else
    {
        /* produce a random id */
        mosq->id = (char *)_mosquitto_malloc(sizeof(char));
        if (!mosq->id)
        {
            return MOSQ_ERR_NOMEM;
        }
        mosq->id[0] = 'm';
        mosq->id[1] = 'o';
        mosq->id[2] = 's';
        mosq->id[3] = 'q';
        mosq->id[4] = '/';
        
        for(int i=5; i<23; i++)
        {
            mosq->id[i] = (rand()%73)+48;
        }
    }
    mosq->in_packet.payload = NULL;
    _mosquitto_packet_cleanup(&mosq->in_packet);
    mosq->out_packet = NULL;
    mosq->current_out_packet = NULL;
    mosq->last_msg_in = mosquitto_time();
    mosq->last_msg_out = mosquitto_time();
    mosq->ping_t = 0;
    mosq->last_mid = 0;
    mosq->state = mosq_cs_new;
    mosq->messages = NULL;
    mosq->max_inflight_messages = 20;
    mosq->will = NULL;
    mosq->on_connect = NULL;
    mosq->on_publish = NULL;
    mosq->on_message = NULL;
    mosq->on_subscribe = NULL;
    mosq->on_unsubscribe = NULL;
    mosq->host = NULL;
    mosq->port = 1883;
    mosq->in_callback = false;
    mosq->queue_len = 0;
    mosq->reconnect_delay = 1;
    mosq->reconnect_delay_max = 1;
    mosq->reconnect_exponential_backoff = false;
    mosq->threaded = false;
#ifdef WITH_TLS
    mosq->ssl = NULL;
    mosq->tls_cert_reqs = SSL_VERIFY_PEER;
    mosq->tls_insecure = false;
#endif
    return MOSQ_ERR_SUCCESS;
}

static int _mosquitto_connect_init(struct mosquitto *mosq, const char *host, int port, int keepalive, const char *bind_address)
{
    if(!mosq) return MOSQ_ERR_INVAL;
    if(!host || port <= 0) return MOSQ_ERR_INVAL;
    
    if(mosq->host) _mosquitto_free(mosq->host);
    mosq->host = _mosquitto_strdup(host);
    if(!mosq->host) return MOSQ_ERR_NOMEM;
    mosq->port = port;
    
    if(mosq->bind_address) _mosquitto_free(mosq->bind_address);
    if(bind_address){
        mosq->bind_address = _mosquitto_strdup(bind_address);
        if(!mosq->bind_address) return MOSQ_ERR_NOMEM;
    }
    
    mosq->keepalive = keepalive;
    
    return MOSQ_ERR_SUCCESS;
}

struct mosquitto *mosquitto_new(const char *id, bool clean_session ,void *userdata)
{
    struct mosquitto *mosq = NULL;
    int rc;
    
    if (clean_session == false && id == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    
    mosq = (struct mosquitto *)_mosquitto_calloc(1, sizeof(struct mosquitto));
    if (mosq)
    {
        mosq->sock = INVALID_SOCKET;
#ifdef WITH_THREADING
        mosq->thread_id = pthread_self();
#endif
        rc = mosquitto_init(mosq, id, clean_session, userdata);
        if (rc)
        {
            _mosquitto_destroy(mosq);
            if (rc == MOSQ_ERR_INVAL)
            {
                errno = EINVAL;
            }
            else if (rc == MOSQ_ERR_NOMEM)
            {
                errno = ENOMEM;
            }
            return NULL;
        }
    }
    else
    {
        errno = ENOMEM;
    }
    return mosq;
}

int mosquitto_connect_bind(struct mosquitto *mosq, const char *host, int port, int keepalive, const char *bind_address)
{
    int rc;
    rc = _mosquitto_connect_init(mosq, host, port, keepalive, bind_address);
    if(rc) return rc;
    
    pthread_mutex_lock(&mosq->state_mutex);
    mosq->state = mosq_cs_new;
    pthread_mutex_unlock(&mosq->state_mutex);
    
    return _mosquitto_reconnect(mosq, true);
}

int mosquitto_connect(struct mosquitto *mosq, const char *host, int port, int keepalive)
{
    return mosquitto_connect_bind(mosq, host, port, keepalive, NULL);
}

int mosquitto_reconnect(struct mosquitto *mosq)
{
    return _mosquitto_reconnect(mosq, true);
}

static int _mosquitto_reconnect(struct mosquitto *mosq, bool blocking)
{
    LOG_INFO("_mosquitto_reconnect");
    
    int rc;
    
    //check paramter
    if (!mosq || !mosq->host || mosq->port <=0) return MOSQ_ERR_INVAL;
    
    mosq->state = mosq_cs_new;
    
    _mosquitto_packet_cleanup(&mosq->in_packet);
    
    _mosquitto_out_packet_cleanup_all(mosq);
    
    _mosq_messages_reconnect_reset(mosq);
    
    rc = _mosquitto_socket_connect(mosq, mosq->host, mosq->port, mosq->bind_address, blocking);
    if (rc) return rc;
    
    return client_send_connect_command_mosq(mosq, mosq->keepalive, mosq->clean_session);
}

int _mosquitto_packet_handle(struct mosquitto *mosq)
{
    assert(mosq);
    
    LOG_INFO("---------receive command");
    switch((mosq->in_packet.command)&0xF0){
        case PINGREQ:
            return _mosquitto_handle_pingreq(mosq);
        case PINGRESP:
            return client_receive_ping_response_mosq(mosq);
        case PUBACK:
            return _mosquitto_handle_puback(mosq, "PUBACK");
        case PUBCOMP:
            return _mosquitto_handle_puback(mosq, "PUBCOMP");
        case PUBLISH:
            return _mosquitto_handle_publish(mosq);
        case PUBREC:
            return _mosquitto_handle_pubrec(mosq);
        case PUBREL:
            return _mosquitto_handle_pubrel(mosq);
        case CONNACK:
            return client_receive_connect_ack_mosq(mosq);
        case SUBACK:
            return client_receive_suback_mosq(mosq);
        case UNSUBACK:
            return client_receive_unsubscribe_mmosq(mosq);
        default:
            /* If we don't recognise the command, return an error straight away. */
            LOG_INFO("unknown command!");
            return MOSQ_ERR_PROTOCOL;
    }
}

int _mosquitto_packet_read(struct mosquitto *mosq)
{
    if (!mosq) return  MOSQ_ERR_INVAL;
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
    
    int rc;
    
    /*
     MQTTKit的注释比较的含糊，我这里做一个简单的解释。
     有关select()和pselect()函数的使用自行百度。
     你需要知道的是当socket从网络获取到数据的时候，这个函数会被调用
     
     有关MQTT协议的具体内容，自行查看文档。
     中文文档地址:https://www.gitbook.com/book/mcxiaoke/mqtt-cn/details
     
     MQTT协议的首部是固定报头。
     首字节低四位用于指定控制报文类型的标志位，暂时用不上，取出第一个字节以后会保存起来
     高四位是MQTT控制报文的类型，这个是我们需要的。
     所以:
     第一步:读取第一个字节进行保存。 固定报头是必定存在的，所以直接读取首字节
     
     MQTT协议的内容是由固定报头，可变报头和有效负载组成的。MQTT固定报头剩下的字节，标志后面所有内容的长度。
     但需要注意的是剩余长度字段使用的是一个变长度的编码方案。对于不同大小的值编码方式不同
     具体的编码方案请查阅文档
     这里的读取和判断会比较的麻烦。因为这一部分字段是变长，可能不止一个字节，所以我们读取数据的时候可能会出现失败。
     并且读取数据以后我们需要分析判断编码方案，之后才能获取所需要的内容
     所以:
     第二步:依次判断读取 剩余长度 的大小
     
     第三步:成功获取 剩余长度 之后我们需要依次解析可变报头和paylod里面的内容。因为长度是不定所以我们需要一段一段获取，
     每次获取需要把当前的位置和获取的数据保存起来。
     
     第四步:在成功读取完所有数据之后，交由_mosquitto_handle_packet()处理
     
     第五步:释放内存并将所有参数重置
     */
    
    /* Command */
    uint8_t byte;
    ssize_t read_length;
    if (!mosq->in_packet.command)
    {
        read_length = _mosquitto_net_read(mosq, &byte, 1);
        if (read_length == 1)
        {
            mosq->in_packet.command = byte;
        }
        else
        {
            if (read_length == 0) return MOSQ_ERR_CONN_LOST;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return MOSQ_ERR_SUCCESS;
            }
            else
            {
                switch (errno)
                {
                    case ECONNRESET:
                        return MOSQ_ERR_CONN_LOST;
                    default:
                        return MOSQ_ERR_ERRNO;
                }
            }
        }
    }
    
    /*
     Remaining Length
     伪码：
     multiplier = 1
     value = 0
     do
        encodedByte = 'next byte from stream'
        value += (encodedByte AND 127) * multiplier
        multiplier *= 128
        if (multiplier > 128*128*128)
            throw Error(Malformed Remaining Length)
     while ((encodedByte AND 128) != 0)
     */
    if (!mosq->in_packet.have_remaining)
    {
        /* Get remaining length */
        do
        {
            read_length = _mosquitto_net_read(mosq, &byte, 1);
            if (read_length == 1)
            {
                mosq->in_packet.remaining_count ++;
                if (mosq->in_packet.remaining_count > 4) return MOSQ_ERR_PROTOCOL;
                
                mosq->in_packet.remaining_length += (byte & 127) * mosq->in_packet.remaining_mult;
                mosq->in_packet.remaining_mult *= 128;
                if (mosq->in_packet.remaining_mult >128 * 128 * 128) return MOSQ_ERR_PROTOCOL;
            }
            else
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return MOSQ_ERR_SUCCESS;
                }
                else
                {
                    switch(errno)
                    {
                        case ECONNRESET:
                            return MOSQ_ERR_CONN_LOST;
                        default:
                            return MOSQ_ERR_ERRNO;
                    }
                }
            }
        }while ((byte & 128) != 0);
        
        /* If paylod is not null. Ready to read paylod */
        if (mosq->in_packet.remaining_length > 0)
        {
            mosq->in_packet.payload = _mosquitto_malloc(mosq->in_packet.remaining_length * sizeof(uint8_t));
            if (!mosq->in_packet.payload) return MOSQ_ERR_NOMEM;
            mosq->in_packet.to_process = mosq->in_packet.remaining_length;
        }
        mosq->in_packet.have_remaining = 1;
    }
    
    while (mosq->in_packet.to_process)
    {
        read_length = _mosquitto_net_read(mosq, &(mosq->in_packet.payload[mosq->in_packet.pos]), mosq->in_packet.to_process);
        if (read_length > 0)
        {
            mosq->in_packet.pos += read_length;
            mosq->in_packet.to_process -= read_length;
        }
        else
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return MOSQ_ERR_SUCCESS;
            }
            else
            {
                switch(errno)
                {
                    case ECONNRESET:
                        return MOSQ_ERR_CONN_LOST;
                    default:
                        return MOSQ_ERR_ERRNO;
                }
            }
        }
    }
    
    /* All data for this packet is read. */
    mosq->in_packet.pos = 0;
    
    rc = _mosquitto_packet_handle(mosq);
    
    /* Free data and reset values */
    _mosquitto_packet_cleanup(&mosq->in_packet);
    
    mosq->last_msg_in = mosquitto_time();
    return rc;
}

int mosquitto_loop_read(struct mosquitto *mosq,int max_packets)
{
    LOG_INFO("mosquitto_loop_read");
    if (max_packets < 1) return MOSQ_ERR_INVAL;
    
    int rc = 0;
    max_packets = mosq->queue_len > 1 ? mosq->queue_len :1;
    
    for (int i = 0; i < max_packets; i++)
    {
        rc = _mosquitto_packet_read(mosq);
        if (rc)
        {
            return _mosquitto_loop_rc_handle(mosq, rc);
        }
    }
    return rc;
}

int mosquitto_loop_write(struct mosquitto *mosq, int max_packets)
{
    LOG_INFO("mosquitto_loop_write");
    
    int rc = 0;
    int i;
    if(max_packets < 1) return MOSQ_ERR_INVAL;
    
    max_packets = mosq->queue_len>1?mosq->queue_len:1;
    
    for (i = 0; i<max_packets; i++)
    {
        rc = _mosquitto_packet_write(mosq);
        if(rc || errno == EAGAIN || errno == EWOULDBLOCK){
            return _mosquitto_loop_rc_handle(mosq, rc);
        }
    }
    return rc;
}
int mosquitto_set_username_pwd(struct mosquitto *mosq, const char *username, const char *password)
{
    if (!mosq) return MOSQ_ERR_INVAL;
    
    /* Free last data */
    if (mosq->username)
    {
        _mosquitto_free(mosq->username);
        mosq->username = NULL;
    }
    if (mosq->password)
    {
        _mosquitto_free(mosq->password);
        mosq->password = NULL;
    }
    
    /* Set new data */
    if (username)
    {
        mosq->username = _mosquitto_strdup(username);
        if (!mosq->username) return MOSQ_ERR_NOMEM;
        if (password)
        {
            mosq->password = _mosquitto_strdup(password);
            if (!mosq->password)
            {
                _mosquitto_free(mosq->username);
                mosq->username = NULL;
                return MOSQ_ERR_NOMEM;
            }
        }
    }
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_loop_misc(struct mosquitto *mosq)
{
    LOG_INFO("mosquitto_loop_misc");
    
    if (!mosq) return MOSQ_ERR_INVAL;
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
    
    time_t now;
    now = mosquitto_time();
    
    _mosquitto_check_keepalive(mosq);
    if (mosq->last_retry_check + 1 < now)
    {
        _mosquitto_messages_retry_check(mosq);
        mosq->last_retry_check = now;
    }
    
    /*
     mosq->ping_t不为0表示正在等待PINGRESQ，now - mosq->ping_t >= mosq->keepalive表示等待时间已经超过我们设置的最大间隔时间
     所以此时客户端断开连接
     */
    int rc;
    if (mosq->ping_t && now - mosq->ping_t >= mosq->keepalive)
    {

        _mosquitto_socket_close(mosq);
        if (mosq->state == mosq_cs_disconnecting)
        {
            rc = MOSQ_ERR_SUCCESS;
        }
        else
        {
            rc = MOSQ_ERR_NOMEM;
        }
        
        if (mosq->on_disconnect)
        {
            mosq->in_callback = true;
            mosq->on_disconnect(mosq,mosq->userdata,rc);
            mosq->in_callback = false;
        }

        return MOSQ_ERR_CONN_LOST;
    }
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_loop(struct mosquitto *mosq, int timeout, int max_packets)
{
    LOG_INFO("mosquitto_loop");
    if (!mosq || max_packets < 1) return MOSQ_ERR_INVAL;
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
        
    fd_set readfds, writefds;
    int rc;
    
    __DARWIN_FD_ZERO(&readfds);
    __DARWIN_FD_SET(mosq->sock, &readfds);
    __DARWIN_FD_ZERO(&writefds);
    /* 只有当前有未写入的数据才去检查它的是否可写入，否则不检查 */
    if (mosq->out_packet || mosq->current_out_packet)
    {
        __DARWIN_FD_SET(mosq->sock, &writefds);
#ifdef WITH_TLS
    }
    else if (mosq->ssl && mosq->want_write)
    {
        __DARWIN_FD_SET(mosq->sock, &writefds);
#endif
    }

    struct timespec local_timeout;
    if (timeout >= 0)
    {
        local_timeout.tv_sec = timeout/1000;
        local_timeout.tv_nsec = (timeout-local_timeout.tv_sec*1000)*1e6;
    }
    else
    {
        local_timeout.tv_sec = 1;
        local_timeout.tv_nsec = 0;
    }
    
    int fdcount;
    
    fdcount = pselect(mosq->sock+1, &readfds, &writefds, NULL, &local_timeout, NULL);
    if (fdcount == -1)
    {
        if (errno == EINTR)
        {
            return MOSQ_ERR_SUCCESS;
        }
        else
        {
            return MOSQ_ERR_ERRNO;
        }
    }
    else
    {
        LOG_INFO("fdcount IS %d",fdcount);
        if (__DARWIN_FD_ISSET(mosq->sock, &readfds))
        {
            rc = mosquitto_loop_read(mosq, max_packets);
            if (rc || mosq->sock == INVALID_SOCKET)
            {
                return rc;
            }
        }
        if(FD_ISSET(mosq->sock, &writefds))
        {
            rc = mosquitto_loop_write(mosq, max_packets);
            if(rc || mosq->sock == INVALID_SOCKET)
            {
                return rc;
            }
        }
    }

    return mosquitto_loop_misc(mosq);
}

static int _mosquitto_loop_rc_handle(struct mosquitto *mosq, int rc)
{
    if(rc)
    {
        _mosquitto_socket_close(mosq);
        if(mosq->state == mosq_cs_disconnecting)
        {
            rc = MOSQ_ERR_SUCCESS;
        }
        if(mosq->on_disconnect)
        {
            mosq->in_callback = true;
            mosq->on_disconnect(mosq, mosq->userdata, rc);
            mosq->in_callback = false;
        }
        return rc;
    }
    return rc;
}

int mosquitto_loop_forever(struct mosquitto *mosq, int timeout, int max_packets)
{
    LOG_INFO("mosquitto_loop_forever");
    int run = 1;
    int rc = 0;
    
    if (!mosq) return MOSQ_ERR_INVAL;
    
//    if (mosq->state == mosq_cs_connect_async)
//    {
//        mosquitto_reconnect(mosq);
//    }
  
    unsigned int reconnect_delay;
    unsigned int reconnects = 0;
    
    /*
     进入loop 调用select检查socket状态
     当返回不是MOSQ_ERR_SUCCESS的时候检测当前socket状态，从而判断是否重连
     断连则退出loop
     否则计算重连间隔
     再次检测socket状态是否断连
     重新进入loop
     */
    while (run)
    {
        do
        {
            rc = mosquitto_loop(mosq, timeout, max_packets);
        }while (rc == MOSQ_ERR_SUCCESS);
        
        if (errno == EPROTO)
        {
            LOG_WARNING("QUIT mosquitto_loop_forever");
            return rc;
        }
        
        if (mosq->state == mosq_cs_disconnecting)
        {
            LOG_WARNING("DISCONNECTING");
            run = 0;
        }
        else
        {
            if (mosq->reconnect_delay > 0 && mosq->reconnect_exponential_backoff)
            {
                reconnect_delay = mosq->reconnect_delay * reconnects * reconnects;
            }
            else
            {
                reconnect_delay = mosq->reconnect_delay;
            }
            
            if (reconnect_delay > mosq->reconnect_delay_max)
            {
                reconnect_delay = mosq->reconnect_delay_max;
            }
            else
            {
                reconnects++;
            }
            
            sleep(reconnect_delay);
            
            if(mosq->state == mosq_cs_disconnecting){
                run = 0;
            }
            else
            {
                LOG_WARNING("DISCONNECTING");
                mosquitto_reconnect(mosq);
            }
        }
    }
    
    return rc;
}

int mosquitto_disconnect(struct mosquitto *mosq)
{
    if (!mosq) return MOSQ_ERR_INVAL;
        
    mosq->state = mosq_cs_disconnecting;
    
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
        
    return client_send_disconnect_command_mosq(mosq);
}

void _mosquitto_destroy(struct mosquitto *mosq)
{
    if (!mosq) return;
    
#ifdef WITH_THREADING
    if(!pthread_equal(mosq->thread_id, pthread_self())){
        pthread_cancel(mosq->thread_id);
        pthread_join(mosq->thread_id, NULL);
    }
    
    if(mosq->id){
        /*
         如果mosq->id不为空代表客户端已经初始化完成，需要释放这些互斥锁
         如果为空，就不用关心了
         */
        pthread_mutex_destroy(&mosq->callback_mutex);
        pthread_mutex_destroy(&mosq->log_callback_mutex);
        pthread_mutex_destroy(&mosq->state_mutex);
        pthread_mutex_destroy(&mosq->out_packet_mutex);
        pthread_mutex_destroy(&mosq->current_out_packet_mutex);
        pthread_mutex_destroy(&mosq->msgtime_mutex);
        pthread_mutex_destroy(&mosq->message_mutex);
    }
#endif
    
    _mosquitto_socket_close(mosq);
    _mosquitto_message_cleanup_all(mosq);
    _mosquitto_will_clear(mosq);
    
#ifdef WITH_TLS
    if(mosq->ssl){
        SSL_free(mosq->ssl);
    }
    if(mosq->ssl_ctx){
        SSL_CTX_free(mosq->ssl_ctx);
    }
    if(mosq->tls_cafile) _mosquitto_free(mosq->tls_cafile);
    if(mosq->tls_capath) _mosquitto_free(mosq->tls_capath);
    if(mosq->tls_certfile) _mosquitto_free(mosq->tls_certfile);
    if(mosq->tls_keyfile) _mosquitto_free(mosq->tls_keyfile);
    if(mosq->tls_pw_callback) mosq->tls_pw_callback = NULL;
    if(mosq->tls_version) _mosquitto_free(mosq->tls_version);
    if(mosq->tls_ciphers) _mosquitto_free(mosq->tls_ciphers);
    if(mosq->tls_psk) _mosquitto_free(mosq->tls_psk);
    if(mosq->tls_psk_identity) _mosquitto_free(mosq->tls_psk_identity);
#endif
    
    /* Paramter cleanup */
    if (mosq->address)
    {
        _mosquitto_free(mosq->address);
        mosq->address = NULL;
    }
    if(mosq->id)
    {
        _mosquitto_free(mosq->id);
        mosq->id = NULL;
    }
    if(mosq->username)
    {
        _mosquitto_free(mosq->username);
        mosq->username = NULL;
    }
    if(mosq->password)
    {
        _mosquitto_free(mosq->password);
        mosq->password = NULL;
    }
    if(mosq->host)
    {
        _mosquitto_free(mosq->host);
        mosq->host = NULL;
    }
    if(mosq->bind_address)
    {
        _mosquitto_free(mosq->bind_address);
        mosq->bind_address = NULL;
    }
    
    _mosquitto_out_packet_cleanup_all(mosq);
    _mosquitto_packet_cleanup(&mosq->in_packet);
}

void mosquitto_destroy(struct mosquitto *mosq)
{
    if(!mosq) return;
    
    _mosquitto_destroy(mosq);
    _mosquitto_free(mosq);
}

int mosquitto_tls_set(struct mosquitto *mosq, const char *cafile, const char *capath, const char *certfile, const char *keyfile, int (*pw_callback)(char *buf, int size, int rwflag, void *userdata))
{
#ifdef WITH_TLS
    FILE *fptr;
    
    if (!mosq || !(cafile || capath) || (certfile && !keyfile) || (!certfile && keyfile)) return MOSQ_ERR_INVAL;
        
    if (cafile)
    {
        fptr = _mosquitto_fopen(cafile, "rt");
        if (fptr)
        {
            fclose(fptr);
        }
        else
        {
            return MOSQ_ERR_INVAL;
        }
        mosq->tls_cafile = _mosquitto_strdup(cafile);
        if (!mosq->tls_cafile)
        {
            return MOSQ_ERR_NOMEM;
        }
    }
    else if (mosq->tls_cafile)
    {
        _mosquitto_free(mosq->tls_cafile);
        mosq->tls_cafile = NULL;
    }
    
    if (certfile)
    {
        fptr = _mosquitto_fopen(certfile, "rt");
        if (fptr)
        {
            fclose(fptr);
        }
        else
        {
            if (mosq->tls_cafile)
            {
                _mosquitto_free(mosq->tls_cafile);
                mosq->tls_cafile = NULL;
            }
            if (mosq->tls_capath)
            {
                _mosquitto_free(mosq->tls_capath);
                mosq->tls_capath = NULL;
            }
            return MOSQ_ERR_INVAL;
        }
        mosq->tls_certfile = _mosquitto_strdup(certfile);
        if(!mosq->tls_certfile)
        {
            return MOSQ_ERR_NOMEM;
        }
    }
    else
    {
        if(mosq->tls_certfile) _mosquitto_free(mosq->tls_certfile);
        mosq->tls_certfile = NULL;
    }
    
    if(keyfile)
    {
        fptr = _mosquitto_fopen(keyfile, "rt");
        if(fptr)
        {
            fclose(fptr);
        }
        else
        {
            if(mosq->tls_cafile)
            {
                _mosquitto_free(mosq->tls_cafile);
                mosq->tls_cafile = NULL;
            }
            if(mosq->tls_capath)
            {
                _mosquitto_free(mosq->tls_capath);
                mosq->tls_capath = NULL;
            }
            if(mosq->tls_certfile)
            {
                _mosquitto_free(mosq->tls_certfile);
                mosq->tls_certfile = NULL;
            }
            return MOSQ_ERR_INVAL;
        }
        mosq->tls_keyfile = _mosquitto_strdup(keyfile);
        if(!mosq->tls_keyfile)
        {
            return MOSQ_ERR_NOMEM;
        }
    }
    else
    {
        if(mosq->tls_keyfile) _mosquitto_free(mosq->tls_keyfile);
        mosq->tls_keyfile = NULL;
    }
    
    mosq->tls_pw_callback = pw_callback;
    
    return MOSQ_ERR_SUCCESS;
#else
    return MOSQ_ERR_NOT_SUPPORTED;
#endif
}
