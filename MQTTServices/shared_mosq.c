//
//  shared_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/5/5.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "shared_mosq.h"

/* For DISCONNECT, PINGREQ and PINGRESP */
int _mosq_send_simple_command(struct mosquitto *mosq, uint8_t command)
{
    struct _mosquitto_packet *packet = NULL;
    
    assert(mosq);
    packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
    if (!packet) return MOSQ_ERR_NOMEM;
    
    packet->command = command;
    packet->remaining_length = 0;
    
    int rc;
    rc = _mosquitto_packet_alloc(packet);
    if (rc)
    {
        _mosquitto_free(packet);
        return rc;
    }
    
    return _mosquitto_packet_queue(mosq, packet);
}

int _mosquitto_send_real_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
    assert(mosq);
    assert(topic);
    
    struct _mosquitto_packet *packet = NULL;
    int packetlen;
    int rc;
    
    packetlen = 2 + (int)strlen(topic) + payloadlen;
    if (qos > 0) packetlen += 2;
    packet = _mosquitto_calloc(1, sizeof(struct mosquitto));
    if (!packet) return MOSQ_ERR_NOMEM;
    
    packet->mid = mid;
    packet->command = PUBLISH | ((dup&0x1)<<3) | (qos<<1) | retain;
    packet->remaining_length = packetlen;
    rc = _mosquitto_packet_alloc(packet);
    if (rc) return rc;
    
    /* Variable header (topic string) */
    _mosq_write_string(packet, topic, strlen(topic));
    if (qos > 0) _mosq_write_uint16(packet, mid);
    
    /* Paylod */
    if (payloadlen) _mosquitto_write_bytes(packet, payload, payloadlen);
    
    return _mosquitto_packet_queue(mosq, packet);
}

/* For PUBACK, PUBCOMP, PUBREC, and PUBREL */
int _mosquitto_send_command_with_mid(struct mosquitto *mosq, uint8_t command, uint16_t mid, bool dup)
{
    assert(mosq);
    
    struct _mosquitto_packet *packet = NULL;
    int rc;
    
    packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
    if (!packet) return MOSQ_ERR_NOMEM;
    
    packet->command = command;
    if (dup) packet->command |= 8;
    packet->remaining_length = 2;
    rc = _mosquitto_packet_alloc(packet);
    if (rc) return rc;
    
    packet->payload[packet->pos + 0] = MOSQ_MSB(mid);
    packet->payload[packet->pos + 1] = MOSQ_LSB(mid);
    
    return _mosquitto_packet_queue(mosq, packet);
}

int _mosq_send_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
    assert(mosq);
    assert(topic);
    
    if (mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
    
    LOG_INFO("Client %s sending PUBLISH (dup->%d, qos->%d, retain->%d, mid->%d, %s, ...(%ld bytes))", mosq->id, dup, qos, retain, mid, topic, (long)payloadlen);
    
    return _mosquitto_send_real_publish(mosq, mid, topic, payloadlen, payload, qos, retain, dup);
}

int _mosquitto_send_puback(struct mosquitto *mosq, uint16_t mid)
{
    LOG_INFO("Client %s send PUBACK (Mid: %d)",mosq->id, mid);
    
    return _mosquitto_send_command_with_mid(mosq, PUBACK, mid, false);
}

int _mosquitto_send_pubrec(struct mosquitto *mosq, uint16_t mid)
{
    LOG_INFO("Client %s sending PUBREC (Mid: %d)", mosq->id, mid);
    
    return _mosquitto_send_command_with_mid(mosq, PUBREC, mid, false);
}

int _mosquitto_send_pubrel(struct mosquitto *mosq, uint16_t mid, bool dup)
{
    LOG_INFO("Client %s sending PUBREL (Mid: %d)", mosq->id, mid);
    
    return _mosquitto_send_command_with_mid(mosq, PUBREL|2, mid, dup);
}

int _mosquitto_send_pubcomp(struct mosquitto *mosq, uint16_t mid)
{
    LOG_INFO("Client %s sending PUBCOMP (Mid: %d)", mosq->id, mid);

    return _mosquitto_send_command_with_mid(mosq, PUBCOMP, mid, false);
}
