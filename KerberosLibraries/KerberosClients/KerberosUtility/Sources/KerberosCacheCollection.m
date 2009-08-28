/*
 * KerberosCacheCollection.m
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#import "KerberosCacheCollection.h"
#import "KerberosCache.h"
#import "KerberosCredential.h"

#define kCacheCollectionUpdateTimerInterval 5

// ---------------------------------------------------------------------------

static int SortCaches (KerberosCache *cache1, KerberosCache *cache2, void *context)
{
    if ([cache1 lastDefaultTime] > [cache2 lastDefaultTime]) {
        // cache1 has been default more recently
        return NSOrderedAscending;
    } else if ([cache1 lastDefaultTime] < [cache2 lastDefaultTime]) {
        // cache2 has been default more recently
        return NSOrderedDescending;
    } else {
        // Neither has been default (usually)
        return NSOrderedSame;
    }
}

@implementation KerberosCacheCollection

// ---------------------------------------------------------------------------

+ (KerberosCacheCollection *) sharedCacheCollection
{
    static KerberosCacheCollection *sCacheCollection = NULL;
    
    if (!sCacheCollection) {
        sCacheCollection = [[KerberosCacheCollection alloc] init];
    }
    
    return sCacheCollection;
}

// ---------------------------------------------------------------------------

+ (void) waitForChange: (NSArray *) portArray
{
    NSAutoreleasePool *pool;
    NSConnection *connection;
    
    pool = [[NSAutoreleasePool alloc] init];    
    
    connection = [NSConnection connectionWithReceivePort: [portArray objectAtIndex: 0]
						sendPort: [portArray objectAtIndex: 1]];
    
    {
	cc_int32 err = ccNoError;
	cc_context_t context = NULL;
	
	err = cc_initialize (&context, ccapi_version_max, NULL, NULL);
	
	while (!err) {
	    // This call puts the thread to sleep
	    err = cc_context_wait_for_change (context);
            
	    if (!err) {
		dprintf ("%s thread noticed update", __FUNCTION__);
	    } else {
		dprintf ("%s thread got error %d (%s)", __FUNCTION__, err, error_message (err));
                err = 0; /* The server quit unexpectedly -- just try again */
            }

            [(KerberosCacheCollection *) [connection rootProxy] update];
	    sleep (1);
        }
	
	if (context) { cc_context_release (context); }
    }
    
    dprintf ("%s thread exiting", __FUNCTION__);
    [pool release];
}

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        cachesArray = NULL;
	threadConnection = NULL;

        if (![self update]) {
            [self release];
            return NULL;
        }
        
	[self update];
	
	NSPort *port1 = [NSPort port];
	NSPort *port2 = [NSPort port];
	
	threadConnection = [[NSConnection alloc] initWithReceivePort: port1
							    sendPort: port2];
	[threadConnection setRootObject: self];
		
	[NSThread detachNewThreadSelector: @selector(waitForChange:) 
				 toTarget: [KerberosCacheCollection class] 
			       withObject: [NSArray arrayWithObjects:port2, port1, NULL]];
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (cachesArray) { [cachesArray release]; }
    [threadConnection release];
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (BOOL) update
{
    cc_int32             err = ccNoError;
    NSMutableArray      *newCachesArray = NULL;
    cc_context_t         context = NULL;
    cc_ccache_iterator_t iterator = NULL;
    
    if (!err) {
	newCachesArray = [[NSMutableArray alloc] init];
	if (!newCachesArray) { err = ccErrNoMem; }
    }
    
    if (!err) {
	err = cc_initialize (&context, ccapi_version_4, NULL, NULL);
    }
    
    if (!err) {
	err = cc_context_new_ccache_iterator (context, &iterator);
    }
    
    while (!err) {
	cc_ccache_t    ccache = NULL;
	KerberosCache *cache = NULL;
	
	if (!err) {
	    err = cc_ccache_iterator_next (iterator, &ccache);
	}
	
	if (!err) {
	    cache = [self findCacheForCCache: ccache];
	    if (cache) {
		err = [cache update];
	    } else {
		cache = [[[KerberosCache alloc] initWithCCache: ccache] autorelease];
		if (cache) { 
                    // KerberosCache object takes ownership of ccache.
                    ccache = NULL; 
                } else {
                    err = ccErrInvalidCCache; 
                }
	    }
	}
	
	if (!err) {
	    [newCachesArray addObject: cache];
            
	} else if (err == ccErrInvalidCCache) {
            err = ccNoError; // CCache was destroyed while we were looking at it
        }
        
	if (ccache) { cc_ccache_release (ccache); }
    }
    
    if (err == ccIteratorEnd) {
	err = ccNoError;
    }
    
    if (!err) {
	[newCachesArray sortUsingFunction: SortCaches context: NULL];
	
	if (cachesArray) { [cachesArray release]; }
	cachesArray = newCachesArray;
	newCachesArray = NULL; // don't free
	
	dprintf ("Posting CacheCollectionDidChangeNotification...");
	// Post immediately!  Otherwise the objects will get out of sync with the table views
	[[NSNotificationCenter defaultCenter] postNotificationName: CacheCollectionDidChangeNotification 
							    object: self];
	
    }
    
    if (err) {
        dprintf ("update() got error %d (%s)", err, error_message (err)); 
    }

    if (newCachesArray) { [newCachesArray release]; }
    if (iterator      ) { cc_ccache_iterator_release (iterator); }
    if (context       ) { cc_context_release (context); }

    
    return !err;
}

// ---------------------------------------------------------------------------

- (KerberosCache *) findCacheForCCache: (cc_ccache_t) ccache
{
    unsigned int i = 0;
    
    for (i = 0; i < [cachesArray count]; i++) {
        KerberosCache *cache = [cachesArray objectAtIndex: i];
        if ([cache isEqualToCCache: ccache]) {
            return cache;
        }
    }
    
    return NULL;
}

// ---------------------------------------------------------------------------

- (KerberosCache *) findCacheForName: (NSString *) name
{
    KerberosCache *cache = NULL;
    cc_context_t context = NULL;
    cc_ccache_t ccache = NULL;
    cc_int32 err = cc_initialize (&context, ccapi_version_4, NULL, NULL);;
    
    if (!err) {
        err = cc_context_open_ccache (context, [name cString], &ccache);
    }
    
    if (!err) {
        cache = [self findCacheForCCache: ccache];
    }
        
    if (ccache ) { cc_ccache_release (ccache); }
    if (context) { cc_context_release (context); }
    
    return cache;
}

// ---------------------------------------------------------------------------

- (KerberosCache *) cacheAtIndex: (unsigned int) cacheIndex
{
    if (cacheIndex < [cachesArray count]) {
        return [cachesArray objectAtIndex: cacheIndex];
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (unsigned int) indexOfCache: (KerberosCache *) cache
{
    return [cachesArray indexOfObject: cache];
}

// ---------------------------------------------------------------------------

- (KerberosCache *) defaultCache
{
    unsigned int i = 0;
    
    for (i = 0; i < [cachesArray count]; i++) {
        KerberosCache *cache = [cachesArray objectAtIndex: i];
        if ([cache isDefault]) {
            return cache;
        }
    }
    
    return NULL;
}

// ---------------------------------------------------------------------------

- (NSArray *) caches
{
    return cachesArray;
}

// ---------------------------------------------------------------------------

- (int) numberOfCaches
{
    return [cachesArray count];
}

@end

