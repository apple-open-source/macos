/*
 *  NSLUtil.c
 *  automount
 *
 *  Created by Pat Dirks on Wed Mar 27 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

#import "log.h"
#include "NSLUtil.h"
#import "automount.h"

#define TRACE_NSL 0

#define NONDECREASINGSEARCHSTATES 0
#define UNIQUESTATECHANGESONLY 1

pthread_mutexattr_t gDefaultMutexAttr = { 0 };
pthread_condattr_t gDefaultCondAttr = { 0 };

pascal void XNeighborhoodLookupNotifyProc(void *clientContext, NSLRequestRef requestRef);
pascal void XServicesLookupNotifyProc(void *clientContext, NSLRequestRef requestRef);

/***********************************************************************************
*
*   N S L   U T I L I T Y   F U N C T I O N S
*
***********************************************************************************/

void InitSearchResultList(struct SearchResultList *resultsptr, NSLNeighborhood targetneighborhood) {
    resultsptr->searchTarget = targetneighborhood;
	pthread_mutex_init(&resultsptr->resultListMutex, NULL);
    TAILQ_INIT(&resultsptr->contentsFound);
    pthread_cond_init(&resultsptr->searchResultsCond, NULL);
    resultsptr->searchComplete = 0;
}



void SetSearchState(SearchContextPtr searchContext, enum searchStatus searchState) {
#if NONDECREASINGSEARCHSTATES
	if (searchState >= searchContext->searchState) {
#endif
		searchContext->searchState = searchState;
		if (searchState == kSearchComplete) searchContext->results->searchComplete = true;
#if NONDECREASINGSEARCHSTATES
	} else {
		sys_msg(debug, LOG_DEBUG, "SetSearchState: ignoring backwards state change (from %d to %d, context = 0x%x).",
									searchContext->searchState, searchState, searchContext);
	};
#endif
    pthread_cond_broadcast(&searchContext->results->searchResultsCond);
}



void UpdateSearchState(SearchContextPtr searchContext, enum searchStatus searchState) {
	enum searchStatus newSearchState = kSearchActive;
	
	if ( (searchState == kNSLXCachedSearchComplete) ||
		 (searchState == kNSLXInitialSearchComplete) ||
		 (searchState == kNSLSearchStateComplete) ) {
		switch (searchState) {
		  case kNSLXCachedSearchComplete:
			newSearchState = kCachedSearchComplete;
			break;
		
		  case kNSLXInitialSearchComplete:
			newSearchState = kInitialSearchComplete;
			break;
		
		  case kNSLSearchStateComplete:
			newSearchState = kSearchComplete;
			break;
		  
		  default:
			/* Never reached */
			break;
		};
		
#if UNIQUESTATECHANGESONLY
		if (searchContext->searchState != newSearchState) {
#endif
			sys_msg(debug_nsl, LOG_DEBUG, "UpdateSearchState: changing search state (context = 0x%x) from %d to %d.",
										searchContext, searchContext->searchState, newSearchState);
			SetSearchState(searchContext, newSearchState);
			(*searchContext->notificationCallBack)(searchContext);
#if UNIQUESTATECHANGESONLY
		} else {
			sys_msg(debug_nsl, LOG_DEBUG, "UpdateSearchState: search state (context = 0x%x) is already %d; no changes to be set.",
										searchContext, searchContext->searchState);
		};
#endif
	};
}



BOOL CachedSearchComplete(SearchContextPtr searchContext) {
	return (searchContext->searchState >= kCachedSearchComplete);
}



BOOL InitialSearchComplete(SearchContextPtr searchContext) {
	return (searchContext->searchState >= kInitialSearchComplete);
}



int SearchIsComplete(SearchContextPtr searchContext) {
	return searchContext->results->searchComplete;
}



void WaitForCachedSearchCompletion(SearchContextPtr callContext) {
	struct SearchResultList *searchResults = callContext->results;
	
	pthread_mutex_lock(&callContext->results->resultListMutex);
    while ( !CachedSearchComplete(callContext) ) {
		pthread_mutex_unlock(&searchResults->resultListMutex);
        pthread_cond_wait(&searchResults->searchResultsCond, &searchResults->resultListMutex);
    };
	pthread_mutex_unlock(&searchResults->resultListMutex);
}



void WaitForInitialSearchCompletion(SearchContextPtr callContext) {
	struct SearchResultList *searchResults = callContext->results;
	
	pthread_mutex_lock(&callContext->results->resultListMutex);
    while ( !InitialSearchComplete(callContext) ) {
		pthread_mutex_unlock(&searchResults->resultListMutex);
        pthread_cond_wait(&searchResults->searchResultsCond, &searchResults->resultListMutex);
    };
	pthread_mutex_unlock(&searchResults->resultListMutex);
}



#if 0
void WaitForSearchCompletion(SearchContextPtr callContext) {
	struct SearchResultList *searchResults = callContext->results;
	
	pthread_mutex_lock(&callContext->results->resultListMutex);
    while ( !SearchIsComplete(callContext) ) {
		pthread_mutex_unlock(&searchResults->resultListMutex);
        pthread_cond_wait(&searchResults->searchResultsCond, &searchResults->resultListMutex);
    };
	pthread_mutex_unlock(&searchResults->resultListMutex);
}
#endif



int StartSearchForNeighborhoodsInNeighborhood( NSLNeighborhood ParentNeighborhood,
                                               SearchContextPtr callContext )
{
    NSLXClientNotifyUPP		myXNeighborhoodNotifyUPP = NULL;
	NSLError 				iErr = kNSLErrorNoErr;
    
	myXNeighborhoodNotifyUPP = (NSLXClientNotifyUPP) NewNSLXClientNotifyUPP(XNeighborhoodLookupNotifyProc);
	
	callContext->searchTargetType = kNetworkNeighborhood;
	callContext->searchState = kSearchActive;
    // get the default neighborhoods (top level)
    iErr = NSLXStartNeighborhoodLookup( callContext->searchClientRef,
                                        myXNeighborhoodNotifyUPP,
                                        callContext,
                                        ParentNeighborhood,
                                        &callContext->searchRef);
    
    if ( iErr.theErr )
    {
        // all errors returned immediately from NSLXStartNeighborhoodLookup are fatal.
        // non-fatal errors should be reported through the callbacks.
		sys_msg(debug, LOG_ERR, "NSLXStartNeighborhoodLookup returned error: %ld", iErr.theErr);
		SetSearchState(callContext, kSearchComplete);
    };
	
    return iErr.theErr;
}



pascal void XNeighborhoodLookupNotifyProc(void *clientContext, NSLRequestRef requestRef)
{
	NSLNeighborhood	theNeighborhood = NSLXGetNeighborhoodResult(requestRef);
	NSLSearchState	theSearchState = NSLXGetSearchState(requestRef);
	NSLError		theSearchStatus = NSLXGetSearchStatus(requestRef);
	SearchContextPtr callContext = (SearchContextPtr)clientContext;
	struct SearchResult	*thisResult = NULL;

    // if we got an empty notification, ignore it
    if ( theSearchStatus.theErr == noErr &&					// no errors
         theNeighborhood == NULL &&							// no items
         theSearchState == kNSLSearchStateOnGoing			// doesn't terminate a search
       ) {
        return;
    } else if (theNeighborhood) {
        // handle the result.  WARNING - this callback can be called on a different pthread than was started on...
        // we'll add a new call, NSLXGetNameFromNeighborhood that returns a CFStringRef...
        char*			name = NULL;
        long			nameLen = 0;
        
        NSLGetNameFromNeighborhood( theNeighborhood, &name, &nameLen );

        if ( name && nameLen > 0 )
        {
			thisResult = (struct SearchResult *)calloc(1, sizeof(struct SearchResult));
			if (thisResult) {
                INIT_SEARCHRESULT(thisResult,
                                  callContext->searchClientRef,
                                  kNetworkNeighborhood);
				thisResult->result.neighborhood = NSLCopyNeighborhood( theNeighborhood );
				if (thisResult->result.neighborhood == NULL) {
					sys_msg(debug_nsl, LOG_ERR, "NULL service result from NSLCopyNeighborhood?!");
					free(thisResult);
					thisResult = NULL;
				} else {
					thisResult->resultType = NSLXGetResultType(requestRef);
					pthread_mutex_lock(&callContext->results->resultListMutex);
					TAILQ_INSERT_TAIL(&callContext->results->contentsFound, thisResult, sr_link);
					(*callContext->notificationCallBack)(callContext);
					pthread_mutex_unlock(&callContext->results->resultListMutex);
					pthread_cond_signal(&callContext->results->searchResultsCond);
				};
			};
        }
    } else {
		sys_msg(debug_nsl, LOG_ERR, "NSL returned NULL neighborhood?!");
	};

	sys_msg(debug_nsl, LOG_DEBUG, "XNeighborhoodLookupNotifyProc: search context = 0x%x; thisResult = 0x%x, NSL search state = %d (currently %d internally)",
		callContext, thisResult, theSearchState, callContext->searchState);
	UpdateSearchState(callContext, theSearchState);
}



int StartSearchForServicesInNeighborhood( NSLNeighborhood neighborhood,
                                          CFArrayRef serviceTypes,
                                          SearchContextPtr callContext )
{
    // start a search in a given neigborhood...
    NSLXClientNotifyUPP		myXServicesNotifyUPP = NULL;
	NSLError 				iErr = kNSLErrorNoErr;
    
    if ( myXServicesNotifyUPP == NULL ) myXServicesNotifyUPP = (NSLXClientNotifyUPP) NewNSLXClientNotifyUPP(XServicesLookupNotifyProc);
    
#if TRACE_NSL
	sys_msg(debug, LOG_DEBUG, "StartSearchForServicesInNeighborhood: Search result list at 0x%x = { 0x%x, 0x%x }.",
								(unsigned long)&(callContext->results->contentsFound),
								(unsigned long)callContext->results->contentsFound.tqh_first,
								(unsigned long)callContext->results->contentsFound.tqh_last);
#endif
	
	callContext->searchTargetType = kNetworkServer;
	callContext->searchState = kSearchActive;
	iErr = NSLXStartServicesLookup( callContext->searchClientRef,
									myXServicesNotifyUPP,
									callContext,
									neighborhood,
									serviceTypes,
									&callContext->searchRef);

	if ( iErr.theErr )
	{
		// all errors returned immediately from NSLXStartNeighborhoodLookup are fatal.
		// non-fatal errors should be reported through the callbacks.
		sys_msg(debug, LOG_ERR, "NSLXStartServicesLookup returned error: %ld", iErr.theErr);
		SetSearchState(callContext, kSearchComplete);
	}

    return iErr.theErr;
}



pascal void XServicesLookupNotifyProc(void *clientContext, NSLRequestRef requestRef)
{
	SearchContextPtr callContext = (SearchContextPtr)clientContext;
    NSLService theResult = NSLXGetSearchResult( requestRef );
	NSLSearchState theSearchState = NSLXGetSearchState(requestRef);
	NSLError theSearchStatus = NSLXGetSearchStatus(requestRef);
    struct SearchResult	*thisResult = NULL;
    
#if TRACE_NSL
	sys_msg(debug, LOG_DEBUG, "Entering XServicesLookupNotifyProc:");
	sys_msg(debug, LOG_DEBUG, "\ttheSearchStatus.theErr = 0x%08lx", theSearchStatus.theErr);
	sys_msg(debug, LOG_DEBUG, "\ttheResult = 0x%x", (unsigned long)theResult);
	sys_msg(debug, LOG_DEBUG, "\ttheSearchState = %d", theSearchState);
#endif
	
    // if we got an empty notification, don't queue it
    if ( (theSearchStatus.theErr == noErr) &&				// no errors
         (theResult == NULL) &&								// no items
         (theSearchState == kNSLSearchStateOnGoing)			// doesn't terminate a search
       )
    {
        return;
    }
    
    if (theResult) 
    {
        thisResult = (struct SearchResult *)calloc(1, sizeof(struct SearchResult));
        if (thisResult) {
#if TRACE_NSL
			sys_msg(debug, LOG_DEBUG, "XServicesLookupNotifyProc: Search result list at 0x%x = { 0x%x, 0x%x }.",
								(unsigned long)&(callContext->results->contentsFound),
								(unsigned long)callContext->results->contentsFound.tqh_first,
								(unsigned long)callContext->results->contentsFound.tqh_last);
			sys_msg(debug, LOG_DEBUG, "\tAdding search result at 0x%x...", (unsigned long)thisResult);
#endif
            INIT_SEARCHRESULT(thisResult,
                                callContext->searchClientRef,
                                kNetworkServer);
			thisResult->result.service =
					(NSLService)CFPropertyListCreateDeepCopy( kCFAllocatorDefault,
																			(CFMutableDictionaryRef)theResult,
																			kCFPropertyListMutableContainers);
			if (thisResult->result.service == NULL) {
				sys_msg(debug_nsl, LOG_ERR, "NULL service result from CFPropertyListCreateDeepCopy?!");
				free(thisResult);
				thisResult = NULL;
			} else {
				thisResult->resultType = NSLXGetResultType(requestRef);
				
				pthread_mutex_lock(&callContext->results->resultListMutex);
				TAILQ_INSERT_TAIL(&callContext->results->contentsFound, thisResult, sr_link);
	#if TRACE_NSL
				sys_msg(debug, LOG_DEBUG, "XServicesLookupNotifyProc: Search result list at 0x%x = { 0x%x, 0x%x }.",
									(unsigned long)&(callContext->results->contentsFound),
									(unsigned long)callContext->results->contentsFound.tqh_first,
									(unsigned long)callContext->results->contentsFound.tqh_last);
	#endif
				(*callContext->notificationCallBack)(callContext);
				pthread_mutex_unlock(&callContext->results->resultListMutex);
				pthread_cond_signal(&callContext->results->searchResultsCond);
			};
        };
    } else {
		sys_msg(debug_nsl, LOG_ERR, "NSL returned NULL service result?!");
    }
    
	sys_msg(debug_nsl, LOG_DEBUG, "XServicesLookupNotifyProc: search context = 0x%x; thisResult = 0x%x, NSL search state = %d (currently %d internally)",
		callContext, thisResult, theSearchState, callContext->searchState);
	UpdateSearchState(callContext, theSearchState);
}



//-----------------------------------------------------------------------------------------------
//	GetMainStringFromAttribute
//
//	Returns: The value in the dictionary if it is a CFStringRef, otherwise it tries to find the 
//			 first string in another type (CFArrayRef, CFDictionaryRef).
//-----------------------------------------------------------------------------------------------

CFStringRef GetMainStringFromAttribute( CFDictionaryRef inDict, CFStringRef inKey )
{
    CFStringRef result = NULL;
    CFTypeRef dValue;
    
    if ( CFDictionaryGetValueIfPresent( inDict, inKey, &dValue ) && dValue )
    {
        if ( CFGetTypeID(dValue) == CFStringGetTypeID() )
        {
            result = (CFStringRef)dValue;
        }
        else
        if ( CFGetTypeID(dValue) == CFArrayGetTypeID() )
        {
            result = (CFStringRef)CFArrayGetValueAtIndex((CFArrayRef)dValue, 0);
        }
    }
    
    return result;
}
