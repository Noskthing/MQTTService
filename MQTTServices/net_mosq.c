//
//  net_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "net_mosq.h"


#include <stdlib.h>
#include <string.h>

int _mosq_try_connect(const char *host, uint16_t port, int *sock, const char *bind_address, bool blocking)
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

int _mosq_socket_connect(struct mosquitto* mosq,const char * host,uint16_t port,const char * bind_address,bool blocking)
{
    int sock = INVALID_SOCKET;
    int rc;
    
    //check paramter
    if (!mosq || !host || !port) return MOSQ_ERR_INVAL;
    
    //create socket,we will return error code if fail otherwise go on
    rc = _mosq_try_connect(host, port, &sock, bind_address, blocking);
    if (rc != MOSQ_ERR_SUCCESS) return  rc;
    
    /*
        We will support SSL/TLS here.
     */
    
    mosq->sock = sock;
    
    return MOSQ_ERR_SUCCESS;
}

int _mosq_packet_queue(struct mosquitto *mosq, struct _mosquitto_packet *packet)
{
    assert(mosq);
    assert(packet);
    
    packet->pos = 0;
    packet->to_process = packet->packet_length;
    
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
        return -1;
    }
    else
    {
        return MOSQ_ERR_SUCCESS;
    }
}

