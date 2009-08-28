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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <notify.h>
#include <kvbuf.h>
#include <syslog.h>
#include <assert.h>
#include <stack>
#include <DirectoryServiceCore/DSSemaphore.h>
#include <DirectoryServiceCore/CLog.h>
#include "CCache.h"
#include "CPlugInList.h"

extern	CPlugInList		*gPlugins;

using namespace std;

#pragma mark sCacheValidation structure functions

sCacheValidation::sCacheValidation( const char *inNode )
{
    static CFMutableSetRef  nodeNameSet  = NULL;
    static DSSemaphore      nodeNameSetLock;
    
    nodeNameSetLock.WaitLock();
    
    if ( nodeNameSet == NULL )
        nodeNameSet = CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );
        
    CFStringRef nodeName = CFStringCreateWithCString( kCFAllocatorDefault, inNode, kCFStringEncodingUTF8 );
    if ( nodeName != NULL )
    {
        CFStringRef tempName = (CFStringRef) CFSetGetValue( nodeNameSet, nodeName );
        if ( tempName != NULL ) {
            fNode = (CFStringRef) CFRetain( tempName );
        } else {
            CFSetSetValue( nodeNameSet, nodeName );
            fNode = (CFStringRef) CFRetain( nodeName );
        }
        
        DSCFRelease( nodeName );
    }
    
    nodeNameSetLock.SignalLock();
    
    fToken = GetToken();
    fNodeAvailable = true;
    fRefCount = 1;
}

uint32_t sCacheValidation::GetToken( void )
{
    char        tempNode[512]   = { 0, };   // node shouldn't be that long
    char        *nodeName;
    uint32_t    iToken          = 0;
    
    CFStringGetCString( fNode, tempNode, sizeof(tempNode), kCFStringEncodingUTF8 );
    
    // if we have a token and we can validate the stamp use it
    nodeName = strtok( tempNode, "/" );
    
    if ( nodeName != NULL )
        iToken = gPlugins->GetValidDataStamp( nodeName );
    
    return iToken;
}

#pragma mark -
#pragma mark sCacheEntry structure

sCacheEntry::sCacheEntry( int32_t inTTL, time_t inTimeStamp, uint32_t inFlags, kvbuf_t *inBuffer )
{
    fValidation = NULL;
    fTTL = inTTL;
    fBestBefore = inTimeStamp + inTTL;
    fLastAccess = inTimeStamp;
    fHits = 0;
    fRefCount = 1;
    fFlags = inFlags;
    fBuffer = inBuffer;
}

sCacheEntry::~sCacheEntry( void )
{
    DbgLog( kLogDebug, "CCache::CacheEntry:delete entry %X", this );
    DSRelease( fValidation );
    if ( fBuffer != NULL )
    {
        kvbuf_free( fBuffer );
        fBuffer = NULL;
    }
}

bool sCacheEntry::Validate( time_t inNow )
{
	bool	outValid    = false;
    
    if ( this != NULL )
    {
        // if we have validation data, then we use it, otherwise we use the dates
        if ( fValidation != NULL )
        {
            // if the node is available
            if ( fValidation->fNodeAvailable == false || 
                 (fValidation->IsValid() == true && (fBestBefore == 0 || inNow < fBestBefore)) )
            {
                outValid = true;
            }
        }
        else if ( fBestBefore == 0 || inNow < fBestBefore )
        {
            outValid = true;
        }
    }
	
	return outValid;
}

bool sCacheEntry::MakeValueNonPurgeable( void *value, void *user_data )
{
    sCacheEntry *entry = (sCacheEntry *) value;
    
    if ( entry == NULL || entry->fBuffer == NULL )
        return true;
    
#if USE_PURGEABLE_FOR_SMALL_ALLOCATIONS
    if ( malloc_make_nonpurgeable(entry) != 0 )
        return false;
#endif
    
    if ( kvbuf_make_nonpurgeable(entry->fBuffer) == 0 )
    {
        DbgLog( kLogDebug, "CCache::CacheEntry::MakeValueNonPurgeable for %X (buffer %X) - SUCCESS", entry, entry->fBuffer );
        return true;
    }
    
#if USE_PURGEABLE_FOR_SMALL_ALLOCATIONS
    malloc_make_purgeable( entry );
#endif

    DbgLog( kLogDebug, "CCache::CacheEntry::MakeValueNonPurgeable for %X (buffer %X) - FAIL", entry, entry->fBuffer );
    return false;
}

void sCacheEntry::MakeValuePurgeable( void *value, void *user_data )
{
    sCacheEntry *entry = (sCacheEntry *) value;
    
    if ( entry != NULL && entry->fBuffer != NULL ) {
        DbgLog( kLogDebug, "CCache::CacheEntry::MakeValuePurgeable for %X (buffer %X)", entry, entry->fBuffer );
        kvbuf_make_purgeable( entry->fBuffer );
    }

#if USE_PURGEABLE_FOR_SMALL_ALLOCATIONS
    malloc_make_purgeable( entry );
#endif
}

#pragma mark -
#pragma mark Cache public routines

CCache::CCache( int32_t inTTL, uint32_t inPolicyFlags ) : fCacheLock("CCache::fCacheLock")
{
	fCacheTTL = inTTL;
	fPolicyFlags = inPolicyFlags;
    fCache = NULL;
    
    cache_attributes_t attrs = {
        version                     : CACHE_ATTRIBUTES_VERSION_1,
        key_hash_cb                 : cache_key_hash_cb_cstring,
        key_is_equal_cb             : cache_key_is_equal_cb_cstring,
        
        key_retain_cb               : CCache::KeyRetain,
        key_release_cb              : cache_release_cb_free,
        
        value_release_cb            : CCache::CacheEntryRelease,
        
        value_make_nonpurgeable_cb  : sCacheEntry::MakeValueNonPurgeable, 
        value_make_purgeable_cb     : sCacheEntry::MakeValuePurgeable,
        
        user_data : NULL
    };
    
    cache_create( "com.apple.DirectoryService.CCache", &attrs, &fCache );
}

CCache::~CCache( void )
{
	Flush();
}

sCacheEntry *CCache::CreateEntry( kvbuf_t *inBuffer, char *inKey, int32_t inTTL, uint32_t inFlags )
{
	sCacheEntry	*out	= NULL;
	
	// we don't insert anything without a key, no point
	if ( this != NULL && inKey != NULL )
	{
        bool bAddEntry  = false;
        
        fCacheLock.WaitLock();
        
        // if this is an extended entry replace any existing
        if ( (inFlags & CACHE_ENTRY_TYPE_REPLACE) != 0 || (inFlags & CACHE_ENTRY_TYPE_EXTENDED) != 0 ) {
            cache_remove( fCache, inKey ); // force remove the key
            bAddEntry = true;
        } else {
            bAddEntry = RemoveCollision( inKey );
        }
        
		if ( bAddEntry == true && inTTL > 0 )
		{
			// if TTL provided is larger than our default, cap it to the default
			if ( inTTL > fCacheTTL )
				inTTL = fCacheTTL;
            
            out = new sCacheEntry( inTTL, time(NULL), inFlags, inBuffer );
            assert( out != NULL );
            
            if ( cache_set_and_retain(fCache, inKey, out, (inBuffer != NULL ? inBuffer->datalen : 0)) == 0 ) {
                DbgLog( kLogDebug, "CCache::CreateEntry succeeded for %X", out );
            }
            else {
                DbgLog( kLogError, "CCache::CreateEntry failed for %X", out );
                DSRelease( out );
            }
		}
        
        fCacheLock.SignalLock();
	}
	
	return out;
}

void CCache::ReleaseEntry( sCacheEntry *inEntry )
{
    if ( inEntry != NULL )
    {
        DbgLog( kLogDebug, "CCache::ReleaseEntry called for %X", inEntry );
        
        // we hold our lock since we do other things out-of-band from the cache
        fCacheLock.WaitLock();
        cache_release_value( fCache, inEntry );
        fCacheLock.SignalLock();
    }
}

bool CCache::AddKeyToEntry( sCacheEntry *inEntry, char *inKey, bool inUnique )
{
	bool	bOut	= false;
	
	if ( this != NULL && inEntry != NULL && inKey != NULL)
	{
        bool bAddEntry  = false;
        
        fCacheLock.WaitLock();
        
        if ( (inEntry->fFlags & CACHE_ENTRY_TYPE_REPLACE) != 0 || (inEntry->fFlags & CACHE_ENTRY_TYPE_EXTENDED) != 0 ) {
            cache_remove( fCache, inKey ); // force remove the key
            bAddEntry = true;
        } else if ( inUnique == true ) {
            bAddEntry = RemoveCollision( inKey );
        }
        
		if ( bAddEntry == true || inUnique == false )
        {
            if ( cache_set_and_retain(fCache, inKey, inEntry, (inEntry->fBuffer != NULL ? inEntry->fBuffer->datalen : 0)) == 0 ) {
                cache_release_value( fCache, inEntry );
            }
        }

        fCacheLock.SignalLock();
	}
	
	return bOut;
}

kvbuf_t *CCache::Fetch( char *inKey, int32_t *outLowestTTL, uint32_t reqFlags )
{
    kvbuf_t     *out        = NULL;
    uint32_t    expireFlags = 0;     // track expiring entries for notifications

	if ( this != NULL )
	{
        sCacheEntry *entry      = NULL;
        time_t      now         = time( NULL );
        int32_t     lowestTTL   = fCacheTTL; // start with the max since it's the highest it can be

        fCacheLock.WaitLock();
        
        if ( inKey != NULL )
        {
            if ( cache_get_and_retain(fCache, inKey, (void **)&entry) == 0 )
            {
                assert( entry != NULL );
                
                // check if ttl has expired or if invalid
                if ( entry->Validate(now) == true )
                {
                    if ((fPolicyFlags & CACHE_POLICY_UPDATE_TTL_ON_HIT) != 0 && (entry->fTTL != 0))
                        entry->fBestBefore = now + entry->fTTL;
                    
                    // if the caller requests a specific flag, check it
                    if ( reqFlags == 0 || (entry->fFlags & reqFlags) != 0 ) {
                        entry->fHits++;
                        entry->fLastAccess = now;
                        
                        out = kvbuf_new();
                        kvbuf_append_kvbuf( out, entry->fBuffer );
                        
                        if ( entry->fTTL < lowestTTL )
                            lowestTTL = entry->fTTL;
                    }
                    
                    cache_release_value( fCache, entry );
                }
                else 
                {
                    expireFlags |= entry->fFlags;
                    cache_release_value( fCache, entry );
                    cache_remove( fCache, inKey );
                }
            }
        }
        
        if ( outLowestTTL != NULL )
            (*outLowestTTL) = lowestTTL;
                
        DoNotifies( expireFlags );			
        
        fCacheLock.SignalLock();
	}
	
	return out;
}

struct sSweepInvoke
{
    stack<void *>       *fRemoveList;
    uint32_t            fEntryType;
    bool                fCheckDate;
    int                 fCount;
    uint32_t            fExpireFlags;
    time_t              fNow;
};

static void SweepInvoke( void *key, void *value, void *user_data )
{
    sSweepInvoke    *context    = (sSweepInvoke *) user_data;
    sCacheEntry     *freeItem   = (sCacheEntry *) value;

    if ( freeItem->fFlags == 0 || (freeItem->fFlags & context->fEntryType) != 0 )
    {
        // if we aren't checking date or now is greater than the best_before date
        // we bypass validation if this is a host entry for sweeps because lookups from a directory
        // are invalid for hosts when they are offline
        if ( ((freeItem->fFlags & CACHE_ENTRY_TYPE_HOST) != 0 || freeItem->fValidation == NULL || freeItem->fValidation->fNodeAvailable) && 
             (context->fCheckDate == false || context->fNow >= freeItem->fBestBefore) )
        {
            context->fExpireFlags |= freeItem->fFlags;
            context->fRemoveList->push( key );
            context->fCount++;
        }
    }
}

int CCache::Sweep( uint32_t inEntryType, bool inCheckDate )
{
	int			iCount		= 0;
	
	if ( this != NULL ) {
        stack<void *>   removeList;
        sSweepInvoke    context =   {
                                        fRemoveList     : &removeList,
                                        fEntryType      : inEntryType,
                                        fCheckDate      : inCheckDate,
                                        fCount          : 0,
                                        fExpireFlags    : 0,
                                        fNow            : time( NULL ),
                                    };

        fCacheLock.WaitLock();

        cache_invoke( fCache, SweepInvoke, &context );
        
        // we can't remove entries while we are applying so we track them
        while ( removeList.size() )
        {
            cache_remove( fCache, removeList.top() );
            removeList.pop();
        }
        
        DoNotifies( context.fExpireFlags );
        
        fCacheLock.SignalLock();

        iCount = context.fCount;
    }
    
	return iCount;
}

struct sAvail
{
    CFStringRef fNode;
    bool        fNewState;
    int         fCount;
};

static void InvokeUpdateAvail( void *key, void *value, void *user_data )
{
    sCacheEntry *entry      = (sCacheEntry *) value;
    sAvail      *context    = (sAvail *) user_data;
    
    
    // if the node has validation data, and the state has changed, update it
    sCacheValidation *valid_t = entry->fValidation;
    if (valid_t != NULL && valid_t->fNode != NULL && context->fNewState != valid_t->fNodeAvailable && 
        CFStringCompare(valid_t->fNode, context->fNode, 0) == kCFCompareEqualTo)
    {
        valid_t->fNodeAvailable = context->fNewState;
        context->fCount++;
    }
}

int CCache::UpdateAvailability( char *inNode, bool inState )
{
	int		iCount = 0;
	
	if ( this != NULL && inNode != NULL)
	{
        sAvail  context = {
            fNode : CFStringCreateWithCString( kCFAllocatorDefault, inNode, kCFStringEncodingUTF8 ),
            fNewState : inState,
            fCount : 0
        };
        
        fCacheLock.WaitLock();

        cache_invoke( fCache, InvokeUpdateAvail, &context );
        iCount = context.fCount;
        DSCFRelease( context.fNode );
        
        fCacheLock.SignalLock();
	}
	
	return iCount;
}

#pragma mark -
#pragma mark Private methods

void CCache::DoNotifies( uint32_t inFlags )
{
	if( (inFlags & CACHE_ENTRY_TYPE_GROUP) == CACHE_ENTRY_TYPE_GROUP )
        notify_post( "com.apple.system.DirectoryService.InvalidateCache.group" );
	
    if( (inFlags & CACHE_ENTRY_TYPE_HOST) == CACHE_ENTRY_TYPE_HOST )
        notify_post( "com.apple.system.DirectoryService.InvalidateCache.host" );
	
    if( (inFlags & CACHE_ENTRY_TYPE_SERVICE) == CACHE_ENTRY_TYPE_SERVICE )
        notify_post( "com.apple.system.DirectoryService.InvalidateCache.service" );
    
    if( (inFlags & CACHE_ENTRY_TYPE_USER) == CACHE_ENTRY_TYPE_USER )
        notify_post( "com.apple.system.DirectoryService.InvalidateCache.user" );
}

bool CCache::RemoveCollision( char *inKey )
{
	bool	bOut    = true;

	if ( this != NULL && inKey != NULL )
	{
        sCacheEntry *entry  = NULL;
        
        if ( cache_get_and_retain(fCache, inKey, (void **)&entry) == 0 )
        {
            cache_release_value( fCache, entry );
            if ( (fPolicyFlags & CACHE_POLICY_REPLACE_ON_COLLISION) != 0 ) {
                cache_remove( fCache, inKey );
            } else {
                bOut = false;
            }
        }
	}
	
	return bOut;
}

void CCache::KeyRetain( void *key_in, void **key_out, void *user_data )
{
    *key_out = strdup( (char *) key_in );
}

void CCache::CacheEntryRelease( void *inValue, void *user_data )
{
    sCacheEntry *entry = (sCacheEntry *) inValue;
    
    DSRelease( entry );
}
