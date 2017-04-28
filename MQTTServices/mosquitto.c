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

static int _mosquitto_reconnect(struct mosquitto *mosq, bool blocking);

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




int mosquitto_reconnect(struct mosquitto *mosq)
{
    return _mosquitto_reconnect(mosq, true);
}

static int _mosquitto_reconnect(struct mosquitto *mosq, bool blocking)
{
    int rc;
    struct _mosquitto_packet *packet = NULL;
    
    //check paramter
    if (!mosq || !mosq->host || mosq->port <=0) return MOSQ_ERR_INVAL;
    
    mosq->state = mosq_cs_new;
    
    _mosquitto_packet_cleanup(&mosq->in_packet);
    
    if (mosq->out_packet && !mosq->current_out_packet)
    {
        mosq->current_out_packet = mosq->out_packet;
        mosq->out_packet = mosq->out_packet->next;
        if (!mosq->out_packet)
        {
            mosq->out_packet_last = NULL;
        }
    }
    
    while (mosq->current_out_packet)
    {
        packet = mosq->current_out_packet;
        /* Free data and reset values */
        if (mosq->out_packet)
        {
            mosq->out_packet = mosq->out_packet->next;
        }
        else
        {
            mosq->out_packet_last = NULL;
        }
        
        _mosquitto_packet_cleanup(packet);
        _mosquitto_free(packet);
        
        mosq->current_out_packet = mosq->out_packet;
    }
    rc = _mosquitto_socket_connect(mosq, mosq->host, mosq->port, mosq->bind_address, blocking);
    if (rc)return rc;
    
    return client_send_connect_command_mosq(mosq, mosq->keepalive, mosq->clean_session);
}
