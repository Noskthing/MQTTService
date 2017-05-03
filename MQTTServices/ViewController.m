//
//  ViewController.m
//  MQTTServices
//
//  Created by ml on 2017/4/25.
//  Copyright © 2017年 李博文. All rights reserved.
//

#import "ViewController.h"
#include "mosquitto.h"
#include "mosquitto_internal.h"
#include "client_mosq.h"

@interface ViewController ()

@end

@implementation ViewController

static void connect_ack_callback_test(struct mosquitto * mosq, void *userdata, int rc)
{
    printf("rs-----%d\n",rc);
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    struct mosquitto *mosq =  mosquitto_new("4A05FF63-AEC9-4941-96BD-6C7932C7F2DA", false, NULL);
    mosq->username = "efabab60-deb9-11e6-b789-1d29b1631a66";
    mosq->password = "c8481f8843e5083c9431a557e1458387f6d5174e";
    mosq->host = "52.220.124.2";
    mosq->on_connect = connect_ack_callback_test;
    
    int rs = mosquitto_reconnect(mosq);
    NSLog(@"rs is %d",rs);
    
    dispatch_queue_t queue = dispatch_queue_create("E9823A44", NULL);
    dispatch_async(queue, ^{
        mosquitto_loop_forever(mosq, -1, 1);
    });
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


@end
