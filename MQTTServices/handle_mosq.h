//
//  handle_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/5/8.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef handle_mosq_h
#define handle_mosq_h

#include "mosquitto_internal.h"
#include "memory_mosq.h"
#include "util_mosq.h"
#include "logger.h"
#include "shared_mosq.h"
#include "message_mosq.h"

#include <stdio.h>

int _mosquitto_handle_pingreq(struct mosquitto *mosq);
int _mosquitto_handle_publish(struct mosquitto *mosq);
int _mosquitto_handle_puback(struct mosquitto *mosq, const char *type);
int _mosquitto_handle_pubrec(struct mosquitto *mosq);
int _mosquitto_handle_pubrel(struct mosquitto *mosq);
#endif /* handle_mosq_h */
