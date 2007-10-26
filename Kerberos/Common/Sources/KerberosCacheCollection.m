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

- (id) init
{
    if ((self = [super init])) {
        cachesArray = NULL;
        lastChangeTime = 0;
        updateTimer = NULL;

        if (![self update]) {
            [self release];
            return NULL;
        }
        
        updateTimer = [[TargetOwnedTimer scheduledTimerWithTimeInterval: kCacheCollectionUpdateTimerInterval
                                                                 target: self 
                                                               selector: @selector (updateTimer:) 
                                                               userInfo: NULL
                                                                repeats: YES] retain];
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (cachesArray) { [cachesArray release]; }
    if (updateTimer) { 
        [updateTimer invalidate];
        [updateTimer release]; 
    }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (void) updateTimer: (TargetOwnedTimer *) timer
{
    [self update];
}

// ---------------------------------------------------------------------------

- (BOOL) update
{
    BOOL updated = NO;
    cc_int32  err = ccNoError;
    cc_time_t newLastChangeTime = 0;
    
    if (!err) {
        err = KLLastChangedTime (&newLastChangeTime);
    }
    
    if (!err) {
        if (newLastChangeTime > lastChangeTime) {
            dprintf ("updateTimer got new change time %d", newLastChangeTime);

            NSMutableArray      *newCachesArray = NULL;
            cc_ccache_t          defaultCCache = NULL;
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
                err = cc_context_open_default_ccache (context, &defaultCCache);
                if (err != ccNoError) {
                    // No caches is okay
                    defaultCCache = NULL;
                    err = ccNoError;
                }
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
                        err = [cache synchronizeWithCCache: ccache 
                                             defaultCCache: defaultCCache];
                    } else {
                        cache = [[[KerberosCache alloc] initWithCCache: ccache 
                                                         defaultCCache: defaultCCache] autorelease];
                        if (!cache) { err = ccErrNoMem; }
                    }
                }
                
                if (!err) {
                    [newCachesArray addObject: cache];
                    cache = NULL;  // don't free
                    ccache = NULL; // KerberosCache object takes ownership of ccache
                }
                
                if (cache ) { [cache release]; }
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
                
                lastChangeTime = newLastChangeTime;
                
                dprintf ("Posting CacheCollectionDidChangeNotification...");
                // Post immediately!  Otherwise the objects will get out of sync with the table views
                [[NSNotificationCenter defaultCenter] postNotificationName: CacheCollectionDidChangeNotification 
                                                                    object: self];
                
                updated = YES;
            }
            
            if (newCachesArray) { [newCachesArray release]; }
            if (iterator      ) { cc_ccache_iterator_release (iterator); }
            if (defaultCCache ) { cc_ccache_release (defaultCCache); }
            if (context       ) { cc_context_release (context); }
        }
    }
    
    if (err != ccNoError) {
        dprintf ("updateTimer got error %d (%s)", err, error_message (err)); 
    }
    
    return updated;
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

