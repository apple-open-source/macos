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

#ifndef __CACHE_H__
#define __CACHE_H__

#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <libkern/OSAtomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <kvbuf.h>

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

// free entries

#define CACHE_ENTRY_TYPE_REPLACE  0x40000000
#define CACHE_ENTRY_TYPE_NEGATIVE 0x80000000
#define CACHE_ENTRY_TYPE_ALL	  0xffffffff

/*
 * Validation token is the node name with a token.
 */
struct sCacheValidation
{
	char            *fNode;
	uint32_t        fToken;
	bool            fNodeAvailable;
	int             fRefCount;
    OSSpinLock      fSpinLock;
    
                                sCacheValidation( const char *inNode );
    
    inline  bool                IsValid         ( void ) { return (fToken == GetToken()); }
            
            sCacheValidation    *Retain         ( void );
            void                Release         ( void );

private:
            uint32_t            GetToken( void );
    
    inline                      ~sCacheValidation( void )
    {
        free( fNode );
        fNode = NULL;
    }
};

/*
 * Keylist structure, don't use clist more efficient
 */
struct sKeyListItem
{
    sKeyListItem    *fNext;
    char			*fKey;

    inline sKeyListItem( sKeyListItem *inNext, char *inKey )
    {
        fNext = inNext;
        fKey = inKey;
    }
    
    inline ~sKeyListItem( void )
    {
		free( fKey );

        fNext = NULL;
        fKey = NULL;
    }
};

struct sKeyList
{
    uint32_t        fCount;
	sKeyListItem    *fHead;
    
    // functions for operating on a keylist
            sKeyList    ( void );
            ~sKeyList   ( void );
    
    bool    AddKey      ( char *inKey );
    void    DeleteKey   ( const char *inKey );
};

struct sCacheEntry
{
	sCacheEntry         *fPrev;
	sCacheEntry         *fNext;
	sKeyList            fKeyList;
	sCacheValidation	*fValidation;
	int32_t             fTTL;
	time_t				fBestBefore;
    time_t				fLastAccess;	// used to age out entries under memory pressure
	uint32_t			fHits;			// used to move the item up the store list when it is hit
	uint32_t			fRefCount;
	uint32_t			fFlags;
	kvbuf_t				*fBuffer;
    
    // Functions
                        sCacheEntry     ( int32_t inTTL, time_t inTimeStamp, uint32_t inFlags, kvbuf_t *inBuffer );
    
            void        Isolate         ( void );
            void        InsertAfter     ( sCacheEntry *inAfter );
			void		InsertBefore	( sCacheEntry *inBefore );
    
            bool        Validate        ( time_t inNow );
            bool        CompareWith     ( sCacheEntry *inComparison, time_t inMRAWindow );
			bool		CompareAllKeys	( sKeyList *inKeys );
    
    inline  bool        AddKey          ( char *inKey )
                        {
                            if ( this != NULL )
                                return fKeyList.AddKey( inKey );
                            else
                                return false;
                        }
    
    inline  void        AddValidation   ( sCacheValidation *inValidation )
						{
							if ( this != NULL && inValidation != NULL )
                            {
								fValidation->Release();
								fValidation = inValidation->Retain();
							} 
						}
    
    inline  sCacheEntry *Retain         ( void ) { if ( this != NULL) fRefCount++; return this; }
    inline  void        Release         ( void ) { if ( this != NULL && (--fRefCount) == 0 ) delete this; }
    
private:
    // destructor is private, retain release only
                        ~sCacheEntry    ( void );

};

struct sBucketListItem
{
    sBucketListItem     *fNext;  // next item in the list
    const char          *fKey;   // this points directly at the key in entry
    sCacheEntry         *fEntry; // the entry itself
    
    inline sBucketListItem( sBucketListItem *inNext, const char *inKey, sCacheEntry *inEntry )
    {
        fNext = inNext;
        fKey = inKey;
        fEntry = inEntry->Retain();
    }
    
    inline ~sBucketListItem( void )
    {
        fNext = NULL;
        fKey = NULL;    // we don't release this, it's pointing to the real thing
        fEntry->Release();
    }
};

struct sBucketList
{
	uint32_t        fCount;
	sBucketListItem *fHead;
    
    // Functions
            sBucketList     ( void );
            ~sBucketList    ( void );
    
    bool    AddItem         ( const char *inKey, sCacheEntry *inEntry );
    void    DeleteItem      ( const char *inKey, sCacheEntry *inEntry );
};

struct sEntryListItem
{
    sEntryListItem  *fNext;
    sCacheEntry     *fEntry;
	
	inline	sEntryListItem( sCacheEntry *inEntry ) { fNext = NULL; fEntry = inEntry->Retain(); }
	inline	~sEntryListItem( void ) { fNext = NULL; fEntry->Release(); fEntry = NULL; }
};

struct sEntryList
{
    sEntryListItem *fHead;
	sEntryListItem *fTail;
    
    // functions
			sEntryList		( void );
            ~sEntryList     ( void );
    
    void    AddEntry        ( sCacheEntry *inEntry );
};

class CCache
{
    public:
        // these should never be accessed directly without grabbing the cache lock
        sCacheEntry         *fHead;				// top of the list
        sCacheEntry         *fTail;
        DSMutexSemaphore    fCacheLock;
        uint32_t            fBucketCount;
        sBucketList         **fBuckets;
        int32_t             fCacheTTL;
        time_t              fMRAWindow;
        uint32_t            fMaxSize;
        uint32_t            fPolicyFlags;
    
    public:
                    CCache              ( uint32_t inMaxSize, uint32_t inBuckets, time_t inMRAWindow, int32_t inTTL, uint32_t inPolicyFlags );
                    ~CCache             ( void );
    
        sCacheEntry *AddEntry           ( kvbuf_t *inBuffer, const char *inKey, int32_t inTTL, uint32_t inFlags );
        void        RemoveEntry         ( sCacheEntry *inEntry );

        bool        AddKeyToEntry       ( sCacheEntry *inEntry, const char *inKey, bool inUnique );
        void        RemoveKey           ( const char *inKey );
	
        kvbuf_t     *Fetch              ( sKeyList *inKeys, bool inMatchAll = false, int32_t *outLowestTTL = NULL );

        void        Flush               ( void );
        int         Sweep               ( uint32_t inEntryType, bool inCheckDate );
    
        int         UpdateAvailability  ( char *iNode, bool inState );
        bool        isCacheOverMax      ( void );
        uint32_t    GetCount            ( void );
	
    private:
        void        DoNotifies          ( uint32_t inFlags );
        bool        RemoveCollision     ( const char *inKey );
        uint32_t    HashKey             ( const char *inKey );
	
		void		IsolateEntry		( sCacheEntry *inEntry );
		void		InsertEntryAfter	( sCacheEntry *inAfter, sCacheEntry *inEntry );
		sCacheEntry	*ReorderEntry		( sCacheEntry *inEntry );
		sCacheEntry	*FindEntry			( const char *inKey );
};

#endif /* __CACHE_H__ */
