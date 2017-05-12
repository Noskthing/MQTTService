//
//  thread_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/5/12.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include <stdio.h>
#include "mosquitto_internal.h"

#ifndef WIN32
#include <unistd.h>
#endif

void *_mosquitto_thread_main(void *obj);

int mosquitto_loog_start(struct mosquitto *mosq)
{
#ifdef WITH_THREADING
    if (!mosq) return MOSQ_ERR_INVAL;
    
    pthread_create(&mosq->thread_id, NULL, _mosquitto_thread_main, mosq);
    return MOSQ_ERR_SUCCESS;
#else
    return MOSQ_ERR_NOT_SUPPORTED;
#endif
}

int mosquitto_loop_stop(struct mosquitto *mosq, bool force)
{
#ifdef WITH_THREADING
    if (!mosq) return MOSQ_ERR_INVAL;
    
    if (force)
    {
        pthread_cancel(mosq->thread_id);
    }
    pthread_join(mosq->thread_id, NULL);
    mosq->thread_id = pthread_self();
    return MOSQ_ERR_SUCCESS;
#else
    return MOSQ_ERR_NOT_SUPPORTED;
#endif
}

#ifdef WITH_THREADING
void *_mosquitto_thread_main(void *obj)
{
    struct mosquitto *mosq = obj;
    
    if (!mosq) return NULL;
    
    mosq->threaded = true;
    pthread_mutex_lock(&mosq->state_mutex);
    if (mosq->state == mosq_cs_connected)
    {
        pthread_mutex_unlock(&mosq->state_mutex);
        mosquitto_reconnect(mosq);
    }
    else
    {
        pthread_mutex_unlock(&mosq->state_mutex);
    }
    mosquitto_loop_forever(mosq, -1, 1);
    
    mosq->threaded = false;
    return obj;
}
#endif
