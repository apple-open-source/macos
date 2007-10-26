/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import "MyController.h"
#import <DSlibinfoMIG.h>
#import <DSlibinfoMIG_types.h>
#import <servers/bootstrap.h>
#import <OpenDirectory/NSOpenDirectory.h>

@implementation MyController

- (id)init
{
    statistics = [[NSMutableArray alloc] init];
	cacheInfo = [NSMutableDictionary new];
    [self controlClicked: nil];
    return self;
}

- (void)updateWithPercentage:(NSMutableArray *)inArray
{
    NSEnumerator    *objEnum = [inArray objectEnumerator];
    id              object;
    
    while( object = [objEnum nextObject] )
    {
        id  object2 = [object objectForKey:@"subValues"];
        if( object2 != nil )
        {
            [self updateWithPercentage: object2];
        }
        else
        {
            int cacheHits = [[object objectForKey:@"Cache Hits"] intValue];
            int cacheMisses = [[object objectForKey:@"Cache Misses"] intValue];
            int totalCalls = cacheHits + cacheMisses;
            
            if( cacheHits > 0 )
                [object setObject:[NSString stringWithFormat:@"%0.2f%%", (cacheHits * 100.0)/totalCalls] forKey:@"Cache Hits"];
            else
                [object setObject:[NSString stringWithFormat:@"%0.2f%%", 0.0] forKey:@"Cache Hits"];
            
            if( cacheMisses > 0 )
                [object setObject:[NSString stringWithFormat:@"%0.2f%%", (cacheMisses * 100.0)/totalCalls] forKey:@"Cache Misses"];
            else
                [object setObject:[NSString stringWithFormat:@"%0.2f%%", 0.0] forKey:@"Cache Misses"];
        }
    }    
}

- (void)refreshCacheDetails
{
	NSPropertyListFormat    format = NSPropertyListXMLFormat_v1_0;

	[detailsWindow makeKeyAndOrderFront: nil];
	
	ODNode          *odNode = [ODNode nodeWithSession: [ODSession defaultSession] name: @"/Cache" error: nil];
	NSDictionary    *details = [odNode nodeDetailsForKeys: [NSArray arrayWithObject: @"dsAttrTypeNative:LibinfoCacheDetails"] error: nil];
	NSString        *detailsString = [[details objectForKey:@"dsAttrTypeNative:LibinfoCacheDetails"] lastObject];
	
	NSData *tempData = [detailsString dataUsingEncoding: NSUTF8StringEncoding];
	
	[self willChangeValueForKey: @"cacheInfo"];
	
	[cacheInfo setDictionary:[NSPropertyListSerialization propertyListFromData: tempData
														   mutabilityOption: NSPropertyListMutableContainersAndLeaves
																	 format: &format
														   errorDescription: nil]];
	
	NSMutableArray *cacheEntries = [NSMutableArray array];
	
	NSArray *entries = [cacheInfo objectForKey: @"Entries"];
	int		iCount = 1;
	
	for (NSDictionary *entry in entries)
	{
		NSMutableDictionary *newEntry = [NSMutableDictionary dictionary];
		NSMutableArray *attributes = [NSMutableArray array];
		
		[cacheEntries addObject: newEntry];
		
		for (NSString *key in [entry allKeys])
		{
			id object = [entry objectForKey: key];
			if ([key isEqualToString: @"Keys"])
			{
				// this is an array
				for (NSString *value in object)
				{
					[attributes addObject: [NSDictionary dictionaryWithObject: value forKey: @"value"]];
				}
				
			}
			else if ([key isEqualToString: @"Validation Information"])
			{
				NSMutableDictionary *attribute = [NSMutableDictionary dictionaryWithObject: key forKey: @"value"];
				
				[attributes addObject: attribute];
				
				NSMutableArray *subAttributes = [NSMutableArray array];
				
				for (NSString *subKey in [object allKeys])
				{
					NSString *subObject = [object objectForKey: subKey];
					NSMutableDictionary *subValues = [NSMutableDictionary dictionaryWithObject: subObject forKey: @"value"];
					
					NSMutableDictionary *tempDict;
					
					tempDict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
								subKey, @"value", 
								[NSMutableArray arrayWithObject: subValues], @"subValues",
								NULL];
					
					[subAttributes addObject: tempDict];
				}
				
				[attribute setObject: subAttributes forKey: @"subValues"];
			}
			else if ([key isEqualToString: @"Type"])
			{
				[newEntry setObject: [NSString stringWithFormat: @"%05d: %@", iCount, object] forKey: @"value"];
			}
			else if ([key isEqualToString: @"Best Before"] || [key isEqualToString: @"Last Access"])
			{
				[newEntry setObject: [object descriptionWithCalendarFormat: @"%m/%d/%y %H:%M:%S" timeZone: nil locale: nil] forKey: key];
			}
			else
			{
				[newEntry setObject: object forKey: key];
			}
		}
		
		if ([attributes count])
		{
			[newEntry setObject: attributes forKey: @"subValues"];
		}
		
		iCount++;
	}
	
	[cacheInfo setObject: cacheEntries forKey: @"Entries"];
	
	[self didChangeValueForKey: @"cacheInfo"];
}

- (IBAction)controlClicked:(id)sender
{
    NSPropertyListFormat    format = NSPropertyListXMLFormat_v1_0;

	// this is a refresh statistics
	if( [sender tag] == 0 )
	{
		ODNode          *odNode = [ODNode nodeWithSession: [ODSession defaultSession] name: @"Cache" error: nil];
		NSDictionary    *details = [odNode nodeDetailsForKeys: [NSArray arrayWithObject: @"dsAttrTypeNative:Statistics"] error: nil];
		NSString        *statsString = [[details objectForKey:@"dsAttrTypeNative:Statistics"] lastObject];
		
		[_origStats release];
		_origStats = [[statsString dataUsingEncoding: NSUTF8StringEncoding] retain];
		
		[self willChangeValueForKey: @"statistics"];
		
		[statistics setArray:[NSPropertyListSerialization propertyListFromData: _origStats
													 mutabilityOption: NSPropertyListMutableContainersAndLeaves
															   format: &format
													 errorDescription: nil]];
		if( showPercentages )
		{
			[self updateWithPercentage: statistics];
		}
		
		[self didChangeValueForKey: @"statistics"];
	}
    else if( [sender tag] == 1 )
    {
        [self willChangeValueForKey: @"statistics"];
        
        [statistics setArray:[NSPropertyListSerialization propertyListFromData: _origStats
                                                              mutabilityOption: NSPropertyListMutableContainersAndLeaves
                                                                        format: &format
                                                              errorDescription: nil]];
        if( showPercentages )
        {
            [self updateWithPercentage: statistics];
        }
        
        [self didChangeValueForKey: @"statistics"];
    }
	else if( [sender tag] == 2 )
	{
		[self refreshCacheDetails];
	}
	else if ( [sender tag] == 3 )
	{
		mach_port_t				serverPort;
		char					reply[16384]	= { 0, };
		mach_msg_type_number_t	replyCnt		= 0;
		vm_offset_t				ooreply			= 0;
		mach_msg_type_number_t	ooreplyCnt		= 0;
		int32_t                 procno		= 0;
		security_token_t        userToken;

		if ( bootstrap_look_up( bootstrap_port, kDSStdMachDSLookupPortName, &serverPort ) == KERN_SUCCESS )
		{
			if ( libinfoDSmig_GetProcedureNumber(serverPort, "_flushcache", &procno, &userToken) == KERN_SUCCESS )
			{
				libinfoDSmig_Query( serverPort, procno, "", 0, reply, &replyCnt, &ooreply, &ooreplyCnt, &userToken );
				[self refreshCacheDetails];
			}
		}
	}
}

@end
