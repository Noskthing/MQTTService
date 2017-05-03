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

/* Macros for accessing the MSB and LSB of a uint16_t */
#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

#endif /* util_mosq_h */


void _mosq_write_string(struct _mosquitto_packet *packet, const char * str, uint16_t length);
void _mosq_write_byte(struct _mosquitto_packet *packet, uint8_t byte);
void _mosq_write_uint16(struct _mosquitto_packet *packet, uint16_t word);

int _mosq_read_byte(struct _mosquitto_packet *packet, uint8_t *byte);
