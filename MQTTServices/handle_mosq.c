//
//  handle_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/5/8.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "handle_mosq.h"
#include <assert.h>
#include <string.h>

int _mosquitto_handle_publish(struct mosquitto *mosq)
{
    uint8_t header;
    struct mosquitto_message_all *message;
    int rc = 0;
    uint16_t mid;

    assert(mosq);
    message = _mosquitto_calloc(1, sizeof(struct mosquitto_message_all));
    if (!message) return MOSQ_ERR_NOMEM;
    
    /* 固定报头 */
    header = mosq->in_packet.command;
    
    message->direction = mosq_md_in;
    message->dup = (header&0x08)>>3;
    message->msg.qos = (header&0x06)>>1;
    message->msg.retain = (header & 0x01);
    
    /* 可变报头 主题名*/
    rc = _mosq_read_string(&mosq->in_packet, &message->msg.topic);
    if (rc) {
        _mosquitto_message_cleanup(&message);
        return rc;
    }
    
    if (!strlen(message->msg.topic))
    {
        _mosquitto_message_cleanup(&message);
        return MOSQ_ERR_PROTOCOL;
    }
    
    if (message->msg.qos > 0)
    {
        rc = _mosq_read_uint16(&mosq->in_packet, &mid);
        if (rc)
        {
            _mosquitto_message_cleanup(&message);
            return rc;
        }
        message->msg.mid = (int)mid;
    }
    
    /* Paylod */
    message->msg.payloadlen = mosq->in_packet.remaining_length - mosq->in_packet.pos;
    if (message->msg.payloadlen)
    {
        message->msg.payload = _mosquitto_calloc(message->msg.payloadlen + 1, sizeof(uint8_t));
        if (!message->msg.payload)
        {
            _mosquitto_message_cleanup(&message);
            return MOSQ_ERR_NOMEM;
        }
        rc = _mosq_read_bytes(&mosq->in_packet, message->msg.payload, message->msg.payloadlen);
        if (rc)
        {
            _mosquitto_message_cleanup(&message);
            return rc;
        }
    }
    
    LOG_INFO("Client %s received PUBLISH (dup=%d, qos=%d, retain=%d, mid=%d, '%s', ... (%ld bytes))",
             mosq->id, message->dup, message->msg.qos, message->msg.retain,
             message->msg.mid, message->msg.payload,
             (long)message->msg.topic);
    
    switch (message->msg.qos)
    {
        case 0:
            if (mosq->on_message)
            {
                mosq->in_callback = true;
                mosq->on_message(mosq, mosq->userdata, &message->msg);
                mosq->in_callback = false;
            }
            _mosquitto_message_cleanup(&message);
            return MOSQ_ERR_SUCCESS;
        case 1:
            rc = _mosquitto_send_puback(mosq, message->msg.mid);
            if (mosq->on_message)
            {
                mosq->in_callback = true;
                mosq->on_message(mosq, mosq->userdata, &message->msg);
                mosq->in_callback = false;
            }
            _mosquitto_message_cleanup(&message);
            return rc;
        case 2:
            rc = _mosquitto_send_pubrec(mosq, message->msg.mid);
            message->state = mosq_ms_wait_for_pubrel;
            _mosquitto_message_queue(mosq, message, true);
            return rc;
        default:
            _mosquitto_message_cleanup(&message);
            return MOSQ_ERR_PROTOCOL;
    }
    return 0;
}

int _mosquitto_handle_puback(struct mosquitto *mosq, const char *type)
{
    uint16_t mid;
    int rc;
    
    assert(mosq);
    rc = _mosq_read_uint16(&mosq->in_packet, &mid);
    if (rc) return rc;
    
    LOG_INFO("Client %s received %s (Mid: %d)", mosq->id, type, mid);
    
    if (!_mosquitto_message_delete(mosq, mid, mosq_md_out))
    {
        if (mosq->on_publish)
        {
            mosq->in_callback = true;
            mosq->on_publish(mosq, mosq->userdata, mid);
            mosq->in_callback = false;
        }
    }
    return MOSQ_ERR_SUCCESS;
}

int _mosquitto_handle_pubrec(struct mosquitto *mosq)
{
    assert(mosq);
    
    uint16_t mid;
    int rc;

    rc = _mosq_read_uint16(&mosq->in_packet, &mid);
    if(rc) return rc;
    
    LOG_INFO("Client %s received PUBREC (Mid: %d)", mosq->id, mid);
    
    rc = _mosquitto_message_update(mosq, mid, mosq_md_out, mosq_ms_wait_for_pubcomp);
    if(rc) return rc;
    
    rc = _mosquitto_send_pubrel(mosq, mid, false);
    if(rc) return rc;
    
    return MOSQ_ERR_SUCCESS;
}
