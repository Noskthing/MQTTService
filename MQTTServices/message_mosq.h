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

void _mosq_message_reconnect_reset(struct mosquitto *mosq);

#endif /* message_mosq_h */
