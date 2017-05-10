//
//  will_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/5/10.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef will_mosq_h
#define will_mosq_h

#include <stdio.h>
#include "mosquitto_internal.h"
#include "mosquitto.h"

int _mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *paylod, int qos, bool retain);
int _mosquitto_will_clear(struct mosquitto *mosq);

#endif /* will_mosq_h */
