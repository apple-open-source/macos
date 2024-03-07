//
//  CodesigningUnitTests.mm
//  CodesigningUnitTests
//
//  Copyright (c) 2023 Apple. All rights reserved.
//
#import <Foundation/Foundation.h>
#import <os/log.h>

// Just send all logging into the default log during unit tests runs.
os_log_t secLogObjForScope(const char *scope);
os_log_t secLogObjForScope(const char *scope) {
    return OS_LOG_DEFAULT;
}
