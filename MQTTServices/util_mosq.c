//
//  util_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "util_mosq.h"
#include <assert.h>
#include <string.h>
#include "logger.h"

void _mosquitto_write_byte(struct _mosquitto_packet *packet, uint8_t byte)
{
    assert(packet);
    assert(packet->pos+1 <= packet->packet_length);
    
    packet->payload[packet->pos] = byte;
    packet->pos++;
}

void _mosq_write_uint16(struct _mosquitto_packet *packet, uint16_t word)
{
    _mosquitto_write_byte(packet, MOSQ_MSB(word));
    _mosquitto_write_byte(packet, MOSQ_LSB(word));
}

void _mosquitto_write_bytes(struct _mosquitto_packet *packet, const void *bytes, uint32_t count)
{
    assert(packet);
    assert(packet->pos+count <= packet->packet_length);
    
    memcpy(&(packet->payload[packet->pos]), bytes, count);
    packet->pos += count;
}

void _mosq_write_string(struct _mosquitto_packet *packet, const char * str, uint16_t length)
{
    assert(packet);
    _mosq_write_uint16(packet, length);
    _mosquitto_write_bytes(packet, str, length);
}

void _mosq_write_byte(struct _mosquitto_packet *packet, uint8_t byte)
{
    assert(packet);
    assert(packet->pos + 1 <= packet->packet_length);
    
    packet->payload[packet->pos] = byte;
    packet->pos ++;
}

int _mosq_read_byte(struct _mosquitto_packet *packet, uint8_t *byte)
{
    assert(packet);
    if (packet->pos + 1 > packet->remaining_length) return MOSQ_ERR_PROTOCOL;
    
    *byte = packet->payload[packet->pos];
    packet->pos++;
    
    return MOSQ_ERR_SUCCESS;
}
