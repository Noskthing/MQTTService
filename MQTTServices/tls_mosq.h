//
//  tls_mosq.h
//  MQTTServices
//
//  Created by ml on 2017/5/15.
//  Copyright © 2017年 李博文. All rights reserved.
//

#ifndef tls_mosq_h
#define tls_mosq_h

#include <stdio.h>
#include "mosquitto_internal.h"

int _mosquitto_server_certificate_verify(int preverify_ok, X509_STORE_CTX *ctx);
int _mosquitto_verify_certificate_hostname(X509 *cert, const char *hostname);

#endif /* tls_mosq_h */
