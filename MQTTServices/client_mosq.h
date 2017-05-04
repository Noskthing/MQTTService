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
#include "time_mosq.h"

#pragma mark connect
int client_send_connect_command_mosq(struct mosquitto *mosq, uint16_t keeplive, bool clean_session);
int client_receive_connect_ack_mosq(struct mosquitto *mosq);
#pragma mark ping
int client_send_ping_request_mosq(struct mosquitto *mosq);
int client_receive_ping_response_mosq(struct mosquitto *mosq);
#pragma mark sub
int client_send_subscribe_mosq(struct mosquitto *mosq, int *mid, const char *topic, uint8_t topic_qos);
int client_receive_suback_mosq(struct mosquitto *mosq);
#pragma mark disconnect
int client_send_disconnect_command_mosq(struct mosquitto *mosq);
#endif /* client_mosq_h */
