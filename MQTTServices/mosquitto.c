//
//  mosquitto.c
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "mosquitto.h"
#include "net_mosq.h"


static int _mosquitto_reconnect(struct mosquitto *mosq, bool blocking)
{
    int rc;
    struct _mosquitto_packet *packet;
    
    //check paramter
    if (!mosq || !mosq->host || mosq->port <=0) return MOSQ_ERR_INVAL;
    
    mosq->state = mosq_cs_new;
    
    _mosquitto_packet_cleanup(&mosq->in_packet);
    
    rc = _mosq_socket_connect(mosq, mosq->host, mosq->port, mosq->bind_address, blocking);
    if (rc)return rc;
    
    return 0;
}
