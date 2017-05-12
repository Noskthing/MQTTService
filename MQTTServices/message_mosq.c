//
//  message_mosq.c
//  MQTTServices
//
//  Created by ml on 2017/4/26.
//  Copyright © 2017年 李博文. All rights reserved.
//

#include "message_mosq.h"
#include "logger.h"
#include <assert.h>

void _mosq_messages_reconnect_reset(struct mosquitto *mosq)
{
    assert(mosq);
    
    struct mosquitto_message_all *message;
    struct mosquitto_message_all *prev = NULL;
    
    mosq->queue_len = 0;
    mosq->inflight_messages = 0;
    message = mosq->messages;
    while (message)
    {
        message->timestamp = 0;
        if (message->direction == mosq_md_out)
        {
            mosq->queue_len++;
            if (message->msg.qos > 0)
            {
                mosq->inflight_messages++;
            }
            
            if (mosq->max_inflight_messages == 0 || mosq->inflight_messages < mosq->max_inflight_messages)
            {
                if (message->msg.qos == 1)
                {
                    message->state = mosq_ms_wait_for_puback;
                }
                else if(message->msg.qos == 2)
                {
                    /* 保存状态 */
                }
            }
            else
            {
                message->state = mosq_ms_invalid;
            }
        }
        else
        {
            if (message->msg.qos != 2)
            {
                if (prev)
                {
                    prev->next = message->next;
                    _mosquitto_message_cleanup(&message);
                    message = prev;
                }
                else
                {
                    mosq->messages = message->next;
                    _mosquitto_message_cleanup(&message);
                    message = mosq->messages;
                }
            }
            else
            {
                /*
                 等待接收的message如果Qos = 2的话
                 那么可以不修改message->state
                 因为无论客户端是否接收到message，应该还是可以匹配上的
                 */
            }
        }
    }
    mosq->messages_last = prev;
}

void _mosquitto_message_queue(struct mosquitto *mosq, struct mosquitto_message_all *message, bool doinc)
{
    assert(mosq);
    assert(message);
    
    mosq->queue_len ++;
    if (doinc == true && message->msg.qos > 0 && (mosq->max_inflight_messages == 0 || mosq->inflight_messages < mosq->max_inflight_messages))
    {
        mosq->inflight_messages ++;
    }
    
    message->next = NULL;
    if (mosq->messages_last)
    {
        mosq->messages_last->next = message;
    }
    else
    {
        mosq->messages = message;
    }
    mosq->messages_last = message;
}

void _mosquitto_messages_retry_check(struct mosquitto *mosq)
{
    assert(mosq);
    
    struct mosquitto_message_all *message;
    time_t now = mosquitto_time();
    
    message = mosq->messages;
    while (message)
    {
        /* 
         消息的发送时间 + 重新发送时间 < 当前时间 
         没有返回  等待超时
         */
        if (message->timestamp + mosq->message_retry < now)
        {
            message->timestamp = now;
            message->dup = true;
            switch (message->state)
            {
                case mosq_ms_wait_for_puback:
                case mosq_ms_wait_for_pubrec:
                    _mosq_send_publish(mosq, message->msg.mid, message->msg.topic, message->msg.payloadlen, message->msg.payload, message->msg.qos, message->msg.retain, message->dup);
                    break;
                case mosq_ms_wait_for_pubrel:
                    _mosquitto_send_pubrec(mosq, message->msg.mid);
                    break;
                case mosq_ms_wait_for_pubcomp:
                    _mosquitto_send_pubrel(mosq, message->msg.mid, true);
                default:
                    break;
            }
        }
        message = message->next;
    }
}

int _mosquitto_message_remove(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir, struct mosquitto_message_all **message)
{
    assert(mosq);
    assert(message);
    
    /* cur代表当前操作的message prev是上一个操作的message */
    struct mosquitto_message_all *cur, *prev = NULL;
    bool found = false;
    int rc;
    
    cur = mosq->messages;
    while (cur)
    {
        /* found */
        if (cur->msg.mid == mid && cur->direction == dir)
        {
            *message = cur;
            mosq->queue_len--;
            /* 修改mosq->message */
            if (prev)
            {
                prev->next = cur->next;
            }
            else
            {
                mosq->messages = cur->next;
            }
            /* 修改mosq->messages_last */
            if (cur->next == NULL)
            {
                mosq->messages_last = prev;
            }
            else if (!mosq->messages)
            {
                mosq->messages_last = NULL;
            }
            if ((cur->msg.qos == 2 && dir == mosq_md_in) || (cur->msg.qos > 0 && dir == mosq_md_out))
            {
                mosq->inflight_messages--;
            }
            found = true;
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    
    if (found)
    {
        cur = mosq->messages;
        while (cur)
        {
            if (mosq->max_inflight_messages == 0 || mosq->inflight_messages < mosq->max_inflight_messages)
            {
                if (cur->msg.qos > 0 && cur->state == mosq_ms_invalid && cur->direction == mosq_md_out)
                {
                    mosq->inflight_messages ++;
                    if (cur->msg.qos == 1)
                    {
                        cur->state = mosq_ms_wait_for_puback;
                    }
                    else if(cur->msg.qos == 2)
                    {
                        cur->state = mosq_ms_wait_for_pubrec;
                    }
                    rc = _mosq_send_publish(mosq, cur->msg.mid, cur->msg.topic, cur->msg.payloadlen, cur->msg.payload, cur->msg.qos, cur->msg.retain, cur->dup);
                    if (rc)
                    {
                        return rc;
                    }
                }
                else
                {
                    return MOSQ_ERR_SUCCESS;
                }
            }
        }
        return MOSQ_ERR_SUCCESS;
    }
    else
    {
        return MOSQ_ERR_NOT_FOUND;
    }
}

int _mosquitto_message_delete(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir)
{
    assert(mosq);
    
    struct mosquitto_message_all *message;
    int rc;
    
    rc = _mosquitto_message_remove(mosq, mid, dir, &message);
    if (rc == MOSQ_ERR_SUCCESS)
    {
        _mosquitto_message_cleanup(&message);
    }
    return rc;
}

int _mosquitto_message_update(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir, enum mosquitto_msg_state state)
{
    assert(mosq);
    
    struct mosquitto_message_all *message;
    
    message = mosq->messages;
    while (message)
    {
        if (message->msg.mid == mid && message->direction == dir)
        {
            message->state = state;
            message->timestamp = mosquitto_time();
            return MOSQ_ERR_SUCCESS;
        }
        message = message->next;
    }
    return MOSQ_ERR_SUCCESS;
}

void _mosquitto_message_cleanup_all(struct mosquitto *mosq)
{
    struct mosquitto_message_all *tmp;
    
    assert(mosq);
    
    while (mosq->messages)
    {
        tmp = mosq->messages->next;
        _mosquitto_message_cleanup(&mosq->messages);
        mosq->messages = tmp;
    }
}

void _mosquitto_message_cleanup(struct mosquitto_message_all **message)
{
    struct mosquitto_message_all *msg;
    
    if(!message || !*message) return;
    
    msg = *message;
    
    if(msg->msg.topic) _mosquitto_free(msg->msg.topic);
    if(msg->msg.payload) _mosquitto_free(msg->msg.payload);
    _mosquitto_free(msg);
}
