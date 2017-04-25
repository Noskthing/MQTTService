//
//  memory_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "memory_mosq.h"

void *_mosquitto_calloc(size_t nmemb, size_t size)
{
    void *mem = calloc(nmemb, size);
    
    return mem;
}

void *_mosquitto_malloc(size_t size)
{
    void *mem = malloc(size);
    
    return mem;
}

int _mosquitto_packet_alloc(struct _mosquitto_packet *packet)
{
    uint8_t remaining_bytes[5],bytes;
    uint32_t remaining_length;
    
    if (!packet) return MOSQ_ERR_NOMEM;
    
    remaining_length = packet->remaining_length;
    packet->payload = NULL;
    packet->remaining_count = 0;
    
    while (remaining_length > 0 && packet->remaining_count <= 5)
    {
        bytes = remaining_length % 128;
        remaining_length = remaining_length / 128;
        
        if (remaining_length > 0)
        {
            bytes = bytes | 0x80;
        }
        remaining_bytes[packet->remaining_count] = bytes;
        packet->remaining_count++;
    }
    
    if (packet->remaining_count == 5) return MOSQ_ERR_PAYLOAD_SIZE;
    
    packet->packet_length = packet->remaining_length + 1 + packet->remaining_count;
    packet->payload = _mosquitto_malloc(sizeof(uint8_t)*packet->packet_length);
    if (packet->payload) return MOSQ_ERR_NOMEM;
    
    packet->payload[0] = packet->command;
    for (int i = 0; i < packet->remaining_count; i++)
    {
        packet->payload[i + 1] = remaining_bytes[i];
    }
    packet->pos = 1 + packet->remaining_count;
        
    return MOSQ_ERR_SUCCESS;
}

void _mosquitto_packet_cleanup(struct _mosquitto_packet *packet)
{
    if (!packet) return;
    
    //Free data and reset values
    packet->command = 0;
    packet->have_remaining = 0;
    packet->remaining_count = 0;
    packet->remaining_mult = 1;
    packet->remaining_length = 0;
    if (packet->payload)
    {
        free(packet->payload);
    }
    packet->payload = NULL;
    packet->to_process = 0;
    packet->pos = 0;
}

void _mosquitto_free(void *mem)
{
    free(mem);
}
