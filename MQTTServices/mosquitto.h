//
//  mosquitto.h
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef mosquitto_h
#define mosquitto_h

#include <stdio.h>
#include <stdbool.h>


#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

/* Error values */
enum mosq_err_t {
    MOSQ_ERR_CONN_PENDING = -1,
    MOSQ_ERR_SUCCESS = 0,
    MOSQ_ERR_NOMEM = 1,
    MOSQ_ERR_PROTOCOL = 2,
    MOSQ_ERR_INVAL = 3,
    MOSQ_ERR_NO_CONN = 4,
    MOSQ_ERR_CONN_REFUSED = 5,
    MOSQ_ERR_NOT_FOUND = 6,
    MOSQ_ERR_CONN_LOST = 7,
    MOSQ_ERR_TLS = 8,
    MOSQ_ERR_PAYLOAD_SIZE = 9,
    MOSQ_ERR_NOT_SUPPORTED = 10,
    MOSQ_ERR_AUTH = 11,
    MOSQ_ERR_ACL_DENIED = 12,
    MOSQ_ERR_UNKNOWN = 13,
    MOSQ_ERR_ERRNO = 14,
    MOSQ_ERR_EAI = 15
};

/* MQTT specification restricts client ids to a maximum of 23 characters */
#define MOSQ_MQTT_ID_MAX_LENGTH 23

struct mosquitto_message{
    int mid;
    char *topic;
    void *payload;
    int payloadlen;
    int qos;
    bool retain;
};

struct mosquitto;

int mosquitto_reconnect(struct mosquitto *mosq);

struct mosquitto *mosquitto_new(const char *id, bool clean_session ,void *userdata);
#endif /* mosquitto_h */




