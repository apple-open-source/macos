/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * @header DSoNode
 */


#import "DSoNode.h"

#import <Security/Authorization.h>

#import <unistd.h>

#import "DSoBuffer.h"
#import "DSoDataNode.h"
#import "DSoDataList.h"
#import "DSoRecord.h"
#import "DSoUser.h"
#import "DSoGroup.h"
#import "DSoException.h"
#import "DSoAttributeUtils.h"

@interface DSoNode (DSoNodePrivate)
- (void) reopen;
- (BOOL) _hasRecordsOfType:(const char*)inType;

- (NSArray*) _findRecordsOfTypes: (NSArray*)inTypes
                   withAttribute: (const char*)inAttrib
                           value: (id)inValue
                       matchType: (tDirPatternMatch)inMatchType
              retrieveAttributes: (NSArray*)inAttribsToRetrieve
                     allowBinary: (BOOL)inAllowBinary;
@end

@implementation DSoNode

// ----------------------------------------------------------------------------
//	¥ DSoNode Protected Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** DSoNode Protected Instance Methods ****

// ctor and dtor are protected; instances should be created via FindNode().

- (id)init
{
    [super init];
    mDirectory          = nil;
    mNodeName           = nil;
    mNodeRef            = 0;
    mIsAuthenticated    = NO;
    mSupportsSetAttributeValues = YES;
    mTypeList = nil;
    return self;
}

- (id)initWithDir:(DSoDirectory*)inDir nodeRef:(tDirNodeReference)inNodeRef 
                  nodeName:(NSString*)inNodeName
{
    [self init];
    mNodeRef    = inNodeRef;
    [inDir verifiedDirRef];  // Since we don't actually need to keep the ref itself, verify it on creation 
    mDirectory  = [inDir retain];
    mNodeName   = [inNodeName retain];
    return self;
}

- (void) dealloc
{
    [mTypeList release];
    [mNodeName release];
    
    dsCloseDirNode (mNodeRef) ;
    [mDirectory release];
    [super dealloc];
}

- (void) finalize
{
    dsCloseDirNode (mNodeRef) ;
    [super finalize];
}

- (RecID) _findRecord: (NSString*)inName
            ofType: (const char*)inType
{
    DSoDataNode        *dnName = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory  string:inName] ;
    DSoDataNode        *dnType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inType] ;
    tRecordReference	rrTemp  = 0;
    tDirStatus			nError  = eDSNoErr;
    RecID               retValue;

    nError = dsOpenRecord (mNodeRef, [dnType dsDataNode], [dnName dsDataNode], &rrTemp);
    [dnName release];
    [dnType release];
    if (nError)
        [DSoException raiseWithStatus:nError] ;
        
    retValue.node = self;
    retValue.recordRef = rrTemp;

    return retValue;
}

// ----------------------------------------------------------------------------
//	¥ DSoNode Public Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** DSoNode Public Instance Methods ****

// ---------------------------------------------------------------
//	¥ Read Methods
// ---------------------------------------------------------------
#pragma mark ** Read Methods **

- (NSDictionary*) searchAttributes:(const char*)inAttributeType allowBinary:(BOOL)allowBinary
{
    DSoDataList            *attr        = nil;
    DSoBuffer              *bufAttrList = nil;
    tAttributeListRef       listRef     = 0;
    tContextData            context     = NULL;
    tDirStatus              err         = eDSNoErr;
    NSMutableDictionary    *attributes  = [[NSMutableDictionary alloc] init];
    unsigned long           count       = 0;
    unsigned int            baseSize    = 1024;
    
    @try
    {
        NSAutoreleasePool   *pool        = [NSAutoreleasePool new];

        // Tell dsGetDirNodeInfo to only get the requested attribute, or all if none specified.
        if (inAttributeType == nil)
            attr = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSAttributesAll];
        else
            attr = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:inAttributeType];
        
        bufAttrList = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:baseSize];
        do
        {
            err = dsGetDirNodeInfo(mNodeRef, [attr dsDataList], [bufAttrList dsDataBuffer],
                                   FALSE, (unsigned long *)&count, (tAttributeListRef *)&listRef, (tContextData *)&context);
            if (err == eDSBufferTooSmall)
            {
                [bufAttrList grow:[bufAttrList getBufferSize] + baseSize];
            }
            else if (err == eDSNoErr && count > 0)
            {
                NSDictionary *d = [DSoAttributeUtils getAttributesAndValuesInNode: self
                                                                       fromBuffer: bufAttrList
                                                                    listReference: listRef
                                                                            count: count
                                                                      allowBinary: allowBinary];
                [attributes addEntriesFromDictionary:d];
                dsCloseAttributeList(listRef);
                listRef = 0;
            }
        } 
        while (  (err == eDSNoErr && context != NULL) || (err == eDSBufferTooSmall));
        
        [pool drain];
        
    } @catch( NSException *exception ) {
        [attributes release];
        attributes = nil;
        @throw;
    } @finally {
        [attr release];
        [bufAttrList release];
    }

    if (err)
    {
        [attributes release];
        [DSoException raiseWithStatus:err];
    }

    if ([attributes count] == 0)
    {
        [attributes release];
        [DSoException raiseWithStatus:eDSAttributeNotFound];
    }
    
    return [attributes autorelease];
}

- (NSDictionary*) searchAttributes:(const char*)inAttributeType
{
    return [self searchAttributes: inAttributeType allowBinary: NO];
}

- (NSString*) getAttributeFirstValue:(const char*)inAttributeType
{
    return [self getAttributeFirstValue: inAttributeType allowBinary: NO];
}

- (id) getAttributeFirstValue:(const char *)inAttributeType allowBinary:(BOOL)allowBinary
{
    NSArray *values = [self getAttribute: inAttributeType allowBinary: allowBinary];
    
    return ([values count] ? [values objectAtIndex: 0] : nil);
}

- (NSArray*) getAttribute:(const char*)inAttributeType
{
    return [self getAttribute: inAttributeType allowBinary: NO];
}

- (NSArray*) getAttribute:(const char*)inAttributeType allowBinary:(BOOL)allowBinary
{
    NSAutoreleasePool      *pool        = [[NSAutoreleasePool alloc] init];
    NSDictionary           *d           = nil;
    NSArray                *attribute   = nil;
    
    d = [self searchAttributes: inAttributeType allowBinary: allowBinary];
    attribute = [[d objectForKey:[NSString stringWithUTF8String:inAttributeType]] retain];
    
    [pool drain];
    
    return [attribute autorelease];
}

- (NSDictionary*) getAllAttributes
{
    return [self searchAttributes: nil allowBinary: NO];
}

- (NSDictionary*) getAllAttributesAllowingBinary:(BOOL)allowBinary
{
    return [self searchAttributes: nil allowBinary: allowBinary];
}

// findRecordTypes
//
// First check kDSNAttrRecordType attribute in dsGetDirNodeInfo. If that is not
// available fall back to the legacy discovery method described below.
//
// For each Record type in our master list, call dsGetRecordList with just big enough
// of a buffer to obtain at least one record of that type.  
// If at least one record is found, then that type is contained in the node and is added
// to the list of types to return.
// Using a small buffer, means that the api call will return quicker, because it can't
// return as much.  We thrown away any context data.
//
// Right now we are making some assumptions about the fact that one record will always
// fit into the buffer size given, and that if the count found is zero, then context
// will be null.  This also will not handle the case of a eDSBufferTooSmall error.
- (NSArray*)		findRecordTypes
{
    NSMutableArray			*setOfTypes                 = nil;
    
    @try
    {
        @try
        {
            setOfTypes = [[[self getAttribute:kDSNAttrRecordType] mutableCopy] autorelease];
        } @catch( NSException *exception ) {
        
        }
        if ([setOfTypes count] == 0)
        {
            NSString			*iterType   = nil;
            unsigned long		iterCount   = 0;
            unsigned long       limitCount  = 0;
            setOfTypes = [NSMutableArray array];
            if (mTypeList == nil)
                mTypeList = [[mDirectory standardRecordTypes] retain];
            limitCount  = [mTypeList count];

            for (iterCount = 0 ; iterCount < limitCount; iterCount++)
            {
                iterType = [mTypeList objectAtIndex:iterCount];
                if ([self _hasRecordsOfType:[iterType UTF8String]])
                    [setOfTypes addObject:iterType];
            }
        }
        // Alphabetize the list.
        [setOfTypes sortUsingSelector:@selector(caseInsensitiveCompare:)];
    } @catch( NSException *exception ) {
        
    }
    
    return setOfTypes;
}

- (BOOL)	hasRecordsOfType:(const char*)inType
{
    NSString* dsType = [NSString stringWithUTF8String:inType];
    BOOL bHasType = NO;
    
    @try
    {
        if ([dsType hasPrefix:@kDSStdRecordTypePrefix])
        {
            bHasType = [[self getAttribute:kDSNAttrRecordType] containsObject:dsType];
        }
    } @catch( NSException *exception ) {
        // don't worry about the exception
    }

    if (!bHasType)
    {
        bHasType = [self _hasRecordsOfType:inType];
    }
    
    return bHasType;
}

- (NSArray*) findRecordNames:(NSString*)inName andAttributes:(NSArray*)inAttributes ofType:(const char*)inType matchType:(tDirPatternMatch)inMatchType
{
	if (inAttributes != nil)
	{
		if ([inAttributes containsObject:@kDSNAttrRecordName] == NO
            && [inAttributes containsObject:@kDSAttributesAll] == NO
            && [inAttributes containsObject:@kDSAttributesStandardAll] == NO)
			inAttributes = [inAttributes arrayByAddingObject:@kDSNAttrRecordName];
	}
	return [self _findRecordsOfTypes: [NSArray arrayWithObject:[NSString stringWithUTF8String:inType]]
                       withAttribute: nil 
                               value: inName
                           matchType: inMatchType
                  retrieveAttributes: inAttributes
                         allowBinary: NO];
}

- (NSArray*) findRecordNames:(NSString*)inName ofType:(const char*)inType matchType:(tDirPatternMatch)inMatchType
{
	return [self _findRecordsOfTypes: [NSArray arrayWithObject:[NSString stringWithUTF8String:inType]]
                       withAttribute: nil 
                               value: inName
                           matchType: inMatchType
                  retrieveAttributes: nil
                         allowBinary: NO];
}

- (NSArray*) findRecordNamesOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType
{
	return [self _findRecordsOfTypes: inTypes
                       withAttribute: inAttrib
                               value: inValue
                           matchType: inMatchType
                  retrieveAttributes: nil
                         allowBinary: NO];
}

- (NSArray*) findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType allowBinary:(BOOL)inAllowBinary
{
	return [self _findRecordsOfTypes: inTypes
                       withAttribute: inAttrib
                               value: inValue
                           matchType: inMatchType
                  retrieveAttributes: [NSArray arrayWithObject:@kDSAttributesAll]
                         allowBinary: inAllowBinary];
}

- (NSArray*) findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType
{
	return [self _findRecordsOfTypes: inTypes
                       withAttribute: inAttrib
                               value: inValue
                           matchType: inMatchType
                  retrieveAttributes: [NSArray arrayWithObject:@kDSAttributesAll]
                         allowBinary: NO];
}

- (NSArray*) findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType retrieveAttributes:(NSArray*)inAttribsToRetrieve allowBinary:(BOOL)inAllowBinary
{
	return [self _findRecordsOfTypes: inTypes
                       withAttribute: inAttrib
                               value: inValue
                           matchType: inMatchType
                  retrieveAttributes: inAttribsToRetrieve
                         allowBinary: inAllowBinary];
}

- (NSArray*) findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType retrieveAttributes:(NSArray*)inAttribsToRetrieve
{
	return [self _findRecordsOfTypes: inTypes
                       withAttribute: inAttrib
                               value: inValue
                           matchType: inMatchType
                  retrieveAttributes: inAttribsToRetrieve
                         allowBinary: NO];
}

- (DSoRecord*)		findRecord:(NSString*)inName ofType:(const char*)inType
{
    RecID	rec = [self _findRecord:inName ofType:inType];
    return [[[DSoRecord alloc] initInNode:rec.node recordRef:rec.recordRef type:inType] autorelease] ; 
}

- (DSoUser*)		findUser:(NSString*)inName
{
    RecID	rec = [self _findRecord:inName ofType:kDSStdRecordTypeUsers];
    return [[[DSoUser alloc] initInNode:rec.node recordRef:rec.recordRef type:kDSStdRecordTypeUsers] autorelease] ; 
}

- (DSoGroup*)		findGroup:(NSString*)inName
{
    RecID	rec = [self _findRecord:inName ofType:kDSStdRecordTypeGroups];
    return [[[DSoGroup alloc] initInNode:rec.node recordRef:rec.recordRef type:kDSStdRecordTypeGroups] autorelease] ; 
}

- (DSoGroup*)		adminGroup
{
    return [self findGroup:@"admin"];
}

// ---------------------------------------------------------------
//	¥ Write Methods
// ---------------------------------------------------------------
#pragma mark ** Write Methods **

- (DSoRecord*)		newRecord:(NSString*)inName ofType:(const char*)inType
{
    return [[[DSoRecord alloc] initInNode:self type:inType name:inName] autorelease];
}

// ---------------------------------------------------------------
//	¥ Other Methods
// ---------------------------------------------------------------
#pragma mark ** Other Methods **

- (tDirStatus)		authenticateName:(NSString*)inName
                      withPassword: (NSString*)inPasswd
{
    return [self authenticateName:inName withPassword:inPasswd authOnly:YES];
}

- (tDirStatus)		authenticateName:(NSString*)inName
                      withPassword: (NSString*)inPasswd
                      authOnly: (BOOL)inAuthOnly
{
	NSArray *nameAndPassword = [[NSArray alloc] initWithObjects:inName, inPasswd, nil];
	tDirStatus status = [self authenticateWithBufferItems: nameAndPassword
												 authType: kDSStdAuthNodeNativeClearTextOK
												 authOnly: inAuthOnly];
	[nameAndPassword release];
	return status;
}

- (tDirStatus) authenticateWithBufferItems: (NSArray*)inBufferItems
														  authType: (const char*)inAuthType
														  authOnly: (BOOL)inAuthOnly
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	size_t              ulCurrentLength = 0;
	DSoBuffer		   *dbAuth          = nil;
	DSoBuffer		   *dbStep          = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:2048] ;
	DSoDataNode		   *dnAuthType      = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAuthType] ;
	NSEnumerator	   *enumBuffer      = [inBufferItems objectEnumerator];
	NSString		   *enumItem        = nil;
	tDirStatus          authStatus      = eDSNoErr;
	unsigned int        itemCount       = [inBufferItems count];
	register char	   *cpBuff          = nil;

	// Calculate the sum of the lengths of all the string items in the buffer list
	while (enumItem = (NSString*)[enumBuffer nextObject])
		ulCurrentLength += strlen([enumItem UTF8String]);

	// Now allocate a buffer big enough to pack all the items into
	dbAuth = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:(itemCount*4 + ulCurrentLength)];

	// Get ready to pack the buffer using a direct memcpy() method
	// instead of the DSoBuffer accessor methods to optimize the operation
	cpBuff = ([dbAuth dsDataBuffer])->fBufferData;
	enumBuffer = [inBufferItems objectEnumerator];
	
	// Pack the Buffer with the items in the list
	while (enumItem = (NSString*)[enumBuffer nextObject])
	{
        const char *utf8String = [enumItem UTF8String];
        
		ulCurrentLength = strlen( utf8String );
		memcpy (cpBuff, &ulCurrentLength, sizeof (ulCurrentLength)) ;
		cpBuff += sizeof (ulCurrentLength) ;
		memcpy (cpBuff, utf8String, ulCurrentLength) ;
		cpBuff += ulCurrentLength ;
	}
	// Since we precalculated the size of the buffer to be the exact necessary
	// length, set the length to the size.
	[dbAuth setDataLength:[dbAuth getBufferSize]];

	// Authenticate the user.
	authStatus = dsDoDirNodeAuth (mNodeRef, [dnAuthType dsDataNode], inAuthOnly, [dbAuth dsDataBuffer], [dbStep dsDataBuffer], 0);
	if (authStatus == eDSNoErr && !inAuthOnly)
		mIsAuthenticated = YES ;
	
	[dnAuthType release];
	[dbStep release];
	[dbAuth release];
	[pool drain];
	return authStatus;
}

- (tDirStatus) authenticateWithBufferItems: (NSArray*)inBufferItems
                                  authType: (const char*)inAuthType
                                  authOnly: (BOOL)inAuthOnly
					   responseBufferItems: (NSArray**)outBufferItems
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	size_t			ulCurrentLength = 0;
	DSoBuffer		*dbAuth = nil;
	DSoBuffer		*dbStep = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:2048] ;
	DSoDataNode		*dnAuthType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAuthType] ;
	NSEnumerator	*enumBuffer = [inBufferItems objectEnumerator];
	NSString		*enumItem = nil;
	tDirStatus		authStatus = eDSNoErr;
	unsigned int itemCount = [inBufferItems count];
	register char	*cpBuff = nil;
	unsigned int offset = 0, buffLen = 0;
	tDirStatus		tResult	= eDSNoErr;

	// Calculate the sum of the lengths of all the string items in the buffer list
	while (enumItem = (NSString*)[enumBuffer nextObject])
		ulCurrentLength += strlen([enumItem UTF8String]);

	// Now allocate a buffer big enough to pack all the items into
	dbAuth = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:(itemCount*4 + ulCurrentLength)];

	// Get ready to pack the buffer using a direct memcpy() method
	// instead of the DSoBuffer accessor methods to optimize the operation
	cpBuff = ([dbAuth dsDataBuffer])->fBufferData;
	enumBuffer = [inBufferItems objectEnumerator];

	// Pack the Buffer with the items in the list
	while (enumItem = (NSString*)[enumBuffer nextObject])
	{
        const char *utf8String = [enumItem UTF8String];
        
		ulCurrentLength = strlen( utf8String );
		memcpy (cpBuff, &ulCurrentLength, sizeof (ulCurrentLength)) ;
		cpBuff += sizeof (ulCurrentLength) ;
		memcpy (cpBuff, utf8String, ulCurrentLength) ;
		cpBuff += ulCurrentLength ;
	}
	// Since we precalculated the size of the buffer to be the exact necessary
	// length, set the length to the size.
	[dbAuth setDataLength:[dbAuth getBufferSize]];

	// Authenticate the user.
	authStatus = dsDoDirNodeAuth (mNodeRef, [dnAuthType dsDataNode], inAuthOnly, [dbAuth dsDataBuffer], [dbStep dsDataBuffer], 0);
	if (authStatus == eDSNoErr && !inAuthOnly)
		mIsAuthenticated = YES ;

	if (outBufferItems != nil) {
		NSMutableArray* array = [NSMutableArray new];
		NSString* string = nil;
		cpBuff = ([dbStep dsDataBuffer])->fBufferData;
		buffLen = ([dbStep dsDataBuffer])->fBufferLength;
		while ( (offset < buffLen) && (tResult == eDSNoErr) )
		{
			if (offset + sizeof( unsigned long ) > buffLen)
			{
				tResult = eDSInvalidBuffFormat;
				break;
			}
			memcpy( &ulCurrentLength, cpBuff, sizeof( unsigned long ) );
			cpBuff += sizeof( unsigned long );
			offset += sizeof( unsigned long );
			if (ulCurrentLength + offset > buffLen)
			{
				tResult = eDSInvalidBuffFormat;
				break;
			}
	
			// Node size is: struct size + string length + null term byte
			string = (NSString*)CFMakeCollectable(CFStringCreateWithBytes(NULL,cpBuff,ulCurrentLength,
														kCFStringEncodingUTF8,false));
			cpBuff += ulCurrentLength;
			offset += ulCurrentLength;
			if (string == nil) {
				tResult = eDSInvalidBuffFormat;
				break;
			}
			[array addObject:string];
		}
		if (tResult == eDSNoErr)
			*outBufferItems = array;
		else
			[array release];
	}

	[dnAuthType release];
	[dbStep release];
	[dbAuth release];
	[pool drain];
	return authStatus;
}

- (tDirStatus)customCall:(int)number inputData:(NSData*)inputData
                                    outputData:(NSMutableData*)outputData
{
    long status = eDSNoErr;
    tDataBuffer* customBuff1 = NULL;
    tDataBuffer* customBuff2 = NULL;

	do {
		customBuff1 = dsDataNodeAllocateBlock( 0, [inputData length] + 1,
			[inputData length], (char*)[inputData bytes]);
		if (customBuff1 == 0) break;
		customBuff2 = dsDataBufferAllocate( 0, [outputData length] + 1 );
		if (customBuff2 == 0) break;
	
		status = dsDoPlugInCustomCall( mNodeRef, number, customBuff1, customBuff2 );	
		if (status != eDSNoErr) break;
		
		if (outputData != nil) {
			[outputData setData:[NSData dataWithBytes:customBuff2->fBufferData 
				length:customBuff2->fBufferLength]];
		}
	} while (false);
	
	//clean up allocations
	if (customBuff1 != NULL)
	{
		dsDataBufferDeAllocate( 0, customBuff1 );
		customBuff1 = NULL;
	}
	if (customBuff2 != NULL)
	{
		dsDataBufferDeAllocate( 0, customBuff2 );
		customBuff2 = NULL;
	}
	return status;
}

- (tDirStatus)customCall:(int)number
	sendPropertyList:(id)propList 
	withAuthorization:(void*)authExternalForm
{
	NSData* data = (NSData*)CFPropertyListCreateXMLData(kCFAllocatorDefault,
								(CFPropertyListRef)propList);
	tDirStatus dsStatus = [self customCall:number sendData:data 
								withAuthorization:authExternalForm];
	[data release];
	return dsStatus;
}

- (tDirStatus)customCall:(int)number
	withAuthorization:(void*)authExternalForm
{
	return [self customCall:number sendData:nil withAuthorization:authExternalForm];
}

- (tDirStatus)customCall:(int)number
	sendData:(NSData*)data 
	withAuthorization:(void*)authExternalForm
{
	tDirStatus dsStatus = eDSNoErr;
	NSMutableData* inputData = [[NSMutableData alloc] 
			initWithCapacity:[data length] + sizeof(AuthorizationExternalForm)];
	[inputData appendBytes:authExternalForm length:sizeof(AuthorizationExternalForm)];
	if (data != nil)
	{
		[inputData appendData:data];
	}
	dsStatus = [self customCall:number inputData:inputData outputData:nil];
	[inputData release];
	return dsStatus;
}

- (tDirStatus)customCall:(int)number
	sendItems:(NSArray*)items 
	outputData:(NSMutableData*)outputData
{
	tDirStatus dsStatus = eDSNoErr;
	unsigned long itemSize = 0;
	NSMutableData* inputData = [NSMutableData new];
	NSEnumerator* itemEnum = [items objectEnumerator];
	NSObject* item = nil;
	
	while (item = [itemEnum nextObject])
	{
		if ([item isKindOfClass:[NSString class]])
		{
			const char* utf8String = [(NSString*)item UTF8String];
			if (utf8String != nil)
			{
				itemSize = strlen(utf8String);
			}
			[inputData appendBytes:&itemSize length:sizeof(itemSize)];
			if (itemSize > 0)
			{
				[inputData appendBytes:utf8String length:itemSize];
			}
		}
		else if ([item isKindOfClass:[NSData class]])
		{
			itemSize = [(NSData*)item length];
			[inputData appendBytes:&itemSize length:sizeof(itemSize)];
			if (itemSize > 0)
			{
				[inputData appendData:(NSData*)item];
			}
		}
	}
	  
	dsStatus = [self customCall:number inputData:inputData outputData:outputData];
	[inputData release];
	return dsStatus;
}

- (tDirStatus)customCall:(int)number
	receiveData:(NSMutableData*)outputData 
	withAuthorization:(void*)authExternalForm
	sizeCall:(int)sizeNumber
{
	CFIndex bufferSize = 0;
	NSData* inputData = [[NSData alloc] initWithBytes:authExternalForm
							length:sizeof(AuthorizationExternalForm)];
	NSMutableData* sizeData = [NSMutableData new];
	[sizeData appendBytes:&bufferSize length:sizeof(bufferSize)];
	tDirStatus dsStatus = [self customCall:sizeNumber inputData:inputData 
								outputData:sizeData];
	if (dsStatus == eDSNoErr)
	{
		memcpy(&bufferSize,[sizeData bytes],sizeof(bufferSize));
		[outputData setLength:bufferSize];
		dsStatus = [self customCall:number inputData:inputData 
						 outputData:outputData];
	}
	[sizeData release];
	[inputData release];
	return dsStatus;
}

- (NSString*)getName
{
    return mNodeName;
}

- (DSoDirectory*)	directory
{
    return mDirectory;
}

- (tDirNodeReference)dsNodeReference
{
    return mNodeRef;
}

- (BOOL)usesMultiThreaded
{
	return NO;
}

- (void)setUsesMultiThreaded:(BOOL)inValue
{
}

- (BOOL)supportsSetAttributeValues
{
    return mSupportsSetAttributeValues;
}

- (void)setSupportsSetAttributeValues:(BOOL)inValue
{
    mSupportsSetAttributeValues = inValue;
}

- (NSString*)description
{
	return [NSString stringWithFormat:@"%@ {\n\tname: %@\n\tnode ref: %ld\n\tauthenticated: %s\n}\n",
[super description], mNodeName, mNodeRef, mIsAuthenticated ? "yes" : "no"];
}

@end

@implementation DSoSearchNode

// ----------------------------------------------------------------------------
//	¥ DSoSearchNode Protected Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** DSoSearchNode Protected Instance Methods ****

/* be careful that this method ONLY retrieves a single record (the first found)*/
- (RecID) _findRecord: (NSString*)inName
            ofType: (const char*)inType
{
	DSoDataList        *dlNames     = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory string:inName];
	DSoDataList        *dlTypes     = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:inType];
	DSoDataList        *dlAttrs     = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory
                                        cStrings:kDSNAttrMetaNodeLocation, kDSNAttrRecordName, 0] ;
	DSoBuffer          *dbData      = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:1024];
	tDirStatus			nError      = eDSNoErr;
	unsigned long		ulCount     = 1 ;
	tAttributeListRef	refAttrs    = 0 ;
	tRecordEntryPtr		recInfo     = NULL ;
	NSString           *sNode       = nil;
	NSString           *sName       = nil;
	DSoNode            *nodeHome    = nil;
	DSoDataNode        *dnName      = nil;
	DSoDataNode        *dnType      = nil;
	tRecordReference	rrTemp      = 0;
    RecID				retValue;
	tContextData		context     = NULL;

	do
	{
		nError = dsGetRecordList (mNodeRef, [dbData dsDataBuffer], [dlNames dsDataList], eDSiExact,
							[dlTypes dsDataList], [dlAttrs dsDataList], false, &ulCount, &context);
		if (nError == eDSBufferTooSmall)
			[dbData grow:[dbData getBufferSize]*2];
	} while ( (nError == eDSBufferTooSmall) || (nError == eDSNoErr && ulCount == 0 && context != nil) );

    [dlNames release];
    [dlTypes release];
    [dlAttrs release];

	if (nError == eDSNoErr && ulCount == 0)
		nError = eDSRecordNotFound;
	
	if (nError)
	{
		[dbData release];
		[DSoException raiseWithStatus:nError];
	}
	    
    // Get the attribute values for the first record from the data buffer.
    if (nError = dsGetRecordEntry(mNodeRef, [dbData dsDataBuffer], 1, &refAttrs, &recInfo))
    {
		[dbData release];
		[DSoException raiseWithStatus:nError];
    }
    ulCount = recInfo->fRecordAttributeCount ; // Re-purpose the use of ulCount
    dsDeallocRecordEntry([mDirectory dsDirRef] , recInfo);
    recInfo = nil;

    for ( ; ulCount ; ulCount--)
	{
        tAttributeValueListRef		refValue    = 0 ;
        tAttributeEntryPtr			attrInfo    = NULL ;
        tAttributeValueEntryPtr		value       = NULL ;
        unsigned long               ulUsed      = 0;
        const char                 *szpData     = nil;

        if (nError = dsGetAttributeEntry (mNodeRef, [dbData dsDataBuffer],
                            refAttrs, ulCount, &refValue, &attrInfo))
        {
            [dbData release];
            [DSoException raiseWithStatus:nError];
        }

		if (attrInfo != NULL)
		{
			if (nError = dsGetAttributeValue (mNodeRef, [dbData dsDataBuffer],
									1, refValue, &value))
			{
				[dbData release];
				dsCloseAttributeValueList(refValue);
				dsDeallocAttributeEntry([mDirectory dsDirRef], attrInfo);
				attrInfo = NULL;
				[DSoException raiseWithStatus:nError];
			}
			if (value == NULL)
			{
				[dbData release];
				dsCloseAttributeValueList(refValue);
				dsDeallocAttributeEntry([mDirectory dsDirRef], attrInfo);
				attrInfo = NULL;
				[DSoException raiseWithStatus:eDSAttributeDoesNotExist];
			}
	
			// Break the attribute into manageable variables.
			ulUsed = value->fAttributeValueData.fBufferLength + 1 ;
			szpData = value->fAttributeValueData.fBufferData ; 
			// Match the attribute type so we can associate it properly.
			if (!strcmp (attrInfo->fAttributeSignature.fBufferData,
								kDSNAttrMetaNodeLocation)) {
				sNode = [[NSString alloc] initWithUTF8String:szpData];
			} else if (!strcmp (attrInfo->fAttributeSignature.fBufferData,
								kDSNAttrRecordName)) {
				sName = [[NSString alloc] initWithUTF8String:szpData];
			}
			dsDeallocAttributeValueEntry([mDirectory dsDirRef], value);
			value = NULL;
			dsCloseAttributeValueList(refValue);
			dsDeallocAttributeEntry([mDirectory dsDirRef], attrInfo);
			attrInfo = NULL;
		}
    }
    if (refAttrs != 0)
    {
        dsCloseAttributeList(refAttrs);
    }
    [dbData release];

    nError = eDSRecordNotFound; //reset below if dsOpenRecord succeeds
    if (sNode != NULL)
    {
        nodeHome = [mDirectory findNode:sNode]; // already autoreleased
        [sNode release];
    }
    if (sName != NULL)
    {
        dnName = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory string:sName];
        [sName release];
    }
    if (inType != nil)
    {
        dnType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inType];
    }
    if ( (nodeHome != NULL) && (dnName != NULL) && (dnType != NULL) )
    {
        nError = dsOpenRecord ([nodeHome dsNodeReference], [dnType dsDataNode], [dnName dsDataNode], &rrTemp);
        [dnName release];
        dnName = NULL;
        [dnType release];
        dnType = NULL;
    }

    if (nError)
    {
        if (dnName != NULL)
        {
            [dnName release];
        }
        if (dnType != NULL)
        {
            [dnType release];
        }
        [DSoException raiseWithStatus:nError];
    }

    retValue.node       = nodeHome;
    retValue.recordRef  = rrTemp;
    return retValue;
}

@end

@implementation DSoNode (DSoNodePrivate)

- (NSArray*)		_findRecordsOfTypes: (NSArray*)inTypes
                          withAttribute: (const char*)inAttrib
                                  value: (id)inValue
                              matchType: (tDirPatternMatch)inMatchType
                     retrieveAttributes: (NSArray*)inAttribsToRetrieve
                            allowBinary: (BOOL)inAllowBinary;
{

	DSoBuffer		   *dbData              = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:4096];
	DSoDataList		   *dlRecordTypes       = [[DSoDataList alloc] initWithDir:mDirectory strings:inTypes];
	DSoDataNode		   *dnAttribType        = nil;
	id                  dnSearchValue       = nil;
	DSoDataList		   *dlAttribsToRetrieve = nil;
	tContextData        context             = 0;
	tDirStatus          status              = eDSNoErr;
	tAttributeListRef   refAttrs            = 0;
	tRecordEntryPtr		recInfo             = nil ;
	unsigned long       i                   = 0;
    unsigned long       ulCount             = 0;
	NSMutableArray     *results             = [NSMutableArray array];

	if (inAttrib != nil)
	{
		dnAttribType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttrib];
		dnSearchValue = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory value:inValue];
	}
	else
		dnSearchValue = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory value:inValue];

	if (inAttribsToRetrieve == nil)
		dlAttribsToRetrieve = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSNAttrRecordName];
	else
		dlAttribsToRetrieve = [[DSoDataList alloc] initWithDir:mDirectory strings:inAttribsToRetrieve];
	
	@try
    {
		do {
			if (inAttrib == nil)
				status = dsGetRecordList(mNodeRef, [dbData dsDataBuffer], [dnSearchValue dsDataList], inMatchType,
                                [dlRecordTypes dsDataList], [dlAttribsToRetrieve dsDataList], (inAttribsToRetrieve == nil),
                                (unsigned long *)&ulCount, (tContextData*) &context);
			else if (dlAttribsToRetrieve == nil)
				status = dsDoAttributeValueSearch(mNodeRef, [dbData dsDataBuffer], [dlRecordTypes dsDataList],
                                [dnAttribType dsDataNode], inMatchType, [dnSearchValue dsDataNode],
                                (unsigned long *)&ulCount, (tContextData*) &context);
			else
				status = dsDoAttributeValueSearchWithData(mNodeRef, [dbData dsDataBuffer], [dlRecordTypes dsDataList],
                                [dnAttribType dsDataNode], inMatchType, [dnSearchValue dsDataNode],
                                [dlAttribsToRetrieve dsDataList], NO, (unsigned long *)&ulCount, (tContextData*) &context);
			
			if (status == eDSBufferTooSmall)
			{
				[dbData grow:[dbData getBufferSize]*2];
				continue;
			}
			else if (status != eDSNoErr)
			{
				[DSoException raiseWithStatus:status];
			}
			for (i = 1; i <= ulCount; i++)
			{
				status = dsGetRecordEntry (mNodeRef, [dbData dsDataBuffer], i, (tAttributeListRef *)&refAttrs, (tRecordEntryPtr *)&recInfo);
				if (status == eDSNoErr)
				{
                    @try
                    {
                        if (inAttribsToRetrieve == nil)
                        {
                            char *recName = nil;
                            dsGetRecordNameFromEntry(recInfo, &recName);
                            [results addObject:[NSString stringWithUTF8String:recName]];
                            free(recName);
                        }
                        else
                        {
                            NSDictionary *attribsAndValues = nil;
                            
                            attribsAndValues = [DSoAttributeUtils getAttributesAndValuesInNode:self fromBuffer:dbData
                                                                                 listReference:refAttrs count:recInfo->fRecordAttributeCount allowBinary:inAllowBinary];

                            if ([inAttribsToRetrieve containsObject:kDSOAttrRecordType])
                            {
                                char *cRecType = nil;
                                NSString *recType = nil;
                                dsGetRecordTypeFromEntry(recInfo, &cRecType);
                                recType = [[NSString alloc] initWithCString:cRecType];
                                attribsAndValues = [[attribsAndValues mutableCopy] autorelease];
                                [(NSMutableDictionary*)attribsAndValues setObject:recType forKey:kDSOAttrRecordType];
                                [recType release];
                                free(cRecType);
                            }
                            [results addObject:attribsAndValues];
                        }
                    } @catch( NSException *exception ) {
                        @throw;
                    } @finally {
                        dsDeallocRecordEntry([mDirectory dsDirRef], recInfo);
                        recInfo = NULL;
                        dsCloseAttributeList(refAttrs);
                        refAttrs = 0;
                    }
				}
				else
					[DSoException raiseWithStatus:status];
			}
		} while ((status == eDSBufferTooSmall) || (status == eDSNoErr && context != nil));
		
	} @catch( NSException *exception ) {
        @throw;
    } @finally {
		if (context != nil)
        {
			dsReleaseContinueData(mNodeRef, context);
            context = 0;
        }
        [dbData release];
        [dlRecordTypes release];
        [dnAttribType release];
        [dnSearchValue release];
        [dlAttribsToRetrieve release];
    }
	
	return results;
}

- (BOOL)	_hasRecordsOfType:(const char*)inType
{
	DSoDataList        *dlNames     = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSRecordsAll];
	DSoDataList        *dlTypes     = nil;
	DSoDataList        *dlAttrs     = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSNAttrRecordName] ;
	DSoBuffer          *dbData      = [(DSoBuffer*)[DSoBuffer alloc] initWithDir:mDirectory bufferSize:256];
	tDirStatus			nError      = eDSNoErr;
	unsigned long		ulCount     = 1;
	tContextData		context     = NULL;
	BOOL				bHasType    = NO;

	@try
    {
		dlTypes = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:inType];
		do {

			nError = dsGetRecordList (mNodeRef, [dbData dsDataBuffer], [dlNames dsDataList], eDSExact,
								[dlTypes dsDataList], [dlAttrs dsDataList], TRUE, (unsigned long *)&ulCount, (tContextData *)&context);
			if (nError == eDSBufferTooSmall)
			{
				[dbData grow:[dbData getBufferSize]*2];
			}
			else if (nError != eDSNoErr)
			{
				if (nError != eDSInvalidRecordType)
					[DSoException raiseWithStatus:nError];
			}
			else if (ulCount > 0)
			{
				bHasType = YES;
				break;
			}

		} while ( (nError == eDSBufferTooSmall) || (context != nil) );

	} @catch( NSException *exception ) {
        @throw;
    } @finally {
		if (context != nil)
		{
			dsReleaseContinueData(mNodeRef, context);
			context = 0;
		}
		[dlAttrs release];
		[dlNames release];
		[dlTypes release];
		[dbData release];
    }

	return bHasType;
}

- (void) reopen
{
    tDirStatus      nError      = eDSNoErr;
    DSoDataList    *dlNodeName  = [[DSoDataList alloc] initWithDir:mDirectory separator:'/' pattern:mNodeName];
	
    dsCloseDirNode(mNodeRef);
    mNodeRef = 0;
    nError = dsOpenDirNode ([mDirectory dsDirRef], [dlNodeName dsDataList], &mNodeRef);
    [dlNodeName release];
    if (nError)
		[DSoException raiseWithStatus:nError];
}
@end
