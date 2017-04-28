//
//  client_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "client_mosq.h"
#include "memory_mosq.h"
#include "util_mosq.h"
#include "net_mosq.h"

#include <string.h>
#include <stdbool.h>

int client_send_connect_command_mosq(struct mosquitto *mosq, uint16_t keeplive, bool clean_session)
{
    struct _mosquitto_packet *packet = NULL;
    int payload_length;
    
    uint8_t will = 0;
    uint8_t byte;
    
    int rc;
    uint8_t version = PROTOCOL_VERSION;
    
    if (!mosq || !mosq->id) return MOSQ_ERR_INVAL;
    
    /*
     CONNECT's payload contains one or more packet which prefix is MSB-LSB
     
     Client Identifier - Will Topic - Will Message - User Name - Password
     */
    packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
    if (!packet) return MOSQ_ERR_NOMEM;
    /* Client Identifier */
    payload_length = (int)(2 + strlen(mosq->id));
    if (mosq->will)
    {
        will = 1;
        if (!mosq->will->topic) return MOSQ_ERR_NOMEM;
        /* Will Topic - Will Message */
        payload_length += 2 + (int)strlen(mosq->will->topic) + 2 + (int)mosq->will->payloadlen;
    }
    
    if (mosq->username)
    {
        /* User Name */
        payload_length += 2 + (int)strlen(mosq->username);
        if (mosq->password)
        {
            /* Password */
            payload_length += 2 + (int)strlen(mosq->password);
        }
    }
    
    packet->command = CONNECT;
    
    /* Variable header length = 10  MSB-LSB length= 2*/
    packet->remaining_length = 10 + 2 + payload_length;
    
    /* if alloc fail,free resource */
    rc = _mosquitto_packet_alloc(packet);
    if (rc)
    {
        _mosquitto_free(packet);
        return rc;
    }
    
    /* Variable header */
    _mosq_write_string(packet, PROTOCOL_NAME, strlen(PROTOCOL_NAME));
    _mosq_write_byte(packet, version);
    
    //get flags
    byte = (clean_session&0x1) << 1;
    if (will)
    {
        byte = byte | ((mosq->will->retain&0x1)<<5) | ((mosq->will->qos&0x3)<<3) | ((will&0x1)<<2);
    }
    if (mosq->username)
    {
        byte = byte | 0x1<<7;
        if(mosq->password)
        {
            byte = byte | 0x1<<6;
        }
    }
    _mosq_write_byte(packet, byte);
    //keeplive
    _mosq_write_uint16(packet,keeplive);
    
    
    /* Payload */
    _mosq_write_string(packet, mosq->id, strlen(mosq->id));
    if (will)
    {
        _mosq_write_string(packet, mosq->will->topic, strlen(mosq->will->topic));
        _mosq_write_string(packet, (const char *)mosq->will->payload, mosq->will->payloadlen);
    }
    if (mosq->username)
    {
        _mosq_write_string(packet, mosq->username, strlen(mosq->username));
        if (mosq->password)
        {
            _mosq_write_string(packet, mosq->password, strlen(mosq->password));
        }
    }
    
    mosq->keepalive = keeplive;
    
    return _mosquitto_packet_queue(mosq, packet);
}
