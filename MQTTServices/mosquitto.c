//
//  mosquitto.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "mosquitto.h"
#include "net_mosq.h"

static int _mosquitto_reconnect(struct mosquitto *mosq, bool blocking);

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
    rc = _mosq_socket_connect(mosq, mosq->host, mosq->port, mosq->bind_address, blocking);
    if (rc)return rc;
    
    return _mosq_packet_queue(mosq, packet);
}
