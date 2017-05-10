//
//  will_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/5/10.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "will_mosq.h"
#include "mqtt3_protocol.h"
#include "memory_mosq.h"

#include <assert.h>
#include <string.h>

int _mosquitto_will_clear(struct mosquitto *mosq)
{
    if (!mosq->will) return MOSQ_ERR_SUCCESS;
    
    if (mosq->will->topic)
    {
        _mosquitto_free(mosq->will->topic);
        mosq->will->topic = NULL;
    }
    
    if (mosq->will->payload)
    {
        _mosquitto_free(mosq->will->payload);
        mosq->will->payload = NULL;
    }
    
    _mosquitto_free(mosq->will);
    mosq->will = NULL;
    
    return MOSQ_ERR_SUCCESS;
}

int _mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *payload, int qos, bool retain)
{
    assert(mosq);
    
    int rc;
    
    if (!topic) return MOSQ_ERR_INVAL;
    if (payloadlen < 0 || payloadlen > MQTT_MAX_PAYLOAD) return MOSQ_ERR_PAYLOAD_SIZE;
    if (payloadlen > 0 && !payload) return MOSQ_ERR_INVAL;
    
    if (mosq->will)
    {
        _mosquitto_will_clear(mosq);
    }
    
    mosq->will = _mosquitto_calloc(1, sizeof(struct mosquitto_message));
    if (!mosq->will) return MOSQ_ERR_NOMEM;
    mosq->will->topic = _mosquitto_strdup(topic);
    if (!mosq->will->topic)
    {
        rc = MOSQ_ERR_NOMEM;
        goto cleanup;
    }
    
    mosq->will->payloadlen = payloadlen;
    if (mosq->will->payloadlen > 0)
    {
        if (!payloadlen)
        {
            rc = MOSQ_ERR_INVAL;
            goto cleanup;
        }
        mosq->will->payload = _mosquitto_malloc(sizeof(char)*mosq->will->payloadlen);
        if (!mosq->will->payload)
        {
            rc = MOSQ_ERR_NOMEM;
            goto cleanup;
        }
        memcpy(mosq->will->payload, payload, payloadlen);
    }
    mosq->will->qos = qos;
    mosq->will->retain = retain;
    
    return MOSQ_ERR_SUCCESS;
    
cleanup:
    _mosquitto_will_clear(mosq);
    
    return rc;
}
