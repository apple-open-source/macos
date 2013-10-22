/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
 */

#ifndef __PPPCONTROLLER_REACHABILITY_H__
#define __PPPCONTROLLER_REACHABILITY_H__

struct service;

typedef void (^ReachabilityChangedBlock)(struct service *serv);

void reachability_init(CFRunLoopRef cb_runloop, CFTypeRef cb_runloop_mode, ReachabilityChangedBlock cb_block);
void reachability_clear(struct service *serv);
void reachability_reset(struct service *serv);

#endif /* __PPPCONTROLLER_REACHABILITY_H__ */
