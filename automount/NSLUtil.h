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

#define NSLService CFMutableDictionaryRef

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
	TAILQ_ENTRY(SearchResult) sibling_link;
	NSLClientRef callerClientRef;
    NetworkObjectType objectType;
	union {
		NSLNeighborhood neighborhood;
		NSLService service;
	} result;
    struct SearchResultList *searchResults;
};

#define INIT_SEARCHRESULT(resultptr, clientRef, type, resultslist) \
	(resultptr)->callerClientRef = (clientRef); \
    (resultptr)->objectType = (type); \
    (resultptr)->searchResults = (resultslist);

typedef struct _searchContext {
	NSLClientRef searchClientRef;
	NSLRequestRef searchRef;
    struct SearchResultList *results;
} SearchContext, *SearchContextPtr;

#define INIT_SEARCHCONTEXT(contextptr, clientref, resultslist) \
	(contextptr)->searchClientRef = (clientref); \
    (contextptr)->results = (resultslist);

extern pthread_mutexattr_t gDefaultMutexAttr;
extern pthread_condattr_t gDefaultCondAttr;

void InitSearchResultList(struct SearchResultList *resultsptr, NSLNeighborhood targetneighborhood);
void WaitForSearchCompletion(struct SearchResultList *searchResults);
int StartSearchForNeighborhoodsInNeighborhood( NSLNeighborhood ParentNeighborhood, SearchContextPtr callContext );
int StartSearchForServicesInNeighborhood(NSLNeighborhood neighborhood, CFArrayRef serviceTypes, SearchContextPtr callContext );
CFStringRef GetMainStringFromAttribute( CFDictionaryRef inDict, CFStringRef inKey );
