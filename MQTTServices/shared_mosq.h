//
//  shared_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/5/5.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef shared_mosq_h
#define shared_mosq_h

#include <stdio.h>
#include "mosquitto_internal.h"
#include "mosquitto.h"
#include "mqtt3_protocol.h"
#include "time_mosq.h"
#include "util_mosq.h"

#pragma mark public method
/* For DISCONNECT, PINGREQ and PINGRESP */
int _mosq_send_simple_command(struct mosquitto *mosq, uint8_t command);
int _mosquitto_send_real_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup);
/* For PUBACK, PUBCOMP, PUBREC, and PUBREL */
int _mosquitto_send_command_with_mid(struct mosquitto *mosq, uint8_t command, uint16_t mid, bool dup);

#pragma mark publish
int _mosq_send_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup);
/* Qos 1 */
int _mosquitto_send_puback(struct mosquitto *mosq, uint16_t mid);
/* Qos 2 */
int _mosquitto_send_pubrec(struct mosquitto *mosq, uint16_t mid);
int _mosquitto_send_pubrel(struct mosquitto *mosq, uint16_t mid, bool dup);
int _mosquitto_send_pubcomp(struct mosquitto *mosq, uint16_t mid);
#endif /* shared_mosq_h */
