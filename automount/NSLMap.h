#ifndef __NSL_MAP_H__
#define _NSL_MAP_H__

#import <sys/types.h>
#import <sys/queue.h>
#import <pthread.h>

#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>

#import "AMMap.h"

@class NSLVnode;

@interface NSLMap : Map
{
    NSLClientRef clientRef;
}

- (NSLClientRef)getNSLClientRef;

@end

#endif __NSL_MAP_H__
