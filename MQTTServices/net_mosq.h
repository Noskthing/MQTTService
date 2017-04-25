//
//  net_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef net_mosq_h
#define net_mosq_h

#include "memory_mosq.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <winsock2.h>
typedef int ssize_t;
#endif

#include <stdbool.h>

#endif /* net_mosq_h */



int _mosq_socket_connect(struct mosquitto* mosq,const char * host,uint16_t port,const char * bind_address,bool blocking);
int _mosq_packet_queue(struct mosquitto *mosq, struct _mosquitto_packet *packet);
