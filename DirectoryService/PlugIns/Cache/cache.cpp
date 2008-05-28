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
#include "cache.h"
#include "CPlugInList.h"

extern	CPlugInList		*gPlugins;

#pragma mark sCacheValidation structure functions

sCacheValidation::sCacheValidation( const char *inNode )
{
    fNode = strdup( inNode );
    fToken = GetToken();
    fNodeAvailable = true;
    fRefCount = 1;
    fSpinLock = OS_SPINLOCK_INIT;
}

sCacheValidation *sCacheValidation::Retain( void )
{
    if ( this != NULL )
    {
        OSSpinLockLock( &fSpinLock );
        fRefCount++;
        OSSpinLockUnlock( &fSpinLock );
    }

    return this;
}

void sCacheValidation::Release( void )
{
    bool    bDelete = false;
    
    if ( this != NULL )
    {
        OSSpinLockLock( &fSpinLock );
        if ( (--fRefCount) <= 0 )
            bDelete = true;
        OSSpinLockUnlock( &fSpinLock );
    }
    
    if ( bDelete )
        delete this;
}

uint32_t sCacheValidation::GetToken( void )
{
    char        tempNode[512]   = { 0, };   // node shouldn't be that long
    char        *nodeName;
    uint32_t    iToken          = 0;
    
    strlcpy( tempNode, fNode, sizeof(tempNode) );
    
    // if we have a token and we can validate the stamp use it
    nodeName = strtok( tempNode, "/" );
    
    if ( nodeName != NULL )
        iToken = gPlugins->GetValidDataStamp( nodeName );
    
    return iToken;
}

#pragma mark -
#pragma mark sKeyList structure functions

sKeyList::sKeyList( void )
{
    fCount = 0;
    fHead = NULL;
}

sKeyList::~sKeyList( void )
{
    while( fHead != NULL )
    {
        sKeyListItem *delItem = fHead;
        
        fHead = fHead->fNext;
        
        delete delItem;
        fCount--;
    }
}

bool sKeyList::AddKey( char *inKey )
{
    sKeyListItem *tempItem = new sKeyListItem( fHead, inKey );
    if ( tempItem == NULL )
        return false;
    
    fHead = tempItem;
    fCount++;
    
    return true;
}

void sKeyList::DeleteKey( const char *inKey )
{
    sKeyListItem *tempItem = fHead;
    sKeyListItem *prevItem = NULL;
    
    while ( tempItem != NULL )
    {
        if ( strcmp(inKey, tempItem->fKey) == 0 )
        {
            if ( prevItem != NULL )
                prevItem->fNext = tempItem->fNext;
            else
                fHead = tempItem->fNext;
            
            delete tempItem;
            tempItem = NULL;
            
            fCount--;
        }
        else
        {
            prevItem = tempItem;
            tempItem = tempItem->fNext;
        }
    }
}

#pragma mark -
#pragma mark sEntryList structure functions

sEntryList::sEntryList( void )
{
	fHead = NULL;
	fTail = NULL;
}

sEntryList::~sEntryList( void )
{
	while ( fHead != NULL )
	{
		sEntryListItem *delItem = fHead;
		
		fHead = delItem->fNext;
		
		delete delItem;
	}
	
	fHead = NULL;
	fTail = NULL;
}

void sEntryList::AddEntry( sCacheEntry *inEntry )
{
	if ( fTail != NULL )
	{
		fTail->fNext = new sEntryListItem( inEntry );
		fTail = fTail->fNext;
	}
	else
	{
		fHead = new sEntryListItem( inEntry );
		fTail = fHead;
	}
}

#pragma mark -
#pragma mark sBucketList structure functions

sBucketList::sBucketList( void )
{
    fCount = 0;
    fHead = NULL;
}

sBucketList::~sBucketList( void )
{
    while ( fHead != NULL )
    {
        sBucketListItem *delItem = fHead;
        
        fHead = fHead->fNext;
        
        delete delItem;
        fCount--;
    }
}

bool sBucketList::AddItem( const char *inKey, sCacheEntry *inEntry )
{
    sBucketListItem *newItem = new sBucketListItem( fHead, inKey, inEntry );
    if ( newItem == NULL )
        return false;
    
    fHead = newItem;
    fCount++;
    
    return true;
}

void sBucketList::DeleteItem( const char *inKey, sCacheEntry *inEntry )
{
    sBucketListItem *item       = fHead;
    sBucketListItem *prevItem   = NULL;
    
    while ( item != NULL )
    {
        if ( item->fKey == inKey && item->fEntry == inEntry )
        {
            if ( prevItem != NULL )
                prevItem->fNext = item->fNext;
            else
                fHead = item->fNext;
            
            delete item;
            item = NULL;
            fCount--;
        }
        else
        {
            item = item->fNext;
        }
    }
}

#pragma mark -
#pragma mark sCacheEntry structure

sCacheEntry::sCacheEntry( int32_t inTTL, time_t inTimeStamp, uint32_t inFlags, kvbuf_t *inBuffer )
{
    fNext = fPrev = NULL;
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
    Isolate();
    fValidation->Release();
    fValidation = NULL;
    kvbuf_free( fBuffer );
    fBuffer = NULL;
}

void sCacheEntry::Isolate( void )
{
    if ( fPrev != NULL )
        fPrev->fNext = fNext;
    
    if ( fNext != NULL )
        fNext->fPrev = fPrev;
    
    fPrev = NULL;
    fNext = NULL;
}

void sCacheEntry::InsertAfter( sCacheEntry *inAfter )
{
    if ( inAfter != NULL )
    {
		Isolate();

        // first link this item to the existing items
        fPrev = inAfter;
        fNext = inAfter->fNext;
        
        // now link the others to this one
        inAfter->fNext = this;
        if ( fNext != NULL )
            fNext->fPrev = this;
    }
}

void sCacheEntry::InsertBefore( sCacheEntry *inBefore )
{
	if ( inBefore != NULL )
    {
		Isolate();

        // first link this item to the existing items
        fPrev = inBefore->fPrev;
        fNext = inBefore;
        
        // now link the others to this one
        inBefore->fPrev = this;
        if ( fPrev != NULL )
            fPrev->fNext = this;
    }
}

// returns true to move it up, false to stop
bool sCacheEntry::CompareWith( sCacheEntry *inEntry, time_t inMRAWindow )
{
	int returnValue = true; // default to moving up
	
	// if the one being compared is outside the window of check, then
	// it moves above the current as well
	time_t windowEnd = fLastAccess + inMRAWindow;
	time_t lastAccess = inEntry->fLastAccess;
	
	// if it is within our window time and our hit count is less than the one above, we stay
	if (inEntry->fHits < fHits && lastAccess <= windowEnd)
		returnValue = false;
	
	return returnValue;
}

bool sCacheEntry::CompareAllKeys( sKeyList *inKeys )
{
	bool	returnValue	= false;
	
	if ( this != NULL && inKeys != NULL)
	{
		// loop over keys and search for 
		sKeyListItem *keyitem = inKeys->fHead;
		while ( keyitem != NULL )
		{
			sKeyListItem *entrykey = fKeyList.fHead;
			while ( entrykey != NULL )
			{
				if ( strcmp(keyitem->fKey, entrykey->fKey) == 0 )
				{
                    entrykey = NULL;
					returnValue = true;
				}
                else
                {
                    entrykey = entrykey->fNext;
                }
			}
            
			// if we found it, let's check the next one
            if ( returnValue == true )
                keyitem = keyitem->fNext;
            else
                keyitem = NULL;
		}
	}
	
	return returnValue;
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

#pragma mark -
#pragma mark Cache Entry public routines

CCache::CCache( uint32_t inMaxSize, uint32_t inBuckets, time_t inMRAWindow, int32_t inTTL, uint32_t inPolicyFlags ) : fCacheLock("CCache::fCacheLock")
{
	fHead = fTail = NULL;
	fBucketCount = inBuckets;
	fCacheTTL = inTTL;
	fMRAWindow = inMRAWindow;
	fMaxSize = inMaxSize;
	fPolicyFlags = inPolicyFlags;
	fBuckets = (sBucketList **) calloc( inBuckets, sizeof(sBucketList *) );
}

CCache::~CCache( void )
{
	Flush();
	free( fBuckets );
}

sCacheEntry *CCache::AddEntry( kvbuf_t *inBuffer, const char *inKey, int32_t inTTL, uint32_t inFlags )
{
	sCacheEntry	*out	= NULL;
	
	// we don't insert anything without a key, no point
	if ( this != NULL && inKey != NULL )
	{
        fCacheLock.WaitLock();
        
        if ( (inFlags & CACHE_ENTRY_TYPE_REPLACE) == CACHE_ENTRY_TYPE_REPLACE )
            RemoveKey( inKey ); // force remove the key
        
        // remove the key, but also only cache if TTL > 0
		if ( RemoveCollision(inKey) == true && inTTL > 0 )
		{
			// if TTL provided is larger than our default, cap it to the default
			if ( inTTL > fCacheTTL )
				inTTL = fCacheTTL;

			out = new sCacheEntry( inTTL, time(NULL), inFlags, inBuffer );
			if (out != NULL)
			{
				if ( AddKeyToEntry(out, inKey, false) == true )
				{
					InsertEntryAfter( fTail, out );
				}
				else
				{
					out->Release();
					out = NULL;
				}
			}
		}
        
        fCacheLock.SignalLock();
	}
	
	return out;
}

void CCache::RemoveEntry( sCacheEntry *inEntry )
{
	if ( this != NULL && inEntry != NULL )
	{
        fCacheLock.WaitLock();

		IsolateEntry( inEntry );
		
		for (uint32_t b = 0; b < fBucketCount; b++)
		{
			if ( fBuckets[b] != NULL )
			{
				sBucketListItem *item = fBuckets[b]->fHead;
				sBucketListItem *prevItem = NULL;
				
				while ( item != NULL )
				{
					if ( item->fEntry == inEntry )
					{
						if ( prevItem != NULL )
							prevItem->fNext = item->fNext;
						else
							fBuckets[b]->fHead = item->fNext;
						
						delete item;
						item = NULL;
						
						fBuckets[b]->fCount--;
					}
					else
					{
						prevItem = item;
						item = item->fNext;
					}
				}
			}
			
			// if we have nothing, lets remove the list
			if (fBuckets[b] != NULL && fBuckets[b]->fCount == 0)
			{
				delete fBuckets[b];
				fBuckets[b] = NULL;
			}
		}

		inEntry->Release();
        
        fCacheLock.SignalLock();
	}
}

bool CCache::AddKeyToEntry( sCacheEntry *inEntry, const char *inKey, bool inUnique )
{
	bool	bOut	= false;
	
	if ( this != NULL && inEntry != NULL && inKey != NULL)
	{
        fCacheLock.WaitLock();

        // if entry has CACHE_ENTRY_TYPE_REPLACE we are free to replace it
        if ( (inEntry->fFlags & CACHE_ENTRY_TYPE_REPLACE) == CACHE_ENTRY_TYPE_REPLACE )
            RemoveKey( inKey );

		if ( inUnique == false || RemoveCollision(inKey) == true )
        {
            int bucket = HashKey( inKey );
            if ( fBuckets[bucket] == NULL )
                fBuckets[bucket] = new sBucketList;
            
            if ( fBuckets[bucket] != NULL )
            {
                char *newKey = strdup( inKey );
                
                if ( fBuckets[bucket]->AddItem(newKey, inEntry) == true )
                {
                    if ( inEntry->fKeyList.AddKey(newKey) == true )
                        bOut = true;
                    else
                        fBuckets[bucket]->DeleteItem( newKey, inEntry );
                }
                else
                {
                    free( newKey );
                    newKey = NULL;
                }
            }
        }

        fCacheLock.SignalLock();
	}
	
	return bOut;
}

void CCache::RemoveKey( const char *inKey )
{
    if ( this != NULL && inKey != NULL )
    {
        fCacheLock.WaitLock();
        
        sBucketList *bucketList = fBuckets[ HashKey(inKey) ];
        if ( bucketList != NULL )
        {
            sBucketListItem *item		= bucketList->fHead;
            sBucketListItem *prevItem	= NULL;
            while ( item != NULL )
            {
                if ( strcmp(inKey, item->fKey) == 0 )
                {
                    if ( prevItem != NULL )
                        prevItem->fNext = item->fNext;
                    else
                        bucketList->fHead = item->fNext;
                    
                    item->fEntry->fKeyList.DeleteKey( inKey );
                    
                    // if no keys delete the entry
                    if ( item->fEntry->fKeyList.fCount == 0 )
                        RemoveEntry( item->fEntry );
                    
                    delete item;
                    item = NULL;
                    
                    bucketList->fCount--;                    
                }
                else
                {
                    prevItem = item;
                    item = item->fNext;
                }
            }
        }
        
        fCacheLock.SignalLock();
    }
}

kvbuf_t *CCache::Fetch( sKeyList *inKeys, bool inMatchAll, int32_t *outLowestTTL )
{
    kvbuf_t     *out    = NULL;
	
	if ( this != NULL )
	{
		sEntryList	*expired    = new sEntryList;
        time_t      now         = time(NULL);
        int32_t     lowestTTL   = fCacheTTL; // start with the max since it's the highest it can be

        fCacheLock.WaitLock();

        if ( inKeys != NULL )
        {
            sKeyListItem	*item	= inKeys->fHead;
            
            while ( item != NULL )
            {
                sCacheEntry *entry = FindEntry( item->fKey );
                if ( entry != NULL && (inMatchAll == false || entry->CompareAllKeys(inKeys) == true) )
                {
                    // check if ttl has expired or if invalid
                    if ( entry->Validate(now) == true )
                    {
                        if ((fPolicyFlags & CACHE_POLICY_UPDATE_TTL_ON_HIT) != 0 && (entry->fTTL != 0))
                            entry->fBestBefore = now + entry->fTTL;
                        
                        entry->fHits++;
                        entry->fLastAccess = now;
                        
                        InsertEntryAfter( ReorderEntry(entry), entry );
                        
                        if ( out == NULL )
                            out = kvbuf_new();
                        kvbuf_append_kvbuf( out, entry->fBuffer );
                        
                        if ( entry->fTTL < lowestTTL )
                            lowestTTL = entry->fTTL;
                    }
                    else 
                    {
                        expired->AddEntry( entry );
                    }
                }
                
                item = item->fNext;
            }
        }
        else
        {
            // no keys, they want everything...
            // note: this does not update the last access time on the
            //	     but it will expire entries
            out = kvbuf_new();
            sCacheEntry *entry = fHead;
            while ( entry != NULL )
            {
                if ( entry->Validate(now) == true )
                {
                    kvbuf_append_kvbuf( out, entry->fBuffer );

                    if ( entry->fTTL < lowestTTL )
                        lowestTTL = entry->fTTL;
                }
                else 
                    expired->AddEntry( entry );
                
                entry = entry->fNext;
            }
        }
        
        if ( outLowestTTL != NULL )
            (*outLowestTTL) = lowestTTL;
        
        // track expiring entries for notifications
        uint32_t expireFlags = 0;
        
        sEntryListItem *item = expired->fHead;
        while ( item != NULL )
        {
            expireFlags |= item->fEntry->fFlags;
            RemoveEntry( item->fEntry );
            
            item = item->fNext;
        }
        
        delete expired;
        
        DoNotifies( expireFlags );			
        
        fCacheLock.SignalLock();
	}
	
	return out;
}

void CCache::Flush( void )
{
	if ( this != NULL )
	{
        fCacheLock.WaitLock();

		// first empty our store
		sCacheEntry *entry = fHead;
		while (entry != NULL)
		{
			sCacheEntry *freeEntry = entry;
			entry = entry->fNext;
			freeEntry->Release();
		}
		
		// now get rid of our pointers
		fHead = NULL;
		fTail = NULL;

		for ( uint32_t ii = 0; ii < fBucketCount; ii++ )
		{
			if ( fBuckets[ii] != NULL )
			{
				delete fBuckets[ii];
				fBuckets[ii] = NULL;
			}
		}
        
        fCacheLock.SignalLock();

		notify_post( "com.apple.system.DirectoryService.InvalidateCache" );
	}
}

int CCache::Sweep( uint32_t inEntryType, bool inCheckDate )
{
	int			iCount		= 0;
    uint32_t	expireFlags = 0;
	
	if ( this != NULL )
	{
		time_t now = time( NULL );
		
        fCacheLock.WaitLock();

		sCacheEntry *entry = fHead;
		while ( entry != NULL )
		{
			sCacheEntry *freeItem = entry;
			
			entry = entry->fNext;
			if ( freeItem->fFlags == 0 || (freeItem->fFlags & inEntryType) != 0 )
			{
				// if we aren't checking date or now is greater than the best_before date
                // we bypass validation if this is a host entry for sweeps because lookups from a directory
                // are invalid for hosts when they are offline
				if ( ((freeItem->fFlags & CACHE_ENTRY_TYPE_HOST) != 0 || freeItem->fValidation == NULL || freeItem->fValidation->fNodeAvailable) && 
                     (inCheckDate == false || now >= freeItem->fBestBefore) )
				{
					expireFlags |= freeItem->fFlags;
					RemoveEntry( freeItem );
					iCount++;
				}
			}
		}
		
		// if we have memory pressure see what else we can remove
		int iNeedRemoval = GetCount() - fMaxSize;
		if (iNeedRemoval > 0)
		{
			sCacheEntry *iterEntry = fTail;
			while (iterEntry != NULL)
			{
				// if something hasn't been accessed in the last 5 minutes and has <= X hits, let's chop it off
				if (now < (iterEntry->fLastAccess + (5 * 60)) && iterEntry->fHits > 2 )
					break;
				
				sCacheEntry *freeItem = iterEntry;
				
				iterEntry = iterEntry->fPrev;
				expireFlags |= freeItem->fFlags;
				RemoveEntry( freeItem );
				iCount++;
			}
		}
		
        fCacheLock.SignalLock();

		DoNotifies( expireFlags );
	}
    
	return iCount;
}

int CCache::UpdateAvailability( char *iNode, bool inState )
{
	int		iCount = 0;
	
	if ( this != NULL && iNode != NULL)
	{
        fCacheLock.WaitLock();

		sCacheEntry *entry = fHead;
		while (entry != NULL)
		{
			// if the node has validation data, and the state has changed, update it
			sCacheValidation *valid_t = entry->fValidation;
			if (valid_t != NULL && valid_t->fNode != NULL && inState != valid_t->fNodeAvailable && 
				strcmp(valid_t->fNode, iNode) == 0)
			{
				valid_t->fNodeAvailable = inState;
				iCount++;
			}
			
			entry = entry->fNext;
		}
        
        fCacheLock.SignalLock();
	}
	
	return iCount;
}

uint32_t CCache::GetCount( void )
{
    uint32_t    iCount = 0;
    
    fCacheLock.WaitLock();

    for ( sCacheEntry *entry = fHead; entry != NULL; entry = entry->fNext )
    {
        iCount++;
    }

    fCacheLock.SignalLock();

    return iCount;
}

bool CCache::isCacheOverMax( void )
{
    bool    bOut    = false;
    
    fCacheLock.WaitLock();

    bOut = (GetCount() > fMaxSize);
    
    fCacheLock.SignalLock();
    
    return bOut;
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

bool CCache::RemoveCollision( const char *inKey )
{
	bool	bOut = true;

	if ( this != NULL && inKey != NULL )
	{
        fCacheLock.WaitLock();

		sBucketList *bucketList = fBuckets[ HashKey(inKey) ];
		if ( bucketList != NULL )
		{
			sBucketListItem *item		= bucketList->fHead;
			sBucketListItem *prevItem	= NULL;
			while ( item != NULL )
			{
				if ( strcmp(inKey, item->fKey) == 0 )
				{
					if ( (fPolicyFlags & CACHE_POLICY_REPLACE_ON_COLLISION) == CACHE_POLICY_REPLACE_ON_COLLISION )
					{
						if ( prevItem != NULL )
							prevItem->fNext = item->fNext;
						else
							bucketList->fHead = item->fNext;
						
						bucketList->fCount--;
						item->fEntry->fKeyList.DeleteKey( inKey );
                        
                        // if no keys left from the collision, delete the entry completely
                        if ( item->fEntry->fKeyList.fCount == 0 )
                            RemoveEntry( item->fEntry );
					}
					else
					{
						bOut = false;
					}
					
					item = NULL;
				}
				else
				{
					prevItem = item;
					item = item->fNext;
				}
			}
		}
        
        fCacheLock.SignalLock();
	}
	
	return bOut;
}

uint32_t CCache::HashKey( const char *inKey )
{
	uint32_t v = 0;
	
	if ( inKey != NULL )
	{
		for (const char *p = inKey; *p != '\0'; p++)
		{
			v = (v << 1) ^ (v ^ *p);
		}
		
		v %= fBucketCount;
	}
	
	return v;
}

void CCache::IsolateEntry( sCacheEntry *inEntry )
{
	if ( this != NULL && inEntry != NULL )
	{
		// if this was the tail, reset the tail
		if ( fTail == inEntry )
			fTail = inEntry->fPrev;
		
		// if this was the head, then point to the next entry
		if ( fHead == inEntry )
			fHead = inEntry->fNext;
		
		inEntry->Isolate();
	}
}

void CCache::InsertEntryAfter( sCacheEntry *inAfter, sCacheEntry *inEntry )
{
	// if entries are not identical and it isn't already at the head or it is not already the next
	if ( inAfter != inEntry && ((inAfter == NULL && fHead != inEntry) || (inAfter != NULL && inAfter->fNext != inEntry)) )
	{
		IsolateEntry( inEntry );
		
		// if we have an entry to insert it inAfter, do it
		if ( inAfter != NULL )
		{
			inEntry->InsertAfter( inAfter );
			
			if ( inEntry->fNext == NULL )
				fTail = inEntry; // we must be at the end
		}
		else
		{
			inEntry->InsertBefore( fHead );
			
			fHead = inEntry;
			if ( fTail == NULL )
				fTail = inEntry;
		}
	}
}

sCacheEntry	*CCache::ReorderEntry( sCacheEntry *inEntry )
{
	// if we have no pointers, then we start at the bottom, otherwise, we are moving up
	sCacheEntry *after = (inEntry->fPrev == NULL && inEntry->fNext == NULL ? fTail : inEntry->fPrev);
	while ( after != NULL )
	{
		if ( after->CompareWith(inEntry, fMRAWindow) == false )
			break;
		
		// we work backwards
		after = after->fPrev;
	}
	
	return after;
}

sCacheEntry	*CCache::FindEntry( const char *inKey )
{
	sCacheEntry	*outEntry = NULL;
	
	if ( this != NULL && inKey != NULL )
	{
		sBucketList *bucketList = fBuckets[ HashKey(inKey) ];
		if ( bucketList != NULL )
		{
			sBucketListItem *item = bucketList->fHead;
			while ( item != NULL )
			{
				if ( strcmp(inKey, item->fKey) == 0 )
				{
					outEntry = item->fEntry;
					item = NULL;
				}
				else
				{
					item = item->fNext;
				}
			}
		}
	}
	
	return outEntry;
}
