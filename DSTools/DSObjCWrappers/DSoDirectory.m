/*
 * Copyright (c) 2003-2009 Apple Inc. All rights reserved.
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
 * @header DSoDirectory
 */


#import "DSoDirectory.h"
#import "DSoDirectoryPriv.h"

#import <unistd.h>

#import "DSoNode.h"
#import "DSoBuffer.h"
#import "DSoDataList.h"
#import "DSoException.h"
#import "DSoNodeConfig.h"
#import "DSoDataNode.h"
#import "DSoNodeBrowserItem.h"

// ----------------------------------------------------------------------------
// Public Methods.
#pragma mark ***** Public Methods *****

@implementation DSoDirectory
- (id)init
{
    [super init];
    mDirRef			= 0;
    mLocalNode		= nil;
    mSearchNode		= nil;
    proxyHostname   = nil;
    proxyUsername   = nil;
    proxyPassword   = nil;
    mDirRefLock		= [[NSRecursiveLock alloc] init];
    return self;
}

// Open a connection to the local machine.
- (id)initWithLocal
{
    @try {
        [self init];
        [self openLocalHost];
    } @catch( NSException *exception ) {
        [self release];
        @throw;
    }
    return self;
}

// Open a connection to the local machine.
- (id)initWithLocalPath:(NSString*)filePath
{
    @try {
        [self init];
        [self openLocalOnlyWithLocalPath:filePath];
    } @catch( NSException *exception ) {
        [self release];
        @throw;
    }
    return self;
}


// Open a conection to a remote machine using DS Proxy.
- (id)initWithHost:(NSString*)hostName user:(NSString*)inUser password:(NSString*)inPassword
{
    @try {
        [self init];
        [self openHost:hostName user:inUser password:inPassword];
        // Save the hostname, username and password in case we
        // lose the connection and have to re-establish it
        proxyHostname = [hostName retain];
        proxyUsername = [inUser retain];
        proxyPassword = [inPassword retain];
    } @catch( NSException *exception ) {
        [self release];
        @throw;
    }
    return self;
}

- (void)dealloc
{
    [self close];
    [mDirRefLock release];
    if (proxyHostname != nil)
    {
        [proxyHostname release];
        [proxyUsername release];
        [proxyPassword release];
    }
	[mRecordTypes release];
	[mAttributeTypes release];

    [super dealloc];
}

- (void)finalize
{
    [self close];
    [super finalize];
}
	
				
- (DSRef)verifiedDirRef
{
    tDirStatus nError = eDSNoErr;
    
    [mDirRefLock lock];

    @try
    {
		// DirRef initialized, check its validity.
  		// Important that ref count not changed!
		if (!mDirRef || (nError = dsVerifyDirRefNum (mDirRef)))
		{
			// Reopen the reference
			[self reopen];
        }
    } @catch( NSException *exception ) {
        @throw;
    } @finally {
        [mDirRefLock unlock];
    }
    
    return mDirRef;
}

- (unsigned long)	nodeCount
{
    DSRef			refTemp		= [self verifiedDirRef];
    UInt32			ulCount		= 0;
    tDirStatus		nError		= dsGetDirNodeCount(refTemp, &ulCount);
    
    if (nError)
        [DSoException raiseWithStatus:nError];
        
    return (unsigned long)ulCount;
}

- (NSArray*) findNodeNames:(NSString*)inPattern matchType:(tDirPatternMatch)inType
{
    tDirStatus			nError			= eDSNoErr;
	tDataListPtr		dlpNodeName		= nil;
    tContextData		continueData	= 0;
    DSRef				refTemp			= 0;
    DSoBuffer		   *bufNodeList		= nil;
    unsigned long		i				= 0;
	UInt32				ulCount			= 0;
    char			   *szpTemp			= nil;
    NSString		   *sNodeName		= nil;
    NSMutableArray	   *resultList		= nil;

    refTemp = [self verifiedDirRef];
    bufNodeList = [[DSoBuffer alloc] initWithDir:self  bufferSize:1024];
    
    resultList = [NSMutableArray array];
    
	// Generate a list of matching nodes.
    do {
        if (inPattern) {
            DSoDataList *dlPattern = [[DSoDataList alloc] initWithDir:self separator:'/' pattern:inPattern];
            nError = dsFindDirNodes (refTemp, [bufNodeList dsDataBuffer], [dlPattern dsDataList],
                                        inType, &ulCount, &continueData) ;
            [dlPattern release];
        }
        else
        {
            nError = dsFindDirNodes (refTemp, [bufNodeList dsDataBuffer], NULL,
                                        inType, &ulCount, &continueData) ;
        }

        if (nError == eDSBufferTooSmall)
        {
            unsigned long size = [bufNodeList getBufferSize];
            [bufNodeList grow:size + size];
            continue;
        }
        // Validate results.
        if (nError) {
            [bufNodeList release];
            [DSoException raiseWithStatus:nError];
        }

        resultList = [NSMutableArray arrayWithCapacity:ulCount];

        for (i = 1; i <= ulCount; i++)
        {
            nError = dsGetDirNodeName (refTemp, [bufNodeList dsDataBuffer], i, &dlpNodeName);
            if (nError)
            {
                [bufNodeList release];
                [DSoException raiseWithStatus:nError];
            }

            szpTemp = dsGetPathFromList (refTemp, dlpNodeName, "/") ;
            sNodeName = [NSString stringWithUTF8String:szpTemp];
            [resultList addObject:sNodeName];

			free (szpTemp) ;
			dsDataListDeallocate (refTemp, dlpNodeName) ;
			free (dlpNodeName) ;
        }
		
    } while (continueData != 0 || nError == eDSBufferTooSmall);

    if (continueData != 0)
        dsReleaseContinueData(refTemp, continueData);
    
    [bufNodeList release];
    return resultList; // Already autoreleased.
}


// Return an instantiated version of a particular node.
// 
- (DSoNode*) findNode: (NSString*)inPattern
{
    return [self findNode:inPattern matchType:eDSExact];
}

- (DSoNode*) findNodeViaEnum: (int)inPattern
{
    return [self findNode:nil matchType:inPattern];
}

- (DSoNode*) findNode: (NSString*)inPattern 
                     matchType: (tDirPatternMatch)inType
{
    return [self findNode:inPattern matchType:inType useFirst:YES];
}
                     
- (DSoNode*) findNode: (NSString*)inPattern 
                     matchType: (tDirPatternMatch)inType
                     useFirst: (BOOL)inUseFirst
{
    tDirStatus			nError		= eDSNoErr;
	tDirNodeReference	refNode		= 0;
	tDataListPtr		dlpNodeName = nil;
    DSRef				refTemp		= 0;
    DSoBuffer		   *bufNodeList = nil;
    UInt32				ulCount		= 0;
    char			   *szpTemp		= nil;
    NSString		   *sNodeName   = nil;
    DSoNode			   *opTemp		= nil;
    
    
    // Find the first (should be only) exact match if we have a full name.
    
    // Before going any further, check the node list for a match.
    if (inPattern && (inType == eDSExact) && inUseFirst)
    {
        DSoDataList *dlNodeName;
        // If we have a full name, do not validate it, simply open it here.
		refTemp = [self verifiedDirRef];
		dlNodeName = [[DSoDataList alloc] initWithDir:self separator:'/' pattern:inPattern];
		nError = dsOpenDirNode (refTemp, [dlNodeName dsDataList], &refNode);
        [dlNodeName release];
        if (nError)
        {
			[DSoException raiseWithStatus:nError];
        }
		
		if ([inPattern hasPrefix:@"/Search"]) {
			opTemp = [[DSoSearchNode alloc] initWithDir:self nodeRef:refNode nodeName:inPattern];
		} else if ([inPattern hasPrefix:@"/Configure"]) {
			opTemp = [[DSoNodeConfig alloc] initWithDir:self nodeRef:refNode nodeName:inPattern];   
		} else {
			opTemp = [[DSoNode alloc] initWithDir:self nodeRef:refNode nodeName:inPattern];
		}
        
        return [opTemp autorelease];
    }
    
    refTemp = [self verifiedDirRef];
    bufNodeList = [[DSoBuffer alloc] initWithDir:self];
    
	// Generate a list of matching nodes.
	do {
		if (inPattern)
		{
			DSoDataList *dlPattern = [[DSoDataList alloc] initWithDir:self separator:'/' pattern:inPattern];
			nError = dsFindDirNodes (refTemp, [bufNodeList dsDataBuffer], [dlPattern dsDataList],
									 inType, &ulCount, NULL) ;
			[dlPattern release];
		}
		else
		{
			nError = dsFindDirNodes (refTemp, [bufNodeList dsDataBuffer], NULL,
									 inType, &ulCount, NULL) ;
		}
	} while (nError == eDSBufferTooSmall);

	// Validate results.
    if( nError == eDSNoErr )
    {
        if (ulCount <= 0) {
            nError = eDSUnknownNodeName;
        } else if (!inUseFirst && (ulCount > 1)) {
            nError = eDSNodeNotFound;
        }
    }
    
    if( nError ) {
        [bufNodeList release];
        [DSoException raiseWithStatus:nError];
    }

	// Get the first matching, canonical node name from DS.
	nError = dsGetDirNodeName (refTemp, [bufNodeList dsDataBuffer], 1, &dlpNodeName);
    [bufNodeList release];
    if (nError)
		[DSoException raiseWithStatus:nError];

	szpTemp = dsGetPathFromList (refTemp, dlpNodeName, "/") ;
	sNodeName = [NSString stringWithUTF8String:szpTemp];
	free (szpTemp) ;
    
	// Create a DSoNode from the first entry and add it to the map.
	nError = dsOpenDirNode (refTemp, dlpNodeName, &refNode) ;
	dsDataListDeallocate (refTemp, dlpNodeName) ;
	// Deallocate() only frees the nodes, not the tDataList.
	free (dlpNodeName) ;

    if (nError)
    {
        [DSoException raiseWithStatus:nError];
    }

	switch (inType) {
	case eDSAuthenticationSearchNodeName:
	case eDSContactsSearchNodeName:
	case eDSNetworkSearchNodeName:
		opTemp = [[DSoSearchNode alloc] initWithDir:self nodeRef:refNode nodeName:sNodeName];
		break;
	case eDSConfigNodeName:
		opTemp = [[DSoNodeConfig alloc] initWithDir:self nodeRef:refNode nodeName:inPattern];
		break;
	default:
		opTemp = [[DSoNode alloc] initWithDir:self nodeRef:refNode nodeName:sNodeName];
		break;
	}
    
    return [opTemp autorelease];
}

/*!
 * @method findNodeNames
 * @abstract Finds the list of all node names registered by DS.
 *			Returns an empty array if no matching results are found.
 * @result An array of NSString objects containing the node names.
 */
- (NSArray*) findNodeNames
{
    tDirStatus			nError			= eDSNoErr;
	tDataListPtr		dlpNodeName		= nil;
    tContextData		continueData	= 0;
    DSRef				refTemp			= 0;
    DSoBuffer		   *bufNodeList		= nil;
    unsigned long		i				= 0;
	UInt32				ulCount			= 0;
    char			   *szpTemp			= nil;
    NSString		   *sNodeName		= nil;
    NSMutableArray	   *resultList		= nil;

    refTemp = [self verifiedDirRef];
    bufNodeList = [[DSoBuffer alloc] initWithDir:self  bufferSize:1024];
    
    resultList = [NSMutableArray array];
    
	// Generate a list of matching nodes.
    do {
		nError = dsGetDirNodeList (refTemp, [bufNodeList dsDataBuffer], 
								   &ulCount, &continueData) ;

        if (nError == eDSBufferTooSmall)
        {
            unsigned long size = [bufNodeList getBufferSize];
            [bufNodeList grow:size + size];
            continue;
        }
        // Validate results.
        if (nError) {
            [bufNodeList release];
            [DSoException raiseWithStatus:nError];
        }

        resultList = [NSMutableArray arrayWithCapacity:ulCount];

        for (i = 1; i <= ulCount; i++)
        {
            nError = dsGetDirNodeName (refTemp, [bufNodeList dsDataBuffer], i, &dlpNodeName);
            if (nError)
            {
                [bufNodeList release];
                [DSoException raiseWithStatus:nError];
            }

            szpTemp = dsGetPathFromList (refTemp, dlpNodeName, "/") ;
            sNodeName = [NSString stringWithUTF8String:szpTemp];
            [resultList addObject:sNodeName];

			free (szpTemp) ;
			dsDataListDeallocate (refTemp, dlpNodeName) ;
			free (dlpNodeName) ;
        }
		
    } while (continueData != 0 || nError == eDSBufferTooSmall);

    if (continueData != 0)
        dsReleaseContinueData(refTemp, continueData);
    
    [bufNodeList release];
    return resultList; // Already autoreleased.
}

/*!
 * @method nodeBrowserItems
 * @abstract Finds the all top level items for the node browser.
 * @result An array of DSoNodeBrowserItem objects.
 */
- (NSArray*) nodeBrowserItems
{
	NSMutableArray* browserItems = nil;
	NSArray* nodeNames = [self findNodeNames];
	NSMutableSet* firstComponents = [NSMutableSet set];
	NSEnumerator* nodeNameEnum = [nodeNames objectEnumerator];
	NSString* nodeName = nil;
	
	while ((nodeName = (NSString*)[nodeNameEnum nextObject]) != nil)
	{
		NSArray* pathComponents = [nodeName pathComponents];
		if ([pathComponents count] > 1)
		{
			[firstComponents addObject:[pathComponents objectAtIndex:1]];
		}
	}
	
	browserItems = [NSMutableArray arrayWithCapacity:[firstComponents count]];
	nodeNameEnum = [firstComponents objectEnumerator];
	
	while ((nodeName = (NSString*)[nodeNameEnum nextObject]) != nil)
	{
		DSoNodeBrowserItem* item = [[DSoNodeBrowserItem alloc] initWithName:nodeName directory:self];
		[browserItems addObject:item];
		[item release];
	}
	
	return browserItems; // Already autoreleased.
}

/*!
 * @method standardRecordTypes
 * @abstract Retrieves all defined standard record types from the config node.
 * @result An array of strings of the record types.
 */
- (NSArray*) standardRecordTypes
{
	if (mRecordTypes == nil) 
	{
		DSoNodeConfig* config = (DSoNodeConfig*)[[DSoNodeConfig alloc] initWithDir:self];
		mRecordTypes = [[config findRecordNames:@kDSRecordsAll
										 ofType:kDSStdRecordTypeRecordTypes
									  matchType:eDSExact] retain];
		[config release];
	}
	return mRecordTypes;
}

/*!
 * @method standardAttributeTypes
 * @abstract Retrieves all defined standard attribute types from the config node.
 * @result An array of strings of the attribute types.
 */
- (NSArray*) standardAttributeTypes
{
	if (mAttributeTypes == nil) 
	{
		DSoNodeConfig* config = (DSoNodeConfig*)[[DSoNodeConfig alloc] initWithDir:self];
		mAttributeTypes = [[config findRecordNames:@kDSRecordsAll
										 ofType:kDSStdRecordTypeAttributeTypes
									  matchType:eDSExact] retain];
		[config release];
	}
	return mAttributeTypes;	
}


// Convenience methods to create well-known nodes.

- (DSoNode*)			localNode
{
    return [self findNode:nil matchType:eDSLocalNodeNames useFirst:YES];
}


- (DSoSearchNode*) searchNode
{
    DSRef				refTemp		= 0;
    DSoBuffer		   *bufNodeList = nil;
    DSoSearchNode      *searchNode  = nil;
    tDataListPtr		dlpName		= 0;
    tDirNodeReference	refNode		= 0;
    tDirStatus			nError		= eDSNoErr;
    UInt32				ulCount		= 0 ;
    char			   *szpTemp		= nil;
    NSString		   *sNodeName   = nil;
    unsigned long		ulTries		= 10;
        
	// See findNode: above for similar comments.

	for ( ; (ulTries-- && !searchNode) ; sleep (3)) {
		// function creates and returns a DSSearchNode, not a DSoNode.

		// Most of FindNode() is duplicated here because this
		// function creates and returns a DSSearchNode, similar to a DSoNode.
		refTemp = [self verifiedDirRef];
		bufNodeList = [[DSoBuffer alloc] initWithDir:self];
		
		do {
			nError = dsFindDirNodes(refTemp, [bufNodeList dsDataBuffer], NULL, eDSSearchNodeName, &ulCount, NULL);
			if (nError == eDSBufferTooSmall) {
				[bufNodeList grow: [bufNodeList getBufferSize] * 2];
			}
		} while (nError == eDSBufferTooSmall);
        
		if (nError != 0 || (ulCount != 1))
        {
            [bufNodeList release];
			continue ;
        }
		if (nError = dsGetDirNodeName (refTemp, [bufNodeList dsDataBuffer], 1, &dlpName))
        {
            [bufNodeList release];
			continue ;
        }
        [bufNodeList release];
        
		szpTemp = dsGetPathFromList (refTemp, dlpName, "/") ;
		sNodeName = [NSString stringWithUTF8String:szpTemp] ;
		free (szpTemp) ;

		// Open the node manually and create an instance of DSSearchNode.
		nError = dsOpenDirNode (refTemp, dlpName, &refNode) ;
		dsDataListDeallocate (refTemp, dlpName) ;
		// Deallocate() only frees the nodes, not the tDataList.
		free (dlpName) ;

		if (!nError) {
			searchNode = [[DSoSearchNode alloc] initWithDir:self nodeRef:refNode nodeName:sNodeName];
			return [searchNode autorelease] ;
		}
		if (nError != eDSUnknownNodeName)
        {
			break ;
        }
	}

    return [searchNode autorelease];
}

- (tDirReference)dsDirRef
{
    return mDirRef;
}

@end

// ----------------------------------------------------------------------------
// Private Methods
#pragma mark ***** Private Methods *****

@implementation DSoDirectory (DSoDirectoryPrivate)

- (void)openLocalHost
{
    tDirStatus nError = eDSNoErr;

    [mDirRefLock lock];
    if (!mDirRef)
        nError = dsOpenDirService(&mDirRef);
    [mDirRefLock unlock];
    if (nError)
        [DSoException raiseWithStatus:nError];
}

- (void)openLocalOnlyWithLocalPath:(NSString *)filePath
{
    tDirStatus nError = eDSNoErr;

    [mDirRefLock lock];
    if (!mDirRef)
        nError = dsOpenDirServiceLocal(&mDirRef, [filePath UTF8String]);
    [mDirRefLock unlock];
    if (nError)
        [DSoException raiseWithStatus:nError];
}


- (void)openHost:(NSString*)hostName user:(NSString*)inUser password:(NSString*)inPassword
{
    DSoBuffer      *step			= nil;
    DSoBuffer      *stepResponse	= nil;
    DSoDataNode    *authMethod		= nil;
    UInt32			length			= 0;
    tDirStatus		status			= eDSNoErr;
	NSData		   *userNameData	= nil;
	NSData		   *passwordData	= nil;

    step = [[DSoBuffer alloc] initWithDir:nil bufferSize:2048];
    stepResponse = [[DSoBuffer alloc] initWithDir:nil bufferSize:2048];
    authMethod = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:nil cString:kDSStdAuthNodeNativeClearTextOK];

	userNameData = [inUser dataUsingEncoding:NSUTF8StringEncoding];
    length = [userNameData length];
    [step setData:&length length:sizeof(length)];
    [step appendData:[userNameData bytes] length:length];

	passwordData = [inPassword dataUsingEncoding:NSUTF8StringEncoding];
    length = [passwordData length];
    [step appendData:&length length:sizeof(length)];
    [step appendData:[passwordData bytes] length:length];

    status = dsOpenDirServiceProxy(&mDirRef, [hostName cString], 0, [authMethod dsDataNode], [step dsDataBuffer], [stepResponse dsDataBuffer], NULL);

	[step release];
	[stepResponse release];
	[authMethod release];
	
    if (status != eDSNoErr) {
        [[DSoException name:@"DSOpenDirServiceErr" reason:@"Cannot open Directory Services Proxy." status:status] raise];
    }
}

- (void)close
{
    [mDirRefLock lock];
    if (mDirRef)
    {
        dsCloseDirService(mDirRef);
        mDirRef = 0;
    }
    [mDirRefLock unlock];
}

// This method assumes that the mDirRef has been invalidated
// and will not close the reference.
- (void)reopen
{
    // If the proxyUsername is not null, then we have a proxy connection.
    [self close];
    if (proxyHostname)
        [self openHost:proxyHostname user:proxyUsername password:proxyPassword];
    else
        [self openLocalHost];
}

- (DSoNodeConfig*)configNode
{
    return [[[DSoNodeConfig alloc] initWithDir:self] autorelease];
}

@end
