//
//  time_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/5/3.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "time_mosq.h"


#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#include <time.h>
#include <assert.h>

#ifdef WIN32
static bool tick64 = false;

void _windows_time_version_check(void)
{
    OSVERSIONINFO vi;
    
    tick64 = false;
    
    memset(&vi, 0, sizeof(OSVERSIONINFO));
    vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if(GetVersionEx(&vi)){
        if(vi.dwMajorVersion > 5){
            tick64 = true;
        }
    }
}
#endif

time_t mosquitto_time()
{
    static mach_timebase_info_data_t tb;
    uint64_t ticks;
    uint64_t sec;
    
    ticks = mach_absolute_time();
    
    if(tb.denom == 0){
        mach_timebase_info(&tb);
    }
    sec = (ticks/1000000000)*(tb.numer/tb.denom);
    
    return (time_t)sec;
}

void _mosquitto_check_keepalive(struct mosquitto *mosq)
{
    assert(mosq);
    
    time_t last_msg_out;
    time_t last_msg_in;
    time_t now;
    
    int rc;
    
    now = mosquitto_time();
    last_msg_out = mosq->last_msg_out;
    last_msg_in = mosq->last_msg_in;
    /*
     socket连接存在
     最后一条发送/接收的消息的时间与现在的间隔超过keepalive
     */
    if (mosq->sock != INVALID_SOCKET &&
        (now - last_msg_out >= mosq->keepalive || now - last_msg_in >= mosq->keepalive))
    {
        /*
         连接状态并且没有未确认的PINGREQ
         ----> 发送PINGREQ
         */
        if (mosq->state == mosq_cs_connected && mosq->ping_t == 0)
        {
            mosq->last_msg_in = now;
            mosq->last_msg_out = now;
            client_send_ping_request_mosq(mosq);
        }
        /*
         断开socket
         */
        else
        {
            _mosquitto_socket_close(mosq);
            if (mosq->state == mosq_cs_disconnecting)
            {
                rc = MOSQ_ERR_SUCCESS;
            }
            else
            {
                rc = MOSQ_ERR_NOMEM;
            }
            
            if (mosq->on_disconnect)
            {
                mosq->in_callback = true;
                mosq->on_disconnect(mosq,mosq->userdata,rc);
                mosq->in_callback = false;
            }
        }
    }
}
