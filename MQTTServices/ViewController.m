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

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    struct mosquitto *mosq =  mosquitto_new("", false, NULL);
    mosq->username = "";
    mosq->password = "";
    mosq->host = "";
    
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
