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

/*!
 * @header cache
 */

#ifndef __CCACHE_H__
#define __CCACHE_H__

#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libkern/OSAtomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <kvbuf.h>
#include <cache.h>
#include <cache_private.h>
#include <cache_callbacks.h>
#include <malloc/malloc.h>
#include <notify.h>
#include <DirectoryServiceCore/CLog.h>
#include <DirectoryServiceCore/DSUtils.h>

/*
 * Policy Flags
 *
 * CACHE_POLICY_REPLACE_ON_COLLISION
 *    causes existing entries to be replaced if a new entry is added for a unique key.
 * CACHE_POLICY_UPDATE_TTL_ON_HIT
 *    resets an entry's time to live if it is accessed by a cache fetch.
 */
#define CACHE_POLICY_REPLACE_ON_COLLISION 0x00000001
#define CACHE_POLICY_UPDATE_TTL_ON_HIT    0x00000002

/*
 * Additional flags for notifications, only use bit flags
 */
#define CACHE_ENTRY_TYPE_GROUP    0x00000001
#define CACHE_ENTRY_TYPE_HOST     0x00000002
#define CACHE_ENTRY_TYPE_SERVICE  0x00000004
#define CACHE_ENTRY_TYPE_USER     0x00000008
#define CACHE_ENTRY_TYPE_COMPUTER 0x00000010
#define CACHE_ENTRY_TYPE_MOUNT    0x00000020
#define CACHE_ENTRY_TYPE_ALIAS    0x00000040
#define CACHE_ENTRY_TYPE_PROTOCOL 0x00000080
#define CACHE_ENTRY_TYPE_RPC      0x00000100
#define CACHE_ENTRY_TYPE_NETWORK  0x00000200

// this means it was an extended entry
#define CACHE_ENTRY_TYPE_EXTENDED 0x00001000

#define CACHE_ENTRY_TYPE_REPLACE  0x40000000
#define CACHE_ENTRY_TYPE_NEGATIVE 0x80000000
#define CACHE_ENTRY_TYPE_ALL	  0xffffffff

/*
 * Validation token is the node name with a token.
 */
struct sCacheValidation
{
	CFStringRef         fNode;  // globally retain, do no release
	uint32_t            fToken;
	bool                fNodeAvailable;
	volatile int32_t    fRefCount;
    
                                sCacheValidation( const char *inNode );
    
#if USE_PURGEABLE_FOR_SMALL_ALLOCATIONS
    static  void *              operator new    ( size_t inLength ) { return malloc_zone_malloc( malloc_default_purgeable_zone(), inLength ); }
    static  void                operator delete ( void *inPtr ) { if ( inPtr != NULL ) malloc_zone_free( malloc_default_purgeable_zone(), inPtr ); }
#endif
    
    inline sCacheValidation *   Retain          ( void ) { return (sCacheValidation *) dsRetainObject( this, &fRefCount ); }
    inline void                 Release         ( void ) { if ( dsReleaseObject(this, &fRefCount, false) ) delete this; }
            
    inline  bool                IsValid         ( void ) { return (fToken == GetToken()); }
    
private:
            uint32_t            GetToken( void );
    
    inline                      ~sCacheValidation( void ) {
        DSCFRelease( fNode );
        DbgLog( kLogDebug, "CCache::CacheEntry:sCacheValidation:delete entry %X", this );
    }
};

struct sCacheEntry
{
	sCacheValidation	*fValidation;
	int32_t             fTTL;
	time_t				fBestBefore;
    time_t				fLastAccess;
	uint32_t			fHits;
	uint32_t			fFlags;
	kvbuf_t				*fBuffer;
    volatile int32_t    fRefCount;
    
    // Functions
                        sCacheEntry     ( int32_t inTTL, time_t inTimeStamp, uint32_t inFlags, kvbuf_t *inBuffer );

#if USE_PURGEABLE_FOR_SMALL_ALLOCATIONS
    static void *       operator new( size_t inLength ) { return malloc_zone_malloc( malloc_default_purgeable_zone(), inLength ); }
    static void         operator delete( void *inPtr ) { if ( inPtr != NULL ) malloc_zone_free( malloc_default_purgeable_zone(), inPtr ); }
#endif
    
    bool                Validate        ( time_t inNow );
    inline sCacheEntry *Retain          ( void ) { return (sCacheEntry *) dsRetainObject(this, &fRefCount); }
    inline void         Release         ( void ) { 
        if ( dsReleaseObject(this, &fRefCount, false) ) {
            delete this;
        }
    }
    
    static bool         MakeValueNonPurgeable( void *value, void *user_data );
    static void         MakeValuePurgeable( void *value, void *user_data );
    
    inline void         AddValidation   ( sCacheValidation *inValidation )
    {
        if ( this != NULL && inValidation != NULL )
        {
            // retain before we release in case we are replacing with the same value
            inValidation->Retain();
            DSRelease( fValidation );
            fValidation = inValidation;
        } 
    }
    
private:
    // destructor is private, retain release only
                ~sCacheEntry    ( void );
};

class CCache
{
    public:
        DSMutexSemaphore    fCacheLock; // we maintain a lock due to our use of collision handling
        uint32_t            fPolicyFlags;
        int32_t             fCacheTTL;
    
    public:
                    CCache              ( int32_t inTTL, uint32_t inPolicyFlags );
                    ~CCache             ( void );
    
        sCacheEntry *CreateEntry           ( kvbuf_t *inBuffer, char *inKey, int32_t inTTL, uint32_t inFlags );
        void        RemoveEntry         ( sCacheEntry *inEntry );
        void        ReleaseEntry        ( sCacheEntry *inEntry );
    
        bool        AddKeyToEntry       ( sCacheEntry *inEntry, char *inKey, bool inUnique );
        void        RemoveKey           ( char *inKey ) { cache_remove( fCache, inKey ); }
	
        kvbuf_t     *Fetch              ( char *inKey, int32_t *outLowestTTL = NULL, uint32_t reqFlags = 0 );
        int         Sweep               ( uint32_t inEntryType, bool inCheckDate );

        void        Flush               ( void ) { cache_remove_all( fCache ); notify_post( "com.apple.system.DirectoryService.InvalidateCache" ); };
    
        int         UpdateAvailability  ( char *inNode, bool inState );
        void        ApplyFunction       ( cache_invoke_fn_t fn, void *user_data ) { Sweep(CACHE_ENTRY_TYPE_ALL, true); cache_invoke(fCache, fn, user_data); }
    
    private:
        cache_t     *fCache;
	
    private:
        void        DoNotifies          ( uint32_t inFlags );
        bool        RemoveCollision     ( char *inKey );
    
        static void CacheEntryRelease   ( void *inValue, void *user_data );
        static void KeyRetain           ( void *key_in, void **key_out, void *user_data );
};

#endif /* __CCACHE_H__ */
