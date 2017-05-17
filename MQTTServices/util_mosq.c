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

int _mosq_read_bytes(struct _mosquitto_packet *packet, void *bytes, uint32_t count)
{
    assert(packet);
    if(packet->pos+count > packet->remaining_length) return MOSQ_ERR_PROTOCOL;
    
    memcpy(bytes, &(packet->payload[packet->pos]), count);
    packet->pos += count;
    
    return MOSQ_ERR_SUCCESS;
}

int _mosq_read_uint16(struct _mosquitto_packet *packet, uint16_t *word)
{
    assert(packet);
    if (packet->pos + 2 > packet->remaining_length) return MOSQ_ERR_PROTOCOL;
    
    uint8_t msb,lsb;
    msb = packet->payload[packet->pos];
    packet->pos ++;
    lsb = packet->payload[packet->pos];
    packet->pos ++;
    
    *word = (msb<<8) + lsb;
    
    return MOSQ_ERR_SUCCESS;
}

int _mosq_read_string(struct _mosquitto_packet *packet, char **str)
{
    assert(packet);
    
    uint16_t len;
    int rc;
    rc = _mosq_read_uint16(packet, &len);
    if (rc) return rc;
        
    if (packet->pos + len > packet->remaining_length) return MOSQ_ERR_PROTOCOL;
        
    *str = _mosquitto_calloc(len + 1, sizeof(char));
    if (*str)
    {
        memcpy(*str, &(packet->payload[packet->pos]), len);
        packet->pos += len;
    }
    else
    {
        return MOSQ_ERR_NOMEM;
    }
    
    return MOSQ_ERR_SUCCESS;
}

uint16_t _mosquitto_mid_generate(struct mosquitto *mosq)
{
    assert(mosq);
    
    mosq->last_mid++;
    if(mosq->last_mid == 0) mosq->last_mid++;
    
    return mosq->last_mid;
}

/* Convert ////some////over/slashed///topic/etc/etc//
 * into some/over/slashed/topic/etc/etc
 */
int _mosquitto_fix_sub_topic(char **subtopic)
{
    char *fixed = NULL;
    char *token;
    char *saveptr = NULL;
    
    assert(subtopic);
    assert(*subtopic);
    
    if(strlen(*subtopic) == 0) return MOSQ_ERR_SUCCESS;
    /* size of fixed here is +1 for the terminating 0 and +1 for the spurious /
     * that gets appended. */
    fixed = _mosquitto_calloc(strlen(*subtopic)+2, 1);
    if(!fixed) return MOSQ_ERR_NOMEM;
    
    if((*subtopic)[0] == '/'){
        fixed[0] = '/';
    }
    token = strtok_r(*subtopic, "/", &saveptr);
    while(token){
        strcat(fixed, token);
        strcat(fixed, "/");
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    fixed[strlen(fixed)-1] = '\0';
    _mosquitto_free(*subtopic);
    *subtopic = fixed;
    return MOSQ_ERR_SUCCESS;
}

FILE *_mosquitto_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}
