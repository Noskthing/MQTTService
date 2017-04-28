//
//  client_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef client_mosq_h
#define client_mosq_h

#include <stdio.h>
#include "mosquitto.h"
#include "mqtt3_protocol.h"


int client_send_connect_command_mosq(struct mosquitto *mosq, uint16_t keeplive, bool clean_session);

#endif /* client_mosq_h */
