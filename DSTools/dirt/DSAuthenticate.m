/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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
 * @header DSAuthenticate
 */


#import "DSAuthenticate.h"
#import <Security/checkpw.h>

extern BOOL doVerbose;

@implementation DSAuthenticate

- init
{
    tDirStatus  status = eDSNoErr;
	
    [super init];
    _SearchNodeRef = 0;
    _dsStat = [[DSStatus sharedInstance] retain];
    [self useAuthenticationSearchPath];
    [self searchForUsers];

    if (doVerbose)
	{
		printf("---> dsOpenDirService() ........................ ");
		fflush(stdout);
	}
	
    status = dsOpenDirService(&_DirRef);
    if (doVerbose)
	{
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}
	
    if (status != eDSNoErr) 
	{
		[[DSException name:@"DSOpenDirServiceErr" reason:@"Cannot open Directory Services." status:status] raise];
		[_dsStat release];
    }
	
    _MasterUserList = [[NSMutableArray alloc] init];
    return self;
}

- (void) dealloc
{
    if (_SearchNodeRef != 0)
        dsCloseDirNode(_SearchNodeRef);
    if (_DirRef != 0)
        dsCloseDirService(_DirRef);
    if (_MasterUserList != nil)
		[_MasterUserList release];
    [_dsStat release];
    [super dealloc];
}

- (void) reset
{
    if (_SearchNodeRef != 0)
	{
        dsCloseDirNode(_SearchNodeRef);
		_SearchNodeRef = 0;
    }
    [self useAuthenticationSearchPath];
    [self searchForUsers];
    if (_MasterUserList != nil) 
		[_MasterUserList release];
    _MasterUserList = [[NSMutableArray alloc] init];
}

// ------------------------------------------------------------------
// useAuthenticationSearchPath
//
// Set this object to use the Authentication search node.
// This is the default behavior.
// ------------------------------------------------------------------

- (void)useAuthenticationSearchPath
{
    _SearchNodeNameToUse = eDSSearchNodeName;
}

// ------------------------------------------------------------------
// useContactSearchPath
//
// Set this object to use the Contacts search node, instead of
// the Authentication search node.
// ------------------------------------------------------------------

- (void)useContactSearchPath
{
    _SearchNodeNameToUse = eDSContactsSearchNodeName;
}

// ------------------------------------------------------------------
// searchForUsers
//
// Set this object to search for users.
// This is the default behavior.
// ------------------------------------------------------------------

- (void)searchForUsers
{
    _SearchObjectType = kDSStdRecordTypeUsers;
}

// ------------------------------------------------------------------
// searchForGroups
//
// Set this object to search for groups, instead of users.
// ------------------------------------------------------------------

- (void)searchForGroups
{
    _SearchObjectType = kDSStdRecordTypeGroups;
}

// ============================================================================
// Wrapper methods for Directory Service Utility functions
//
// These methods provide wrappers for some DS utility functions.
// Most of the wrapped functions require
// an argument that is a reference to an open Directory server.
// These automatically provide that reference from the class's
// members.


// ------------------------------------------------------------------
// allocateDataBuffer: withNumberOfBlocks: shouldReallocate:
//
// Allocates memory for the passed in tDataBufferPtr. It expects
// an argument with the number of blocks of memory to allocate,
// where the block size is specified in the class's header file.
// The final parameter specifies whether the buffer has previously
// been allocated memory and should thus be deallocated before
// allocating the new amount.
// ------------------------------------------------------------------

- (void)allocateDataBuffer:(tDataBufferPtr*)buffer
        withNumberOfBlocks:(unsigned short) numberOfBlocks 
        shouldReallocate:(BOOL) reAllocate
{
    if (reAllocate == YES)
	{
        tDirStatus status;
		status = dsDataBufferDeAllocate(_DirRef, *buffer);
    }
    *buffer = dsDataBufferAllocate(_DirRef, numberOfBlocks*kBufferBlockSize);
}        

// ------------------------------------------------------------------
// deallocateDataBuffer:
//
// Deallocate memory previously allocated to a tDataBuffer.
// ------------------------------------------------------------------

- (tDirStatus)deallocateDataBuffer:(tDataBufferPtr)buffer
{
    if (buffer != NULL)
	{
		tDirStatus status = dsDataBufferDeAllocate(_DirRef, buffer);
		return status;
    }
    else
        return eDSNoErr;
}

// ------------------------------------------------------------------
// deallocateDataList:
//
// Deallocate memory previously allocated to a tDataList.
// The memory allocated to the tDataList pointer object
// must also be free'd manually.   
// ------------------------------------------------------------------

- (tDirStatus)deallocateDataList:(tDataListPtr)list;
{
    if (list != NULL)
	{
        tDirStatus status = dsDataListDeAllocate(_DirRef, list, TRUE);
        free(list);
        return status;
    }
    else
        return eDSNoErr;
}

// ------------------------------------------------------------------
// deallocateDataNode:
//
// Deallocate memory previously allocated to a tDataNode.
// ------------------------------------------------------------------

- (tDirStatus)deallocateDataNode:(tDataNodePtr)node;
{
    if (node != NULL)
	{
        tDirStatus status = dsDataNodeDeAllocate(_DirRef, node);
        return status;
    }
    else
        return eDSNoErr;
}


// ============================================================================
// Private utility methods for accessing obtaining and opening
// directory nodes.


// ------------------------------------------------------------------
// retrieveLocalNode
//
// Uses dsFindDirNodes to find and return a pointer to a tDataBuffer
// that should only contain one item: the name of the local node.
// ------------------------------------------------------------------

- (tDataBufferPtr) retrieveLocalNode
{
    unsigned long int   count   = 0;
    tDataBufferPtr		buffer  = NULL;
    tDirStatus			status  = eDSNoErr;
    
    [self allocateDataBuffer:&buffer withNumberOfBlocks:4 shouldReallocate: NO];
    if (doVerbose)
	{
		printf("---> dsFindDirNodes() for Local Node .......... ");
		fflush(stdout);
	}
	
    status = dsFindDirNodes(_DirRef, buffer, NULL, eDSLocalNodeNames, &count, NULL);
    if (doVerbose)
	{
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}
	
    return buffer;
}

// ------------------------------------------------------------------
// retrieveSearchPathNode
//
// Uses dsFindDirNodes to find and return a pointer to a tDataBuffer
// that should only contain one item: the name of the selected search node
// that is identified by the instance variable _SearchNodeNameToUse.
// ------------------------------------------------------------------

- (tDataBufferPtr) retrieveSearchPathNode
{
    unsigned long int   count   = 0;
    tDataBufferPtr		buffer  = NULL;
    tDirStatus			status  = eDSNoErr;
    
    [self allocateDataBuffer:&buffer withNumberOfBlocks:4 shouldReallocate: NO];
    if (doVerbose)
	{
		printf("---> dsFindDirNodes() for Search Node .......... ");
		fflush(stdout);
	}
	
    status = dsFindDirNodes(_DirRef, buffer, NULL, _SearchNodeNameToUse, &count, NULL);
    if (doVerbose)
	{
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}
	
    return buffer;
}

// ------------------------------------------------------------------
// getNodeNameForBuffer:
//
// Takes a pointer to a tDataBuffer that represents a list of nodes
// and grabs the name of the first item in the buffer.
// It returns the pointer to a tDataList object that
// encapsulates the name of the node.
// ------------------------------------------------------------------

- (tDataListPtr) getNodeNameForBuffer:(tDataBufferPtr)buffer
{
    tDataListPtr	tdlist	= NULL;
    tDirStatus		status  = eDSNoErr;
	
    tdlist = dsDataListAllocate(_DirRef);
    status = dsGetDirNodeName(_DirRef, buffer, 1, &tdlist);
    return tdlist;
}

// ------------------------------------------------------------------
// openDirNodeWithName:
//
// Takes a pointer to a tDataList that encapsulates the name
// of a directory node, and opens that node.  It returns a
// reference to the open node.
// ------------------------------------------------------------------

- (tDirNodeReference)openDirNodeWithName:(tDataListPtr)inNodeName
{
    tDirNodeReference   fNodeRef	= 0;
    tDirStatus			status		= eDSNoErr;
	
    if ( (doVerbose) && (inNodeName != nil) && (inNodeName->fDataListHead->fBufferData != nil) )
	{
		printf("---> dsOpenDirNode() for node named: %s ...", inNodeName->fDataListHead->fBufferData);
		fflush(stdout);
	}
	
    status = dsOpenDirNode(_DirRef, inNodeName, &fNodeRef);
    if (doVerbose)
	{
		printf("\t");
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}
    return fNodeRef;  
}

// ============================================================================
// Private methods for performing search functionality in 
// Directory Services.


// ------------------------------------------------------------------
// fetchRecordListOnSearchNodeMatchingName:
//
// Peforms a search for the specified inUsername on the search path
// node that is currently referenced by this object, and adds matching 
// nodes to this object's master list of found records.
// See also -fetchRecordListOnNode: matchingName:
// ------------------------------------------------------------------

- (tDirStatus)fetchRecordListOnSearchNodeMatchingName:(NSString*)inUsername
{
    tDataListPtr		nodeName	= nil;
    tDataBufferPtr		nodeBuffer  = nil;
    tDirNodeReference   nodeRef		= 0;
    tDirStatus			status		= eDSNoErr;
    
    nodeBuffer  = [self retrieveSearchPathNode];
    nodeName	= [self getNodeNameForBuffer:nodeBuffer];
    nodeRef		= [self openDirNodeWithName:nodeName];
    status		= [self fetchRecordListOnNode:nodeRef matchingName:inUsername];
    dsCloseDirNode(nodeRef);
    [self deallocateDataBuffer:nodeBuffer];
    [self deallocateDataList:nodeName];
	
    return status;
}

// ------------------------------------------------------------------
// addBufferToListOfUsers: fromBuffer: inSearchNode:
//
// Convert Buffer user records to collection of NSArray/NSDictionary/NSString.
// Takes the buffer items and creates an NSDictionary for each key
// and its values.  Those values are placed into an NSArray.
// Then each record is added to the _MasterUserList NSArray.
// ------------------------------------------------------------------
- (void)addToListOfUsers: (unsigned long)returnCount fromBuffer: (tDataBufferPtr)nodeBuffer inSearchNode:(tDirNodeReference)nodeRefToSearch
{
    int						i				= 0;
	int						j				= 0;
	int						k				= 0;
    tAttributeValueListRef  valueRef		= 0;
    tAttributeEntryPtr		pAttrEntry		= nil;
    tAttributeValueEntryPtr pValueEntry		= nil;
    tAttributeListRef		attrListRef		= 0;
    tRecordEntry		   *pRecEntry		= nil;
    NSMutableDictionary    *userAttributes  = nil;
    NSMutableArray		   *userAttrValues  = nil;
    NSString			   *key				= nil;
    tDirStatus				status			= eDSNoErr;
    
    for (i =1; i<= returnCount; i++)
    {
		status =dsGetRecordEntry( nodeRefToSearch, nodeBuffer, i, &attrListRef, &pRecEntry );
		userAttributes = [[NSMutableDictionary alloc ] init];
		
		// Iterate over the Attributes keys for the user.
		for (j =1; j<=pRecEntry->fRecordAttributeCount; j++)
		{
			userAttrValues = [[NSMutableArray alloc] init];
			status =dsGetAttributeEntry( nodeRefToSearch, nodeBuffer, attrListRef, j, &valueRef, &pAttrEntry );
			
			// Iterate over the values for the current attribute key
			for (k =1; k<=pAttrEntry->fAttributeValueCount; k++)
			{
				status =dsGetAttributeValue( nodeRefToSearch, nodeBuffer, k, valueRef, &pValueEntry );
				if (status == eDSNoErr) {
					// Next commented out line causes a one-time 14 byte leak, don't understand why.
					//[userAttrValues addObject: [NSString stringWithUTF8String:pValueEntry->fAttributeValueData.fBufferData]];
					NSString *valueStr = [[NSString alloc] initWithUTF8String:pValueEntry->fAttributeValueData.fBufferData];
					[userAttrValues addObject:valueStr];
					[valueStr release];
				}
				
				// Clean up memory
				if (pValueEntry != nil)
				{
					status = dsDeallocAttributeValueEntry(_DirRef, pValueEntry);
					pValueEntry = nil;
				}
			}
			
			key = [[NSString alloc] initWithCString: pAttrEntry->fAttributeSignature.fBufferData];
			[userAttributes setObject: userAttrValues forKey: key];
			[key release];
			[userAttrValues release];
	
			// Clean up memory
			if (pAttrEntry != nil)
			{
				status = dsDeallocAttributeEntry(_DirRef, pAttrEntry);
				pAttrEntry = nil;
			}
			if (valueRef != nil)
			{
				status = dsCloseAttributeValueList(valueRef);
				valueRef = nil;
			}
		}
		[_MasterUserList addObject: userAttributes];
		[userAttributes release];
	
		// Clean up memory
		if (pRecEntry != nil)
		{
			status = dsDeallocRecordEntry(_DirRef, pRecEntry);
			pRecEntry = nil;
		}
		if (attrListRef != nil)
		{
			status = dsCloseAttributeList(attrListRef);
			attrListRef = nil;
		}
    }
}

// ------------------------------------------------------------------
// fetchRecordListOnNode: matchingName:
//
// Peforms a search for the specified inUsername in the specified
// node reference (presumable a search node), and adds matching 
// nodes to this object's master list of found records.
// See also -fetchRecordListOnSearchNodeMatchingName:
// ------------------------------------------------------------------

- (tDirStatus)fetchRecordListOnNode:(tDirNodeReference)nodeRefToSearch matchingName:(NSString*)inUsername
{
    tContextData			localcontext	= nil;
    tDataListPtr			recName			= nil;
	tDataListPtr			recType			= nil;
	tDataListPtr			attrType		= nil;
    tDataBufferPtr			nodeBuffer		= nil;
    tDirStatus				status			= eDSNoErr;
    unsigned short			blockCount		= 1;
    unsigned long			returnCount		= 0;
    NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
	unsigned long			matchingUsers   = 0;
    
    recName		= dsBuildListFromStrings(_DirRef, [inUsername cString], NULL);
    recType		= dsBuildListFromStrings(_DirRef, _SearchObjectType, NULL);
    attrType	= dsBuildListFromStrings(_DirRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL);
    
    [self allocateDataBuffer: &nodeBuffer withNumberOfBlocks: blockCount shouldReallocate: NO];
	
    do
	{ 
        if (doVerbose)
		{
			printf("---> dsGetRecordList() .... ");
			fflush(stdout);
		}
		
        // Stuff the record entries into nodeBuffer
        status = dsGetRecordList(nodeRefToSearch, nodeBuffer, recName, eDSExact, recType, attrType, false, &returnCount, &localcontext);
        if (doVerbose)
		{
			printf("Count: %lu, Cont: %3s, \n", returnCount,
					localcontext == nil ? "NO" : "YES");
			[_dsStat printOutErrorMessage:"Status" withStatus:status];
		}
		
        if (status == eDSBufferTooSmall)
		{
			blockCount++;
		}
        else if (status == eDSNoErr && returnCount > 0)
		{
			[self addToListOfUsers: returnCount fromBuffer: nodeBuffer inSearchNode: nodeRefToSearch ];
        }
		
        [self allocateDataBuffer: &nodeBuffer withNumberOfBlocks: blockCount shouldReallocate: YES];
		
    } while (  (status == eDSNoErr && localcontext != NULL) || (status == eDSBufferTooSmall));
            
    // Do Memory cleanup
    if (localcontext != nil)
	{
        dsReleaseContinueData(nodeRefToSearch, localcontext);
		localcontext = 0;
	}
    [self deallocateDataList:recType];
    [self deallocateDataList:recName];
    [self deallocateDataList:attrType];
    [self deallocateDataBuffer:nodeBuffer];
    [pool release];

    // If status is not eDSNoErr, and we didn't get any users, raise an exception.
    // If we did get users, then it is some non-fatal error.  The normal
    // print statements will output the status error, but we should not raise
    // an exception (which aborts program execution).
    // However, if we get a eDSServerTimeout error, then we should raise exception.

	matchingUsers = [_MasterUserList count];
    if (status != eDSNoErr && (matchingUsers <= 0 || status == eDSServerTimeout))
    {
		DSException *ex = nil;
		ex = [DSException name:@"FetchRecordError" reason:@"Unable to retrieve any records via dsGetRecordList." status:status];
		[ex raise];
    }
	
    printf("Call to dsGetRecordList returned count = %ld with ", matchingUsers);
	[_dsStat printOutErrorMessage:"Status" withStatus:status];

    if (matchingUsers <= 0)
    {
        printf("No matching users found.\n");
		if (checkpw([inUsername cString], "") != CHECKPW_UNKNOWNUSER)
			printf("**** checkpw() DISAGREES! ****\n");
    }
        
    return status;
}

// ------------------------------------------------------------------
// getAttibute: forRecordNumber
//
// Retrieve the attribute string from the recordNumber'th 
// matching record from the recent search in the search path.
// ------------------------------------------------------------------

- (NSString*)getAttribute: (char*)attr forRecordNumber: (unsigned long)recordNumber
{
    NSArray    *recordValues	= nil;
    NSString   *key				= [[NSString alloc] initWithCString: attr];
    
    recordValues = [[_MasterUserList objectAtIndex: recordNumber] objectForKey: key];
    [key release];
    
    if (recordValues != nil && [recordValues count] > 0)
	{
		return [recordValues objectAtIndex:0];
    }
    else
		return nil;
}
// ------------------------------------------------------------------
// nodePathStrForSearchNodeRecordNumber:
//
// Retrieve the username string from the recordNumber'th 
// matching record from the recent search in the search path.
// ------------------------------------------------------------------

- (NSString*)usernameForSearchNodeRecordNumber:(unsigned long)recordNumber
{
    return [self getAttribute:kDSNAttrRecordName forRecordNumber:recordNumber];
}

// ------------------------------------------------------------------
// nodePathStrForSearchNodeRecordNumber:
//
// Retrieve the node path name string from the recordNumber'th 
// matching record from the recent search in the search path.
// ------------------------------------------------------------------

- (NSString*)nodePathStrForSearchNodeRecordNumber:(unsigned long)recordNumber
{
    return [self getAttribute:kDSNAttrMetaNodeLocation forRecordNumber:recordNumber];
}


// ============================================================================
// Public methods for accessing the Directory Service functions
// that this class provides.


// ------------------------------------------------------------------
// authenticateInNodePathStr: username: password:
//
// Takes the text string name of a node path and performs an
// authentication check on it for the specified username & passowrd.
// Similar to authOnNodePath: username: password:, except this one
// is optimized for short usernames only.  It skips searching the
// node for the user record and just sends checks for authetication
// based on the username specified in the arguments.
// ------------------------------------------------------------------

- (tDirStatus)authenticateInNodePathStr:(NSString*)nodePathStr username:(NSString*)inUsername
                password:(NSString*)inPassword
{
    tDataListPtr		nodePath	= nil;
    tDirNodeReference   userNode	= 0;
    tDirStatus			status		= eDSNoErr;

    nodePath = dsBuildFromPath(_DirRef, [nodePathStr cString], "/");
    if (doVerbose)
	{
		printf("---> dsOpenDirNode() on node Named: %s ... ", [nodePathStr cString]);
		fflush(stdout);
	}
	
    status = dsOpenDirNode(_DirRef, nodePath, &userNode);
    if (doVerbose)
	{
		printf("\t");
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}
    [self deallocateDataList:nodePath];
    
    if (status == eDSNoErr)
        status = [self authenticateInNode:userNode username:inUsername password:inPassword];

    dsCloseDirNode(userNode);
    return status;
}

// ------------------------------------------------------------------
// authenticateInNode: username: password:
//
// Takes a DirectoryServices node path reference and performs an
// authentication check on it for the specified username & passowrd.
// ------------------------------------------------------------------

- (tDirStatus)authenticateInNode:(tDirNodeReference)userNode username:(NSString*)inUsername
                password:(NSString*)inPassword
{
    NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
    tDataBufferPtr			step			= NULL;
    tDataBufferPtr			stepResponse	= NULL;    
    tDataNodePtr			authMethod		= NULL;
    long int				length			= 0;
    long int				current			= 0;
    tDirStatus				status			= 0;

    [self allocateDataBuffer:&step withNumberOfBlocks:4 shouldReallocate: NO];
    [self allocateDataBuffer:&stepResponse withNumberOfBlocks:4 shouldReallocate: NO];
    authMethod = dsDataNodeAllocateString(_DirRef, kDSStdAuthNodeNativeClearTextOK);

    length = strlen( [inUsername UTF8String] );
    memcpy( &(step->fBufferData[current]), &length, sizeof(long));
    current += sizeof(long);
    memcpy( &(step->fBufferData[current]), [inUsername UTF8String], length );
    current +=length;
    
    length = strlen( [inPassword UTF8String] );
    memcpy( &(step->fBufferData[current]), &length, sizeof(long));
    current += sizeof(long);
    memcpy( &(step->fBufferData[current]), [inPassword UTF8String], length );
    
    step->fBufferLength = current + length;
    
    if(doVerbose)
	{
		printf("---> dsDoDirNodeAuth() ......................... ");
		fflush(stdout);
	}
    status = dsDoDirNodeAuth(userNode, authMethod, 1, step, stepResponse, NULL);
    if(doVerbose)
	{
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}
    
    printf("Username: %s\nPassword: %s\n", [inUsername cString], [inPassword cString]);
    if(status == eDSNoErr)
		printf("Success");
    else
		[_dsStat printOutErrorMessage:"Error" withStatus:status];

    // Clean up allocated memory
    //dsCloseDirNode(userNode); // don't close since not opened here
    [self deallocateDataNode:authMethod];
    [self deallocateDataBuffer:step];
    [self deallocateDataBuffer:stepResponse];
    [pool release];
    return (status);
}

// ------------------------------------------------------------------
// authUserOnLocalNode: password:
//
// Takes the specified username and password and performs an
// authentication check for them on the local NetInfo node.
// ------------------------------------------------------------------

- (tDirStatus)authUserOnLocalNode:(NSString*)inUsername password:(NSString*)inPassword
{
    tDataBufferPtr		localNodeBuffer = nil;
    tDataListPtr		localNodeName   = nil;
    tDirNodeReference   localNodeRef	= 0;
    tDirStatus			status			= eDSNoErr;
    u_long				i				= 0;
	unsigned long		matchingUsers   = 0;
    
    localNodeBuffer = [self retrieveLocalNode];
    localNodeName   = [self getNodeNameForBuffer:localNodeBuffer];
    [self deallocateDataBuffer:localNodeBuffer];

    localNodeRef	= [self openDirNodeWithName:localNodeName];
    [self deallocateDataList:localNodeName];

    [self fetchRecordListOnNode:localNodeRef matchingName:inUsername];
	matchingUsers = [_MasterUserList count];
    for (i=0; i< matchingUsers; i++)
	{
		status = [self authenticateInNode:localNodeRef username:[self usernameForSearchNodeRecordNumber:i] password:inPassword];
		printf("\n");
    }
    dsCloseDirNode(localNodeRef);
    return status;
}

// ------------------------------------------------------------------
// authOnNodePath: username: password:
//
// Takes the text string name of a node path and performs an
// authentication check on it for the specified username & passowrd.
// Both long and short usernames can be used because it first
// searches the node for the user record.  Then it extracts
// the short username and uses that to check authentication.
// ------------------------------------------------------------------

- (tDirStatus)authOnNodePath:(NSString*)nodePathStr username:(NSString*)inUsername password:(NSString*)inPassword
{
    tDataListPtr		nodePath		= NULL;
    tDirNodeReference   userNode		= 0;
    tDirStatus			status			= 0;
    int					i				= 0;
	unsigned long		matchingUsers   = 0;

    nodePath = dsBuildFromPath(_DirRef, [nodePathStr cString], "/");

    if (doVerbose)
	{
		printf("---> dsOpenDirNode() on node Named: %s ... ", [nodePathStr cString]);
		fflush(stdout);
	}
    status = dsOpenDirNode(_DirRef, nodePath, &userNode);
    if (doVerbose)
	{
		printf("\t");
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}

    [self deallocateDataList:nodePath];

	// if we have an eDSNoErr on open, then we can continue...
	if( status == eDSNoErr )
	{
		[self fetchRecordListOnNode:userNode matchingName:inUsername];
		matchingUsers = [_MasterUserList count];
		for (i=0; i < matchingUsers; i++)
		{
			status = [self authenticateInNode:userNode username:[self usernameForSearchNodeRecordNumber:i] password:inPassword];
			printf("\n");
		}
		dsCloseDirNode(userNode);
	}
    
    return status;
}

// ------------------------------------------------------------------
// authUserOnSearchPath: password:
// 
// Finds all nodes in the search path that contain a user by the
// specified name. It then successively performs an authentication
// check on those nodes for the username and password specified.
// ------------------------------------------------------------------

- (tDirStatus)authUserOnSearchPath:(NSString*)inUsername password:(NSString*)inPassword
{
    tDataBufferPtr			searchNodeBuffer	= nil;
    tDataListPtr			searchNodeName		= nil;
    tDirStatus				status				= eDSNoErr;
    u_long					i					= 0;
    NSString			   *nodePathStr			= nil;
    NSString			   *realUsername		= nil;
    int						checkpwresult		= 1;
    char				   *cpwresultstr		= nil;
    NSAutoreleasePool      *pool				= nil;
	unsigned long			matchingUsers		= 0;

    NS_DURING
	searchNodeBuffer	= [self retrieveSearchPathNode];
	searchNodeName		= [self getNodeNameForBuffer:searchNodeBuffer];
	[self deallocateDataBuffer:searchNodeBuffer];
	searchNodeBuffer	= nil;
    
	_SearchNodeRef		= [self openDirNodeWithName:searchNodeName];
	[self deallocateDataList:searchNodeName];    
	searchNodeName		= nil;
	
	status = [self fetchRecordListOnNode:_SearchNodeRef matchingName:inUsername];
	matchingUsers = [_MasterUserList count];
	for (i=0 ; i < matchingUsers; i++)
	{
	    pool = [[NSAutoreleasePool alloc] init];
	    nodePathStr		= [self nodePathStrForSearchNodeRecordNumber:i];
	    realUsername	= [self usernameForSearchNodeRecordNumber:i];
		if ([self isMemberOfClass:[DSAuthenticate class]])
		{
			if (i == 0) //only need to do checkpw once since result will not change
			{
				checkpwresult = checkpw([realUsername cString], [inPassword cString]);
				switch (checkpwresult) {
					case CHECKPW_SUCCESS:
						cpwresultstr = "Success";
						break;
					case CHECKPW_UNKNOWNUSER:
						cpwresultstr = "Unknown User";
						break;
					case CHECKPW_BADPASSWORD:
						cpwresultstr = "Bad Password";
						break;
					case CHECKPW_FAILURE:
						cpwresultstr = "Failure";
						break;
				}
				printf("\nCall to checkpw(): %s", cpwresultstr);
			}
		}

	    printf("\n\npath: %s\n", [nodePathStr cString]);
		
	    status = [self authenticateInNodePathStr:nodePathStr username:realUsername password:inPassword];
		[pool release];
		pool = nil;
	}
	printf("\n");
	
    NS_HANDLER
	[self deallocateDataBuffer:searchNodeBuffer];
	[self deallocateDataList:searchNodeName];
	[localException retain];
	[pool release];
	[[localException autorelease] raise];
    NS_ENDHANDLER

    return status;
}

// ------------------------------------------------------------------
// getListOfNodesWithUser:
//
// Searches the entire search path for a user named by the 
// parameter.  It returns an NSArray of NSStrings of the names
// of the nodes.
// ------------------------------------------------------------------
- (NSArray*)getListOfNodesWithUser:(NSString*)inUsername
{
    NSMutableArray     *nodePathStringList  = [[NSMutableArray alloc] init];
    tDataBufferPtr		searchNodeBuffer	= nil;
    tDataListPtr		searchNodeName		= nil;
    u_long				i					= 0;
	unsigned long		matchingUsers		= 0;

    searchNodeBuffer	= [self retrieveSearchPathNode];
    searchNodeName		= [self getNodeNameForBuffer:searchNodeBuffer];
    [self deallocateDataBuffer:searchNodeBuffer];
    
    _SearchNodeRef		= [self openDirNodeWithName:searchNodeName];
    [self deallocateDataList:searchNodeName];
    
    [self fetchRecordListOnNode:_SearchNodeRef matchingName:inUsername];
    
	matchingUsers = [_MasterUserList count];
    for (i=0; i < matchingUsers; i++)
        [nodePathStringList addObject:[self nodePathStrForSearchNodeRecordNumber:i]];
        
    return (NSArray*)[nodePathStringList autorelease];
}

@end
