/*
 *  NSLUtil.h
 *  automount
 *
 *  Created by Pat Dirks on Wed Mar 27 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <pthread.h>
#include <unistd.h>

#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>

#define MAXNSLOBJECTNAMELENGTH 256

@class NSLVnode;

typedef enum {
	kNetworkObjectTypeNone = 0,
	kNetworkNeighborhood = 1,
	kNetworkServer = 2
} NetworkObjectType;

struct SearchResultList {
	union {
		pthread_mutex_t resultListMutex;
		char mutexbuf[64];	/* 44 */
	} mutex_u;
	TAILQ_HEAD(ResultListHead, SearchResult) contentsFound;
    NSLNeighborhood searchTarget;
    int searchComplete;
	pthread_cond_t searchResultsCond;
};
#define resultListMutex mutex_u.resultListMutex

#define INIT_SEARCHRESULTLIST(resultsptr, targetneighborhood) \
	InitSearchResultList(resultsptr, targetneighborhood);

struct SearchResult {
	TAILQ_ENTRY(SearchResult) sr_link;
	NSLResultType resultType;
	NSLClientRef callerClientRef;
    NetworkObjectType objectType;
	union {
		NSLNeighborhood neighborhood;
		NSLServiceRef service;
	} result;
};

#define INIT_SEARCHRESULT(resultptr, clientRef, type) \
	(resultptr)->callerClientRef = (clientRef); \
    (resultptr)->objectType = (type);

typedef struct _searchContext *SearchContextPtr;
typedef void (*NewResultNotificationFunction)(SearchContextPtr);

enum searchStatus {
	kNoSearchActive = 0,
	kSearchActive,
	kCachedSearchComplete,
	kInitialSearchComplete,
	kSearchComplete
};

#define kSearchResultsBeingProcessed 0x00000001
#define kSearchAwaitingCleanup       0x00000002

typedef struct _searchContext {
	TAILQ_ENTRY(_searchContext) sc_link;
	unsigned long searchFlags;
	NSLClientRef searchClientRef;
	NSLRequestRef searchRef;
	NSLVnode *parent_vnode;
	NetworkObjectType searchTargetType;
	unsigned long searchGenerationNumber;
	enum searchStatus searchState;
    struct SearchResultList *results;
    NewResultNotificationFunction notificationCallBack;
    void *notificationClientRef;
} SearchContext;

#define INIT_SEARCHCONTEXT(contextptr, parent, clientref, resultslist, generation, callback, notifyref) \
	(contextptr)->searchFlags = 0; \
	(contextptr)->searchClientRef = (clientref); \
	(contextptr)->parent_vnode = (parent); \
	(contextptr)->searchGenerationNumber = (generation); \
    (contextptr)->results = (resultslist); \
    (contextptr)->notificationCallBack = (callback); \
    (contextptr)->notificationClientRef = (notifyref);

extern pthread_mutexattr_t gDefaultMutexAttr;
extern pthread_condattr_t gDefaultCondAttr;

void InitSearchResultList(struct SearchResultList *resultsptr, NSLNeighborhood targetneighborhood);
void WaitForCachedSearchCompletion(SearchContextPtr callContext);
void WaitForInitialSearchCompletion(SearchContextPtr callContext);
int StartSearchForNeighborhoodsInNeighborhood( NSLNeighborhood ParentNeighborhood, SearchContextPtr callContext );
int StartSearchForServicesInNeighborhood(NSLNeighborhood neighborhood, CFArrayRef serviceTypes, SearchContextPtr callContext );
CFStringRef CopyMainStringFromAttribute( NSLServiceRef inServiceRef, CFStringRef inKey );
