//
//  net_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "net_mosq.h"

#ifdef WITH_TLS
#include <openssl/err.h>
#include <tls_mosq.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "logger.h"
#include "tls_mosq.h"

#ifdef WITH_TLS
int tls_ex_index_mosq = -1;
#endif

void _mosquitto_net_init(void)
{
#ifdef WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
    
#ifdef WITH_TLS
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    if(tls_ex_index_mosq == -1){
        tls_ex_index_mosq = SSL_get_ex_new_index(0, "client context", NULL, NULL, NULL);
    }
#endif
}

void _mosquitto_net_cleanup(void)
{
#ifdef WITH_TLS
    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
#endif
    
#ifdef WIN32
    WSACleanup();
#endif
}


int _mosquitto_try_connect(const char *host, uint16_t port, int *sock, const char *bind_address, bool blocking)
{
    struct addrinfo hints;
    struct addrinfo *ainfo, *rp;
    struct addrinfo *ainfo_bind = NULL, *rp_bind;
    
    int s;
    int rc;
    int opt;
    
    *sock = INVALID_SOCKET;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_socktype = SOCK_STREAM;
    
    s = getaddrinfo(host, NULL, &hints, &ainfo);
    if (s)
    {
        errno = s;
        return MOSQ_ERR_EAI;
    }
    
    if (bind_address)
    {
        s = getaddrinfo(bind_address, NULL, &hints, &ainfo_bind);
        if (s)
        {
            errno = s;
            return MOSQ_ERR_EAI;
        }
    }
    
    s = socket(AF_INET, SOCK_STREAM, 0);
    
    for (rp = ainfo; rp != NULL;  rp = rp->ai_next)
    {
        *sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        
        //create a vaild socket
        if (*sock == INVALID_SOCKET) continue;
        
        if (rp->ai_family == AF_INET)
        {
            ((struct sockaddr_in *)rp->ai_addr)->sin_port = htons(port);
        }
        else if (rp->ai_family == AF_INET6)
        {
            ((struct sockaddr_in6 *)rp->ai_addr)->sin6_port = htons(port);
        }
        else
        {
            continue;
        }
        
        //check for binding
        if (bind_address)
        {
            for (rp_bind = ainfo_bind; rp_bind != NULL; rp_bind = rp_bind->ai_next)
            {
                if (bind(*sock, rp_bind->ai_addr, rp_bind->ai_addrlen) == 0)
                {
                    //found one, next
                    break;
                }
            }
            //no one can bind. close this socket
            if (!rp_bind)
            {
                close(*sock);
                continue;
            }
        }
        
        //connect
        rc = connect(*sock, rp->ai_addr, rp->ai_addrlen);
        
        //Set non-blocking
        opt = fcntl(*sock, F_GETFL, 0);
        if (blocking)
        {
            if(opt == -1 || fcntl(*sock, F_SETFL, opt | O_NONBLOCK) == -1)
            {
                //error, close this socket
                close(*sock);
                continue;
            }
            if (rc == 0 || errno == EINPROGRESS || errno == EWOULDBLOCK)
            {
                //success
                break;
            }
        }
        else
        {
            if (opt == -1 || fcntl(*sock, F_SETFL, opt | O_NONBLOCK) == -1)
            {
                //error, close this socket
                close(*sock);
                continue;
            }
            if (rc == 0 || errno == EINPROGRESS || errno == EWOULDBLOCK)
            {
                //success
                break;
            }
        }

        //no one success,(rc == 0 || errno == EINPROGRESS || errno == EWOULDBLOCK) is False
        close(*sock);
        *sock = INVALID_SOCKET;
    }
    
    //free
    freeaddrinfo(ainfo);
    if (bind_address)
    {
        freeaddrinfo(ainfo_bind);
    }
    
    //if no one success,rp is NULL
    if (!rp)
    {
        return MOSQ_ERR_ERRNO;
    }
    return MOSQ_ERR_SUCCESS;
}

int _mosquitto_socket_connect(struct mosquitto* mosq,const char * host,uint16_t port,const char * bind_address,bool blocking)
{
    int sock = INVALID_SOCKET;
    int rc;
#ifdef WITH_TLS
    int ret;
    BIO *bio;
#endif
    //check paramter
    if (!mosq || !host || !port) return MOSQ_ERR_INVAL;
#ifdef WITH_TLS
    if (mosq->tls_cafile || mosq->tls_capath || mosq->tls_psk)
    {
        blocking = true;
    }
#endif
    //create socket,we will return error code if fail otherwise go on
    rc = _mosquitto_try_connect(host, port, &sock, bind_address, blocking);
    if (rc != MOSQ_ERR_SUCCESS) return  rc;
    
    /*
        We will support SSL/TLS here.
     */
#ifdef WITH_TLS
    if (mosq->tls_cafile || mosq->tls_capath || mosq->tls_psk)
    {
#if OPENSSL_VERSION_NUMBER >= 0x10001000L
        /* 根据version设置本次会话的协议 并申请会话环境*/
        if (!mosq->tls_version || !strcmp(mosq->tls_version, "tlsv1.2"))
        {
            mosq->ssl_ctx = SSL_CTX_new(TLSv1_2_client_method());
        }
        else if (!strcmp(mosq->tls_version, "tlsv1.1"))
        {
            mosq->ssl_ctx = SSL_CTX_new(TLSv1_1_client_method());
        }
        else if(!strcmp(mosq->tls_version, "tlsv1"))
        {
            mosq->ssl_ctx = SSL_CTX_new(TLSv1_client_method());
        }
        else
        {
            LOG_INFO("Error: Protocol %s not supported.", mosq->tls_version);
            close(sock);
            return MOSQ_ERR_INVAL;
        }
#else
        if(!mosq->tls_version || !strcmp(mosq->tls_version, "tlsv1"))
        {
            mosq->ssl_ctx = SSL_CTX_new(TLSv1_client_method());
        }
        else
        {
            LOG_INFO("Error: Protocol %s not supported.", mosq->tls_version);
            close(sock);
            return MOSQ_ERR_INVAL;
        }
#endif
        if(!mosq->ssl_ctx)
        {
            LOG_INFO("Error: Unable to create TLS context.");
            close(sock);
            return MOSQ_ERR_TLS;
        }
        
#if OPENSSL_VERSION_NUMBER >= 0x10000000
        /*
         设置ssl_ctx的选项（位掩码）  注意：之前设置的参数不会被清除
         adds the options set via bitmask in options to ctx. Options already set before are not cleared!
         
         源文件有相关选项的注释。当前选项是即使支持压缩也不使用
         */
        SSL_CTX_set_options(mosq->ssl_ctx, SSL_OP_NO_COMPRESSION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
        /*
         设置ssl_ctx的模式 与上类似
         Use even less memory per SSL connection. 
         */
        SSL_CTX_set_mode(mosq->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
        /* 设置加密方式 */
        if (mosq->tls_ciphers)
        {
            /*
             使用control string(str)来为ssl_ctx设置可用的密码列表
             sets the list of available ciphers for ctx using the control string str. The format of the string is described in ciphers. The list of ciphers is inherited by all ssl objects created from ctx.
             
             return 1 if any cipher could be selected and 0 on complete failure.
             */
            ret = SSL_CTX_set_cipher_list(mosq->ssl_ctx, mosq->tls_ciphers);
            if (ret == 0)
            {
                LOG_INFO("Error: Unable to set TLS ciphers. Check cipher list \"%s\".", mosq->tls_ciphers);
                close(sock);
                return MOSQ_ERR_TLS;
            }
        }
        
        if (mosq->tls_cafile || mosq->tls_capath)
        {
            /* 
             加载CA证书 
             specifies the locations for ctx, at which CA certificates for verification purposes are located. The certificates available via CAfile and CApath are trusted.
             */
            ret = SSL_CTX_load_verify_locations(mosq->ssl_ctx, mosq->tls_cafile, mosq->tls_capath);
            if (ret == 0)
            {
                if(mosq->tls_cafile && mosq->tls_capath)
                {
                    LOG_INFO("Error: Unable to load CA certificates, check cafile \"%s\" and capath \"%s\".", mosq->tls_cafile, mosq->tls_capath);
                }
                else if(mosq->tls_cafile)
                {
                    LOG_INFO("Error: Unable to load CA certificates, check cafile \"%s\".", mosq->tls_cafile);
                }
                else
                {
                    LOG_INFO("Error: Unable to load CA certificates, check capath \"%s\".", mosq->tls_capath);
                }
                close(sock);
                return MOSQ_ERR_TLS;
            }
            /* 设置握手方式 */
            if (mosq->tls_cert_reqs == 0)
            {
                SSL_CTX_set_verify(mosq->ssl_ctx, SSL_VERIFY_NONE, NULL);
            }
            else
            {
                SSL_CTX_set_verify(mosq->ssl_ctx, SSL_VERIFY_PEER, _mosquitto_server_certificate_verify);
            }
            
            if (mosq->tls_pw_callback)
            {
                SSL_CTX_set_default_passwd_cb(mosq->ssl_ctx, mosq->tls_pw_callback);
                SSL_CTX_set_default_passwd_cb_userdata(mosq->ssl_ctx, mosq);
            }
            
            if (mosq->tls_certfile)
            {
                /* 
                 加载客户端证书 
                 adds the first certificate found in the file to the certificate store. The other certificates are added to the store of chain certificates using 
                 */
                ret = SSL_CTX_use_certificate_chain_file(mosq->ssl_ctx, mosq->tls_certfile);
                if (ret != 1)
                {
                    LOG_INFO("Error: Unable to load client certificate \"%s\".", mosq->tls_certfile);
                    close(sock);
                    return MOSQ_ERR_TLS;
                }
            }
            if (mosq->tls_keyfile)
            {
                /* 加载客户端的私钥 */
                ret = SSL_CTX_use_PrivateKey_file(mosq->ssl_ctx, mosq->tls_keyfile, SSL_FILETYPE_PEM);
                if (ret != 1)
                {
                    LOG_INFO("Error: Unable to load client key file \"%s\".", mosq->tls_keyfile);
                    close(sock);
                    return MOSQ_ERR_TLS;
                }
                /*
                 检查私钥是否和证书匹配 
                 checks the consistency of a private key with the corresponding certificate loaded into ctx. If more than one key/certificate pair (RSA/DSA) is installed, the last item installed will be checked. If e.g. the last item       was a RSA certificate or key, the RSA key/certificate pair will be checked. SSL_check_private_key() performs the same check for ssl. If no key/certificate was explicitly added for this ssl, the last item added into ctx will be checked.
                 */
                ret = SSL_CTX_check_private_key(mosq->ssl_ctx);
                if (ret != 1)
                {
                    LOG_INFO("Error: Client certificate/key are inconsistent.");
                    close(sock);
                    return MOSQ_ERR_TLS;
                }
            }
        }
        /* 绑定套接字 */
        mosq->ssl = SSL_new(mosq->ssl_ctx);
        if (!mosq->ssl)
        {
            close(sock);
            return MOSQ_ERR_TLS;
        }
        /*  
         绑定额外数据在ssl对象上
         Several OpenSSL structures can have application specific data attached to them. These functions are used internally by OpenSSL to manipulate application specific data attached to a specific structure.
         */
        SSL_set_ex_data(mosq->ssl, tls_ex_index_mosq, mosq);
        bio = BIO_new_socket(sock, BIO_NOCLOSE);
        if (!bio)
        {
            close(sock);
            return MOSQ_ERR_TLS;
        }
        /*
         ssl对象连接一个BIO来处理读写操作
         SSL_set_bio() connects the BIOs rbio and wbio for the read and write operations of the TLS/SSL (encrypted) side of ssl.
         
         The SSL engine inherits the behaviour of rbio and wbio, respectively. If a BIO is non-blocking, the ssl will also have non-blocking behaviour.
         
         If there was already a BIO connected to ssl, BIO_free() will be called (for both the reading and writing side, if different).
         
         cannot fail
         */
        SSL_set_bio(mosq->ssl, bio, bio);
        
        /* SSL握手 */
        ret = SSL_connect(mosq->ssl);
        if (ret != 1)
        {
            ret = SSL_get_error(mosq->ssl, ret);
            switch (ret)
            {
                case SSL_ERROR_WANT_READ:
                {
                    /* We always try to read anyway */
                }
                    break;
                case SSL_ERROR_WANT_WRITE:
                {
                    mosq->want_write = true;
                }
                    break;
                default:
                    close(sock);
                    return MOSQ_ERR_TLS;
            }
        }
    }
#endif
    
    mosq->sock = sock;
    
    return MOSQ_ERR_SUCCESS;
}

int _mosquitto_packet_write(struct mosquitto *mosq)
{
    LOG_INFO("_mosquitto_packet_write");
    struct _mosquitto_packet *packet;
    
    if (!mosq) return MOSQ_ERR_INVAL;
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
    
    /*
     当前发送包为空且发送队列不为空
     */
    if (mosq->out_packet && !mosq->current_out_packet)
    {
        mosq->current_out_packet = mosq->out_packet;
        mosq->out_packet = mosq->out_packet->next;
        if (!mosq->out_packet)
        {
            mosq->out_packet_last = NULL;
        }
    }
    
    ssize_t write_length;
    while (mosq->current_out_packet)
    {
        packet = mosq->current_out_packet;
        packet->pos = 0;
        packet->to_process = packet->packet_length;
        while (packet->to_process > 0)
        {
            write_length =  _mosquitto_net_write(mosq, &(packet->payload[packet->pos]), packet->to_process);
            LOG_INFO("write_length is %zd",write_length);
            if (write_length > 0)
            {
                packet->pos += write_length;
                packet->to_process -= write_length;
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
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
        
        /* Free data, reset values */
        mosq->current_out_packet = mosq->out_packet;
        if (mosq->out_packet)
        {
            mosq->out_packet = mosq->out_packet->next;
            if (!mosq->out_packet)
            {
                mosq->out_packet_last = NULL;
            }
        }
        
        _mosquitto_packet_cleanup(packet);
        _mosquitto_free(packet);
        mosq->last_msg_out = mosquitto_time();
    }
    
    return MOSQ_ERR_SUCCESS;
}



int _mosquitto_packet_queue(struct mosquitto *mosq, struct _mosquitto_packet *packet)
{
    assert(mosq);
    assert(packet);
    
    LOG_INFO("_mosquitto_packet_queue");
    /* 链表实现了发送队列。设置了两个指针分别指向队列的头尾 */
    packet->next = NULL;
    if (mosq->out_packet)
    {
        mosq->out_packet_last->next = packet;
    }
    else
    {
        mosq->out_packet = packet;
    }
    mosq->out_packet_last = packet;
    
   
    if (mosq->in_callback == false && mosq->threaded == false)
    {
        return _mosquitto_packet_write(mosq);
    }
    else
    {
        return MOSQ_ERR_SUCCESS;
    }
}


int _mosquitto_socket_close(struct mosquitto *mosq)
{
    LOG_INFO("_mosquitto_socket_close");
    int rc = 0;
    assert(mosq);
    /*
     后续添加了SSL/TLS的部分   再实现这一块
     */
#ifdef WITH_TLS
    if (mosq->ssl)
    {
        SSL_shutdown(mosq->ssl);
        SSL_free(mosq->ssl);
        mosq->ssl = NULL;
    }
    
    if (mosq->ssl_ctx)
    {
        SSL_CTX_free(mosq->ssl_ctx);
        mosq->ssl_ctx = NULL;
    }
#endif
    if(mosq->sock != INVALID_SOCKET)
    {
        rc = close(mosq->sock);
        mosq->sock = INVALID_SOCKET;
    }
    return rc;
}

ssize_t _mosquitto_net_read(struct mosquitto *mosq, void *buf, int count)
{
#ifdef WITH_TLS
    int ret;
    int err;
    char ebuf[256];
    unsigned long e;
#endif
    assert(mosq);
    errno = 0;
#ifdef WITH_TLS
    if (mosq->ssl)
    {
        ret = SSL_read(mosq->ssl, buf, count);
        if (ret <= 0)
        {
            err = SSL_get_error(mosq->ssl, ret);

            switch (err)
            {
                case SSL_ERROR_WANT_READ:
                    ret = -1;
                    errno = EAGAIN;
                    break;
                case SSL_ERROR_WANT_WRITE:
                    ret = -1;
                    mosq->want_write = true;
                    errno = EAGAIN;
                    break;
                default:
                    e = ERR_get_error();
                    while(e)
                    {
                        LOG_INFO("OpenSSL Error: %s", ERR_error_string(e, ebuf));
                        e = ERR_get_error();
                    }
                    errno = EPROTO;
                    break;
            }
        }
        return (ssize_t)ret;
    }
    else
    {
#endif
        return read(mosq->sock, buf, count);
#ifdef WITH_TLS
    }
#endif
}

ssize_t _mosquitto_net_write(struct mosquitto *mosq, void *buf, int count)
{
#ifdef WITH_TLS
    int ret;
    int err;
    char ebuf[256];
    unsigned long e;
#endif
    assert(mosq);
    errno = 0;
    
#ifdef WITH_TLS
    if (mosq->ssl)
    {
        ret = SSL_write(mosq->ssl, buf, count);
        if (ret < 0)
        {
            err = SSL_get_error(mosq->ssl, ret);
            if(err == SSL_ERROR_WANT_READ){
                ret = -1;
                errno = EAGAIN;
            }else if(err == SSL_ERROR_WANT_WRITE){
                ret = -1;
                mosq->want_write = true;
                errno = EAGAIN;
            }else{
                e = ERR_get_error();
                while(e){
                    LOG_INFO("OpenSSL Error: %s", ERR_error_string(e, ebuf));
                    e = ERR_get_error();
                }
                errno = EPROTO;
            }
        }
        return ret;
    }
    else
    {
#endif
        return write(mosq->sock, buf, count);
#ifdef WITH_TLS
    }
#endif
}
