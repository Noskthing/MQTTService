//
//  message_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/4/26.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef message_mosq_h
#define message_mosq_h

#include <stdio.h>
#include "mosquitto_internal.h"
#include "shared_mosq.h"
#include "time_mosq.h"

void _mosq_message_reconnect_reset(struct mosquitto *mosq);
void _mosquitto_message_queue(struct mosquitto *mosq, struct mosquitto_message_all *message, bool doinc);

int _mosquitto_message_delete(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir);
int _mosquitto_message_update(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir, enum mosquitto_msg_state state);
#endif /* message_mosq_h */
