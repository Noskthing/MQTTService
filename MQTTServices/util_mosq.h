//
//  util_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef util_mosq_h
#define util_mosq_h

#include <stdio.h>
#include "mosquitto_internal.h"
#include "memory_mosq.h"

/* Macros for accessing the MSB and LSB of a uint16_t */
#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

#endif /* util_mosq_h */

#pragma mark write
void _mosq_write_string(struct _mosquitto_packet *packet, const char * str, uint16_t length);
void _mosq_write_byte(struct _mosquitto_packet *packet, uint8_t byte);
void _mosq_write_uint16(struct _mosquitto_packet *packet, uint16_t word);
#pragma mark read
int _mosq_read_byte(struct _mosquitto_packet *packet, uint8_t *byte);
int _mosq_read_uint16(struct _mosquitto_packet *packet, uint16_t *word);
int _mosq_read_string(struct _mosquitto_packet *packet, char **str);
#pragma mark mid
uint16_t _mosquitto_mid_generate(struct mosquitto *mosq);
