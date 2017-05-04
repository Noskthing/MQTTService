//
//  time_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/5/3.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef time_mosq_h
#define time_mosq_h

#include <stdio.h>
#include "mosquitto_internal.h"
#include "net_mosq.h"
#include "client_mosq.h"
#include "logger.h"

time_t mosquitto_time();

void _mosquitto_check_keepalive(struct mosquitto *mosq);
#endif /* time_mosq_h */
