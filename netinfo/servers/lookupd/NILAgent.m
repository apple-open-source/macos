/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * NILAgent.m
 *
 * Negative entry agent - creates negative records.
 *
 * Copyright (c) 1996, NeXT Software Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "NILAgent.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import "Config.h"
#import <time.h>
#import <arpa/inet.h>

#define DefaultTimeToLive 60

static NILAgent *_sharedNILAgent = nil;

@implementation NILAgent

- (NILAgent *)init
{
	LUDictionary *config;

	if (didInit) return self;

	[super init];

	timeToLive = DefaultTimeToLive;
	config = [configManager configForAgent:"NILAgent" fromConfig:configurationArray];
	if (config != nil)
	{
		if ([config valueForKey:"TimeToLive"] != NULL)
			timeToLive = [config unsignedLongForKey:"TimeToLive"];
	}

	return self;
}

- (LUAgent *)initWithArg:(char *)arg
{
	return [self init];
}

+ (NILAgent *)alloc
{
	if (_sharedNILAgent != nil)
	{
		[_sharedNILAgent retain];
		return _sharedNILAgent;
	}

	_sharedNILAgent = [super alloc];
	_sharedNILAgent = [_sharedNILAgent init];
	if (_sharedNILAgent == nil) return nil;

	system_log(LOG_DEBUG, "Allocated NILAgent 0x%08x\n", (int)_sharedNILAgent);

	return _sharedNILAgent;
}

- (const char *)shortName
{
	return "NIL";
}

- (void)dealloc
{
	system_log(LOG_DEBUG, "Deallocated NILAgent 0x%08x\n", (int)self);
	[super dealloc];
	_sharedNILAgent = nil;
}

- (BOOL)isValid:(LUDictionary *)item
{
	time_t now;
	time_t bestBefore;

	if (item == nil) return NO;

	bestBefore = [item unsignedLongForKey:"_lookup_NIL_best_before"];
	if (bestBefore == -1) return YES;

	now = time(0);

	if (now > bestBefore) return NO;
	return YES;
}

- (LUDictionary *)stamp:(LUDictionary *)item
{
	time_t best_before;
	char scratch[32];

	[item setNegative:YES];

	if (timeToLive == 0)
	{
		[item setValue:"-1" forKey:"_lookup_NIL_best_before"];
	}
	else
	{
		best_before = [item dob] + timeToLive;
		sprintf(scratch, "%lu", best_before);
		[item setValue:scratch forKey:"_lookup_NIL_best_before"];
	}

	[item setValue:"NIL" forKey:"_lookup_agent"];
	[item setValue:"NIL" forKey:"_lookup_info_system"];
	return item;
}

- (LUDictionary *)itemWithKey:(char *)key value:(char *)val
{
	LUDictionary *item;

	item = [[LUDictionary alloc] initTimeStamped];
	[item setValue:val forKey:key];
	return [self stamp:item];
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	return [self itemWithKey:key value:val];
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	LUDictionary *vstamp;
	LUArray *all;

	all = [[LUArray alloc] init];

	vstamp = [[LUDictionary alloc] initTimeStamped];
	[vstamp setBanner:"NILAgent validation"];
	[self stamp:vstamp];
	[all addValidationStamp:vstamp];
	[vstamp release];

	return all;
}

- (LUDictionary *)itemWithKey:(char *)key intValue:(int)val
{
	LUDictionary *item;
	char str[64];

	sprintf(str, "%d", val);
	item = [[LUDictionary alloc] initTimeStamped];
	[item setValue:str forKey:key];
	return [self stamp:item];
}

- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot
{
	LUDictionary *service;

	service = [self itemWithKey:"name" value:name];
	if (prot != NULL) [service setValue:prot forKey:"protocol"];
	return service;
}

- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot
{
	LUDictionary *service;

	service = [self itemWithKey:"name" intValue:*number];
	if (prot != NULL) [service setValue:prot forKey:"protocol"];
	return service;
}

@end
