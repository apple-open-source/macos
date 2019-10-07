/*
 *  prefix.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Feb 21 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifdef __cplusplus

extern "C++" {

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOCFPlugIn.h>

// system
#import <mach/mach.h>
#import <IOKit/IOKitLib.h>
#if !defined(__LP64__)
#import <IOKit/iokitmig.h>
#endif
#import <exception>
#import <assert.h>
#import <pthread.h>
#import <unistd.h>

}

#endif
