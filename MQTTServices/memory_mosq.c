//
//  memory_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "memory_mosq.h"
#include "net_mosq.h"
#include <assert.h>
#include "will_mosq.h"

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
    
    /*
     Remaining Length 编码伪码
     do
        encodedByte = X MOD 128
        X = X DIV 128
        // if there are more data to encode, set the top bit of this byte
        if ( X > 0 )
            encodedByte = encodedByte OR 128
        endif
        'output' encodedByte
     while ( X > 0 )
     */
    do
    {
        bytes = remaining_length % 128;
        remaining_length = remaining_length / 128;
        
        if (remaining_length > 0)
        {
            bytes = bytes | 0x80;
        }
        remaining_bytes[packet->remaining_count] = bytes;
        packet->remaining_count++;
    }while (remaining_length > 0);
        
    if (packet->remaining_count == 5) return MOSQ_ERR_PAYLOAD_SIZE;
    
    packet->packet_length = packet->remaining_length + 1 + packet->remaining_count;
    packet->payload = _mosquitto_malloc(sizeof(uint8_t)*packet->packet_length);
    if (!packet->payload) return MOSQ_ERR_NOMEM;
    packet->payload[0] = packet->command;
    for (int i = 0; i < packet->remaining_count; i++)
    {
        packet->payload[i + 1] = remaining_bytes[i];
    }
    packet->pos = 1 + packet->remaining_count;
        
    return MOSQ_ERR_SUCCESS;
}

void _mosquitto_free(void *mem)
{
    free(mem);
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

void _mosquitto_message_cleanup(struct mosquitto_message_all **message)
{
    struct mosquitto_message_all *msg;
    
    if(!message || !*message) return;
    
    msg = *message;
    
    if(msg->msg.topic) _mosquitto_free(msg->msg.topic);
    if(msg->msg.payload) _mosquitto_free(msg->msg.payload);
    _mosquitto_free(msg);
}


void _mosquitto_message_cleanup_all(struct mosquitto *mosq)
{
    struct mosquitto_message_all *tmp;
    
    assert(mosq);
    
    while (mosq->messages)
    {
        tmp = mosq->messages->next;
        _mosquitto_message_cleanup(&mosq->messages);
        mosq->messages = tmp;
    }
}

void _mosquitto_out_packet_cleanup_all(struct mosquitto *mosq)
{
    struct _mosquitto_packet *packet;
    
    if(mosq->out_packet && !mosq->current_out_packet)
    {
        mosq->current_out_packet = mosq->out_packet;
        mosq->out_packet = mosq->out_packet->next;
    }
    
    while(mosq->current_out_packet)
    {
        packet = mosq->current_out_packet;
        /* Free data and reset values */
        mosq->current_out_packet = mosq->out_packet;
        if(mosq->out_packet)
        {
            mosq->out_packet = mosq->out_packet->next;
        }
        
        _mosquitto_packet_cleanup(packet);
        _mosquitto_free(packet);
    }
}

void _mosquitto_destroy(struct mosquitto *mosq)
{
    if (!mosq) return;

    _mosquitto_socket_close(mosq);
    _mosquitto_message_cleanup_all(mosq);
    _mosquitto_will_clear(mosq);
    
    /* Paramter cleanup */
    if (mosq->address)
    {
        _mosquitto_free(mosq->address);
        mosq->address = NULL;
    }
    if(mosq->id)
    {
        _mosquitto_free(mosq->id);
        mosq->id = NULL;
    }
    if(mosq->username)
    {
        _mosquitto_free(mosq->username);
        mosq->username = NULL;
    }
    if(mosq->password)
    {
        _mosquitto_free(mosq->password);
        mosq->password = NULL;
    }
    if(mosq->host)
    {
        _mosquitto_free(mosq->host);
        mosq->host = NULL;
    }
    if(mosq->bind_address)
    {
        _mosquitto_free(mosq->bind_address);
        mosq->bind_address = NULL;
    }
    
    _mosquitto_out_packet_cleanup_all(mosq);
    _mosquitto_packet_cleanup(&mosq->in_packet);
}

char *_mosquitto_strdup(const char *s)
{
    char *str = strdup(s);
    
    return str;
}
