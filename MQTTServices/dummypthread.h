//
//  dummypthread.h
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef dummypthread_h
#define dummypthread_h

#define pthread_create(A, B, C, D)
#define pthread_join(A, B)
#define pthread_cancel(A)

#define pthread_mutex_init(A, B)
#define pthread_mutex_destroy(A)
#define pthread_mutex_lock(A)
#define pthread_mutex_unlock(A)

#endif /* dummypthread_h */
