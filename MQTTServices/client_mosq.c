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
#include "logger.h"


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

int client_receive_connect_ack_mosq(struct mosquitto *mosq)
{
    assert(mosq);
    if (mosq->in_packet.remaining_length != 2) return MOSQ_ERR_PROTOCOL;
    
    LOG_INFO("Client %s received CONNACK",mosq->id);
    int rc;
    uint8_t byte;
    uint8_t result;
    rc = _mosq_read_byte(&mosq->in_packet, &byte); // Reserved byte, unused
    if (rc) return rc;
    rc = _mosq_read_byte(&mosq->in_packet, &result);
    if (rc) return rc;
    if (mosq->on_connect)
    {
        mosq->in_callback = true;
        mosq->on_connect(mosq, mosq->userdata, result);
        mosq->in_callback = false;
    }
    switch (result)
    {
        case 0:
            mosq->state = mosq_cs_connected;
            return MOSQ_ERR_SUCCESS;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            mosq->state = mosq_cs_disconnecting;
            return MOSQ_ERR_CONN_REFUSED;
        default:
            mosq->state = mosq_cs_disconnecting;
            return MOSQ_ERR_PROTOCOL;
    }
}

int client_send_ping_request_mosq(struct mosquitto *mosq)
{
    return _mosquitto_send_pingreq(mosq);
}

int client_receive_ping_response_mosq(struct mosquitto *mosq)
{
    assert(mosq);
    mosq->ping_t = 0;
    LOG_INFO("Client %s received PINGRESQ",mosq->id);
    return MOSQ_ERR_SUCCESS;
}

int client_send_subscribe_mosq(struct mosquitto *mosq, int *mid, const char *topic, uint8_t topic_qos)
{
    assert(mosq);
    
    int rc;
    uint16_t local_mid;
    struct _mosquitto_packet *packet = NULL;
    packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
    
    packet->command = SUBSCRIBE | (1 << 1);
    /* 可变报头 + MSB + LSB + topic.length + 服务质量要求*/
    packet->remaining_length = 2 + 2 + (uint32_t)strlen(topic) + 1;
    rc = _mosquitto_packet_alloc(packet);
    if (rc)
    {
        _mosquitto_free(packet);
        return rc;
    }
    
    /* Variable header */
    local_mid = _mosquitto_mid_generate(mosq);
    if (mid) *mid = (int)local_mid;
    _mosq_write_uint16(packet, local_mid);
    
    /* Paylod */
    _mosq_write_string(packet, topic, strlen(topic));
    _mosq_write_byte(packet, topic_qos);
    
    LOG_INFO("Client %s sending SUBSCRIBE (Mid: %d, Topic: %s, Qos: %d)",mosq->id,local_mid,topic,topic_qos);
    return _mosquitto_packet_queue(mosq, packet);
}

int client_receive_suback_mosq(struct mosquitto *mosq)
{
    LOG_INFO("Client %s received SUBACK",mosq->id);
    
    int rc;
    uint16_t mid;
    int *granted_qos;
    int qos_count;
    
    rc = _mosq_read_uint16(&mosq->in_packet, &mid);
    if (rc) return rc;
        
    qos_count = mosq->in_packet.remaining_length - mosq->in_packet.pos;
    granted_qos = _mosquitto_malloc(qos_count * sizeof(int));
    if (!granted_qos) return MOSQ_ERR_NOMEM;
    
    int i = 0;
    uint8_t qos;
    while (mosq->in_packet.pos < mosq->in_packet.remaining_length)
    {
        rc = _mosq_read_byte(&mosq->in_packet, &qos);
        if (rc)
        {
            _mosquitto_free(granted_qos);
            return rc;
        }
        granted_qos[i] = qos;
        i++;
    }
    
    if (mosq->on_subscribe)
    {
        mosq->in_callback = true;
        mosq->on_subscribe(mosq, mosq->userdata, mid, qos_count, granted_qos);
        mosq->in_callback = false;
    }
    
    _mosquitto_free(granted_qos);
    
    return MOSQ_ERR_SUCCESS;
}

int client_send_unsubscribe_mosq(struct mosquitto *mosq, int *mid, const char *topic)
{
    assert(mosq);
    
    uint16_t local_mid;
    int rc;
    
    struct _mosquitto_packet *packet = NULL;
    packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
    
    packet->command = UNSUBSCRIBE;
    /* 可变报头 + MSB + LSB +topic */
    packet->remaining_length = 2 + 2 + (int)strlen(topic);
    rc = _mosquitto_packet_alloc(packet);
    if (rc) return MOSQ_ERR_NOMEM;
    
    /* Variable header */
    local_mid = _mosquitto_mid_generate(mosq);
    if (mid) *mid = (int)local_mid;
    _mosq_write_uint16(packet, local_mid);
    
    /* Paylod */
    _mosq_write_string(packet, topic, strlen(topic));
    
    LOG_INFO("Client %s sending UNSUBSCRIBE (Mid: %d, Topic: %s)",mosq->id,local_mid,topic);
    return _mosquitto_packet_queue(mosq, packet);
}

int client_receive_unsubscribe_mmosq(struct mosquitto *mosq)
{
    assert(mosq);
    
    LOG_INFO("Client %s Receive UNSUBACK",mosq->id);
    
    uint16_t mid;
    int rc;
    
    rc = _mosq_read_uint16(&mosq->in_packet, &mid);
    if (rc) return rc;
    
    if (mosq->on_unsubscribe)
    {
        mosq->in_callback = true;
        mosq->on_unsubscribe(mosq, mosq->userdata, mid);
        mosq->in_callback = false;
    }
    
    return MOSQ_ERR_SUCCESS;
}

int client_send_disconnect_command_mosq(struct mosquitto *mosq)
{
    assert(mosq);
    
    LOG_WARNING("Client %s sending DISCONNECT",mosq->id);
    
    mosq->state = mosq_cs_disconnecting;
    
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
    
    return _mosq_send_simple_command(mosq, DISCONNECT);
}

