//
//  logger.h
//  MQTTServices
//
//  Created by ml on 2017/5/2.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef logger_h
#define logger_h

#include <time.h>


#define LOG_DEBUG 1
#define  LOG_INFO_LEVEL 0x0001
#define  LOG_WARNING_LEVEL 0x0002
#define  LOG_ERROR_LEVEL 0x0004
#define  LOG_ALL_LEVEL 0xFFFF

static unsigned int  flags =   LOG_INFO_LEVEL | LOG_WARNING_LEVEL;

#if  LOG_DEBUG
    #define  C_LOG(flag, level, fmt, ...) \
        do{\
            if(flags & flag){\
            time_t t = time(0);\
            struct tm ttt = *localtime(&t);\
            printf("[%s] [%4d-%02d-%02d %02d:%02d:%02d] [%s:%d]      " fmt "", level,\
             ttt.tm_year + 1900, ttt.tm_mon + 1, ttt.tm_mday, ttt.tm_hour,\
            ttt.tm_min, ttt.tm_sec, __FUNCTION__ , __LINE__, ##__VA_ARGS__);\
            printf("\n");\
            }\
        }while(0)

    #define LOG_INFO(fmt, ...)      C_LOG(LOG_INFO_LEVEL, "INFO", fmt, ##__VA_ARGS__)
    #define LOG_WARNING(fmt, ...)   C_LOG(LOG_WARNING_LEVEL, "WARNING", fmt, ##__VA_ARGS__)
    #define LOG_ERROR(fmt, ...)     C_LOG(LOG_ERROR_LEVEL, "ERROR", fmt, ##__VA_ARGS__)
    #define LOG_ALL(fmt, ...)       C_LOG(LOG_ALL_LEVEL, "ALL", fmt, ##__VA_ARGS__)
#else
    #define LOG(fmt, ...)
    #define LOG_INFO(fmt, ...)
    #define LOG_WARNING(fmt, ...)
    #define LOG_ERROR(fmt, ...)
    #define LOG_ALL(fmt, ...)
#endif



#endif /* logger_h */
