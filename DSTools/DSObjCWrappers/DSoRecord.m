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
 * @header DSoRecord
 */


#import "DSoRecord.h"
#import "DSoRecordPriv.h"

#import "DSoNode.h"
#import "DSoBuffer.h"
#import "DSoDataNode.h"
#import "DSoDataList.h"
#import "DSoGroup.h"
#import "DSoUser.h"
#import "DSoException.h"
#import "DSoAttributeUtils.h"

@class DSoUser, DSoGroup;

#define DS_API_IS_UNSUPPORTED(err)  (err == eNotHandledByThisNode \
                                     || err == eNotYetImplemented \
                                     || err == eNoLongerSupported \
                                     || err == eUnknownAPICall) 

@implementation DSoRecord

// ----------------------------------------------------------------------------
//	¥ DSRecord Public Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** Public Instance Methods ****

// ctor & dtor

- (id)init
{
    [super init];
    mDirectory  = nil;
    mParent		= nil;
    mName		= nil;
    mRecRef		= 0;
    return self;
}

- (DSoRecord*)initInNode:(DSoNode*)inParent recordRef:(tRecordReference)inRecRef type:(const char*)inType
{
	DSoRecord *returnRec = self;
	// If the type is a known type for which we have a more specialized sub-class,
	// then return an object of that type.
	if (!strcmp(inType, kDSStdRecordTypeUsers))
		returnRec = [[DSoUser alloc] initInNode:inParent recordRef:inRecRef];
	else if (!strcmp(inType, kDSStdRecordTypeGroups))
		returnRec = [[DSoGroup alloc] initInNode:inParent recordRef:inRecRef];
	else
	{
		[self initInNode:inParent recordRef:inRecRef];
		mType = [[NSString alloc] initWithCString:inType];
	}
	if (returnRec != self)
		[self release];
	
    return returnRec;
}

- (DSoRecord*)initInNode:(DSoNode*)inParent type:(const char*)inType name:(NSString*)inName
{
	return [self initInNode:inParent type:inType name:inName create:YES];
}

- (void)dealloc
{
    if (mRecRef)
        dsCloseRecord(mRecRef);
    [mParent release];
    [mName release];
    [mType release];
    [super dealloc];
}

- (void)finalize
{
    if (mRecRef)
        dsCloseRecord(mRecRef);
    [super finalize];
}

// Inline accessors.
- (NSString*)getName
{
    return mName;
}

- (const char*)getType
{
    return [mType UTF8String];
}

- (DSoNode*)node
{
    return mParent;
}

	// Casting operators.
- (tRecordReference)dsRecordReference
{
    return mRecRef;
}

/******************
 * attributeCount *
 ******************/
- (unsigned long)attributeCount
{
    tDirStatus		nError		= eDSNoErr;
    tRecordEntryPtr recordInfo  = nil;
    unsigned long   count		= 0;
    
    if (nError = dsGetRecordReferenceInfo(mRecRef, &recordInfo))
        [DSoException raiseWithStatus:nError];
    
    count = recordInfo->fRecordAttributeCount;
    
    if (nError = dsDeallocRecordEntry([[mParent directory] dsDirRef], recordInfo))
        [DSoException raiseWithStatus:nError];
    
    return count;
}
 
/****************
 * GetAttribute *
 ****************/

- (NSArray*)getAllAttributes
{
    return [self _getAllAttributesIncludeValues:NO];
}

- (NSDictionary*)getAllAttributesAndValues
{
    return [self _getAllAttributesIncludeValues:YES];
}

- (NSDictionary*)getAttributes:(NSArray*)inAttributes
{
    return [self _getAttributes:inAttributes includeValues:YES];
}

- (NSString*)getAttribute:(const char*)inAttributeType
{
    return [self getAttribute: inAttributeType allowBinary: NO];
}

- (id)getAttribute:(const char*)inAttributeType allowBinary:(BOOL)inAllowBinary
{
	id  returnValue = nil;

	// we want to catch the exception here, as the user is just asking for the attribute.
	// If there isn't one, we should just return nil
	@try
    {
		returnValue = [self getAttribute: inAttributeType index: 1 allowBinary: inAllowBinary];
    } @catch( NSException *exception ) {
        // ignore all exceptions
    }
	
	return returnValue;
}

- (NSString*)getAttribute:(const char*)inAttributeType index:(unsigned long)inIndex
{
    return [self getAttribute: inAttributeType index: inIndex allowBinary: NO];
}

- (id)getAttribute:(const char*)inAttributeType index:(unsigned long)inIndex allowBinary:(BOOL)inAllowBinary
{
	tDirStatus				err ;
	DSRef					dirRef = [mDirectory verifiedDirRef];
	tAttributeValueEntryPtr	pAttrVal = nil;
    DSoDataNode				*type = nil;
    NSString				*sValue = nil;
    
    type = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
    err = dsGetRecordAttributeValueByIndex (mRecRef, [type dsDataNode], inIndex, &pAttrVal);
    [type release];
    if (err)
        [DSoException raiseWithStatus:err];
    
    sValue = [DSoAttributeUtils getAttributeFromBuffer: &pAttrVal->fAttributeValueData allowBinary: inAllowBinary];
    dsDeallocAttributeValueEntry (dirRef, pAttrVal) ;
    return sValue;
}

/*********************
 * GetAttributeRange *
 *********************/
// get a list of attribute values of given type in given range
- (NSArray*)getAttribute:(const char*)inAttributeType range:(NSRange)inRange
{
    return [self getAttribute: inAttributeType range: inRange allowBinary: NO];
}

- (NSArray*)getAttribute:(const char*)inAttributeType range:(NSRange)inRange allowBinary:(BOOL)inAllowBinary
{
	tDirStatus					err			= eDSNoErr;
	DSRef						dirRef		= [mDirectory verifiedDirRef];
	tAttributeValueEntryPtr		pAttrVal	= nil;
    unsigned long				i			= 0;
    DSoDataNode				   *type		= nil;
    NSMutableArray			   *valueList   = [[NSMutableArray alloc] initWithCapacity:inRange.length];
    
    type = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];

    for (i = inRange.location; i < (inRange.location + inRange.length); i++)
    {
        err = dsGetRecordAttributeValueByIndex (mRecRef, [type dsDataNode], i, &pAttrVal);
        if (err)
        {
            [valueList release];
            [type release];
            [DSoException raiseWithStatus:err];
        }
        
        [valueList addObject: [DSoAttributeUtils getAttributeFromBuffer: &pAttrVal->fAttributeValueData allowBinary: inAllowBinary]];
        dsDeallocAttributeValueEntry (dirRef, pAttrVal) ;
        pAttrVal = nil;
    }
    [type release];
    return [valueList autorelease];
}


/*************************
* getAttributeValueCount *
**************************/
- (unsigned long)getAttributeValueCount:(const char*)inAttributeType
{
    tAttributeEntryPtr		pAttrEntry  = nil;
    DSoDataNode			   *attrType	= [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
    tDirStatus				err			= eDSNoErr;
    unsigned long			valueCount  = 0;

    err = dsGetRecordAttributeInfo(mRecRef, [attrType dsDataNode], &pAttrEntry);
    [attrType release];
    if (err)
        [DSoException raiseWithStatus:err];
    valueCount = pAttrEntry->fAttributeValueCount;
    dsDeallocAttributeEntry([mDirectory dsDirRef], pAttrEntry);
    return valueCount;
}

/*****************
 * SetAttributes *
 *****************/
- (void)setAttribute:(const char*)inAttributeType value:(id)inAttributeValue
{
	// this call can throw, make an autorelease array so it gets released
    [self setAttribute:inAttributeType values:[NSArray arrayWithObject:inAttributeValue]];
}

- (void)setAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues
{
    tDirStatus				err				= eDSNoErr;
    DSoDataNode			   *attrType		= nil;
    DSoDataNode			   *attrValue		= nil;
    DSoDataList			   *attrValuesList  = nil;
    register unsigned int   i				= 0;
    unsigned long			count			= [inAttributeValues count];
    tAttributeEntryPtr		entryPtr		= nil;
    tAttributeValueEntryPtr attrPtr			= nil;
    tDataNodePtr			attrTypeNodePtr = NULL;
    DSRef					dirRef			= 0;

    // Make sure the directory is valid.
    
    dirRef = [mDirectory verifiedDirRef];
    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
    attrTypeNodePtr = [attrType dsDataNode]; // convenience pointer to avoid a lot of method calls.
    
    if ([mParent supportsSetAttributeValues])
    {
        attrValuesList = [[DSoDataList alloc] initWithDir: mDirectory values: inAttributeValues];
        err = dsSetAttributeValues (mRecRef, attrTypeNodePtr, [attrValuesList dsDataList]);
        [attrValuesList release];
        if (DS_API_IS_UNSUPPORTED(err)) 
        {
            // not supported by this node, fall back to the old approach
            [mParent setSupportsSetAttributeValues:NO];
        }
        else if (err != eDSEmptyDataList)
        {
            [attrType release];
            if (err)
                [DSoException raiseWithStatus:err];
            
            dsFlushRecord( mRecRef );
            return;
        }
    }
	
    // initialize the first value:
    attrValue = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory value:[inAttributeValues objectAtIndex:0]];
    
	// For simplicity, try to just remove current attribute if it exists
    @try
    {
        err = dsRemoveAttribute (mRecRef, attrTypeNodePtr) ;
        // If the attribute can't be removed then try to delete all but the
        // first value and replace the first value with the new first value
        if (err) {
            
            err = dsGetRecordAttributeInfo (mRecRef, attrTypeNodePtr, (tAttributeEntryPtr *) &entryPtr);
            if (!err && entryPtr->fAttributeValueCount > 0)
            {
                // Delete all but the first value.
                for (i = 2 ; i <= entryPtr->fAttributeValueCount; i++)
                {
                    err = dsGetRecordAttributeValueByIndex (mRecRef, attrTypeNodePtr, 1, (tAttributeValueEntryPtr *) &attrPtr);
                    if (!err)
                    {
                        err = dsRemoveAttributeValue(mRecRef, attrTypeNodePtr,
                                                     attrPtr->fAttributeValueID);
                        dsDeallocAttributeValueEntry(dirRef, attrPtr);
                        attrPtr = nil;
                    }
                    if (err)
                        [DSoException raiseWithStatus:err];
                }
                // Now replace the first value with the new first value
                err = dsGetRecordAttributeValueByIndex (mRecRef, attrTypeNodePtr, 1, (tAttributeValueEntryPtr *) &attrPtr);
                if (!err)
                {
					NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]; // Want this method to be memory contained
                    id              firstObject = [inAttributeValues objectAtIndex:0];
                    const char      *newFirstValue = NULL;
                    unsigned long   newFirstValueLen = 0;
                    tAttributeValueEntryPtr newValueEntry;
                    
                    if( [firstObject isKindOfClass:[NSString class]] )
                    {
                        newFirstValue = [firstObject UTF8String];
                        newFirstValueLen = strlen( newFirstValue );
                    }
                    else if( [firstObject isKindOfClass:[NSData class]] )
                    {
                        newFirstValue = [firstObject bytes];
                        newFirstValueLen = [firstObject length];
                    }
                    else
                    {
                        @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoRecord setAttribute:values:] values contained non NSData nor NSString" userInfo: nil];
                    }

					newValueEntry = dsAllocAttributeValueEntry(dirRef, attrPtr->fAttributeValueID, (void *)newFirstValue, newFirstValueLen );
					// We're done with attrPtr, dealloc it
                    dsDeallocAttributeValueEntry(dirRef, attrPtr);
					attrPtr = nil;

					// Set the first value, then dealloc the valueEntry item.
                    err = dsSetAttributeValue(mRecRef, attrTypeNodePtr, newValueEntry);
					dsDeallocAttributeValueEntry(dirRef, newValueEntry);
					[pool drain];
                }
                if (err)
                    [DSoException raiseWithStatus:err];
            }
            else {
                // if there was an error getting the record attribute info, then maybe the attribute
                // didn't ever exist and we can just add it.
                err = dsAddAttribute (mRecRef, attrTypeNodePtr, 0, [attrValue dsDataNode]);
                if (err)
                    [DSoException raiseWithStatus:err];
            }
            dsDeallocAttributeEntry(dirRef, entryPtr);
            entryPtr = nil;
        }
        else
        {
            // The former attribute was successfully removed, add the first new attribute.
            err = dsAddAttribute (mRecRef, [attrType dsDataNode], 0, [attrValue dsDataNode]);
            if (err)
                [DSoException raiseWithStatus:err];
        }
    } @catch( NSException *exception ) {
        [attrType release]; // need to release this cause we don't release until the end normally
        @throw;
    } @finally {
        if (attrPtr != nil)
            dsDeallocAttributeValueEntry(dirRef, attrPtr);
        if (entryPtr != nil)
            dsDeallocAttributeEntry(dirRef, entryPtr);
        [attrValue release];
    }
    
    for (i = 1; i < count; i++)
    {
        DSoDataNode *dnValue = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory value:[inAttributeValues objectAtIndex:i]];
        err = dsAddAttributeValue (mRecRef, attrTypeNodePtr, [dnValue dsDataNode]);
        [dnValue release];
        if (err)
        {
            [attrType release];
            [DSoException raiseWithStatus:err];
        }
    }
    
    dsFlushRecord( mRecRef );
    
    [attrType release];
}

/*****************
 * AddAttributes *
 *****************/
// Add each attribute to the attribute list; if that particular attribute value
// exists and mergeValues == true, don't add a duplicate
- (void)addAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues
{
    [self addAttribute:inAttributeType values:inAttributeValues mergeValues:YES];
}

- (void)addAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues mergeValues:(BOOL)inMergVals
{
	register unsigned int		i			= 0;
    tDirStatus					err			= eDSNoErr ;
    DSoDataNode				   *attrType	= nil;
    unsigned long				count		= [inAttributeValues count];
    
    [mDirectory verifiedDirRef];
    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];

	for (i = 0; i < count; i++)
    {
		if (!inMergVals
            || ![self attributeExists:inAttributeType withValue:[inAttributeValues objectAtIndex:i]]) {
			DSoDataNode	*dnValue;
            dnValue = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory value:[inAttributeValues objectAtIndex:i]];
			err = dsAddAttributeValue (mRecRef, [attrType dsDataNode], [dnValue dsDataNode]);
            [dnValue release];
            if (err)
            {
                [attrType release];
                [DSoException raiseWithStatus:err];
            }
		}
    }

    dsFlushRecord( mRecRef );

    [attrType release];
}

/*******************
 * ChangeAttribute *
 *******************/
// change the attribute whose value is inAttrValue to inNewAttrValue
- (void)changeAttribute:(const char*)inAttributeType oldValue:(NSString*)inAttrValue newValue:(id)inNewAttrValue
{
	tDirStatus					err			= eDSNoErr;
	tAttributeValueEntryPtr		attrValuePtr= nil;
	tAttributeValueEntryPtr		newValue	= nil;
	DSRef						dirRef		= [mDirectory verifiedDirRef];
    DSoDataNode				   *attrType	= nil;
	unsigned long				newValueLen = 0;
    const char                 *newValuePtr = NULL;
    
    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
	@try
    {
		attrValuePtr = [self getAttrValuePtrForTypeNode:attrType value:inAttrValue];
    } @catch (NSException *exception ) {
		[attrType release];
        @throw;
    }
    
    if( [inNewAttrValue isKindOfClass:[NSString class]] )
    {
        newValuePtr = [inNewAttrValue UTF8String];
        newValueLen = strlen( newValuePtr );
    }
    else if( [inNewAttrValue isKindOfClass:[NSData class]] )
    {
        newValuePtr = [inNewAttrValue bytes];
        newValueLen = [inNewAttrValue length];
    }
    else
    {
		[attrType release];
        @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoRecord changeAttribute:oldValue:newValue:] new value was not NSData nor NSString" userInfo: nil];
    }
    
    newValue = dsAllocAttributeValueEntry( dirRef, attrValuePtr->fAttributeValueID, (void *)newValuePtr, newValueLen ) ;
	dsDeallocAttributeValueEntry (dirRef, attrValuePtr) ;
	if (newValue == nil)
    {
        [attrType release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
	err = dsSetAttributeValue (mRecRef, [attrType dsDataNode], newValue);
    [attrType release];
	dsDeallocAttributeValueEntry (dirRef, newValue) ;
    if (err)
        [DSoException raiseWithStatus:err];
    
    dsFlushRecord( mRecRef );
}

- (void)changeAttribute:(const char*)inAttributeType index:(unsigned int)inIndex newValue:(id)inNewAttrValue
{
	tDirStatus					err			= eDSNoErr;
	tAttributeValueEntryPtr		attrValuePtr= nil;
	tAttributeValueEntryPtr		newValue	= nil ;
	DSRef						dirRef		= [mDirectory verifiedDirRef];
    DSoDataNode				   *attrType	= nil;
	const char				   *newValuePtr = NULL;
	unsigned long				newValueLen = 0;
    
    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
	err = dsGetRecordAttributeValueByIndex (mRecRef, [attrType dsDataNode],
													inIndex, &attrValuePtr);
    if (err)
    {
        [attrType release];
        [DSoException raiseWithStatus:err];
    }

    if( [inNewAttrValue isKindOfClass:[NSString class]] )
    {
        newValuePtr = [inNewAttrValue UTF8String];
        newValueLen = strlen( newValuePtr );
    }
    else if( [inNewAttrValue isKindOfClass:[NSData class]] )
    {
        newValuePtr = [inNewAttrValue bytes];
        newValueLen = [inNewAttrValue length];
    }
    else
    {
        [attrType release];
        @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoRecord changeAttribute:index:newValue:] value was not NSData nor NSString" userInfo: nil];
    }
    
    newValue = dsAllocAttributeValueEntry( dirRef, attrValuePtr->fAttributeValueID, (void *)newValuePtr, newValueLen ) ;
	dsDeallocAttributeValueEntry (dirRef, attrValuePtr) ;
	if (newValue == nil)
    {
        [attrType release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
	err = dsSetAttributeValue (mRecRef, [attrType dsDataNode], newValue);
    [attrType release];
    
	dsDeallocAttributeValueEntry (dirRef, newValue) ;

    if (err)
        [DSoException raiseWithStatus:err];
    
    dsFlushRecord( mRecRef );
}

/*******************
 * AttributeExists *
 *******************/
- (BOOL)attributeExists:(const char*)inAttributeType withValue:(id)inAttributeValue
{
	DSRef						dirRef		= [mDirectory verifiedDirRef];
	DSoDataNode				   *attrType	= nil;
	tAttributeValueEntryPtr		attrPtr		= nil;
	BOOL						bExists		= NO ;

	if (!inAttributeValue || [inAttributeValue length] == 0)
		return NO;

    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
    @try
    {
		attrPtr = [self getAttrValuePtrForTypeNode:attrType value:inAttributeValue];
		dsDeallocAttributeValueEntry (dirRef, attrPtr) ;
		bExists = YES ;
    } @catch (NSException *exception ) {
        // ignore any exceptions here.
    }

    [attrType release];
    return bExists;
}

- (void)removeAttribute:(const char*)inAttributeType
{
    DSoDataNode    *attrType	= nil;
    tDirStatus		nError		= eDSNoErr;

    if (!inAttributeType || inAttributeType[0] == '\0')
        return;
    
    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
    nError = dsRemoveAttribute (mRecRef, [attrType dsDataNode]) ;
    [attrType release];
    if (nError)
        [DSoException raiseWithStatus:nError];
    dsFlushRecord( mRecRef );
}

- (void)removeAttribute:(const char*)inAttributeType value:(id)inAttributeValue
{
    // this call can throw, make an autorelease array so it gets released
    [self removeAttribute:inAttributeType values:[NSArray arrayWithObject:inAttributeValue]];
}

- (void)removeAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues
{
    tDirStatus					nError			= eDSNoErr;
    tAttributeValueEntryPtr		attrValuePtr	= nil;
    DSRef						dirRef			= [mDirectory verifiedDirRef];
    DSoDataNode				   *attrType		= nil;
    int							i				= 0;
	int							cnt				= 0;

    if (inAttributeValues == nil)
        return;
    
    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
	cnt = [inAttributeValues count];
    for (i = 0; i < cnt; i++)
    {
		@try
        {
			attrValuePtr = [self getAttrValuePtrForTypeNode:attrType value:[inAttributeValues objectAtIndex:i]];
        } @catch( NSException *exception ) {
			[attrType release];
            @throw;
        }
		
        nError = dsRemoveAttributeValue (mRecRef, [attrType dsDataNode], attrValuePtr->fAttributeValueID);
        dsDeallocAttributeValueEntry (dirRef, attrValuePtr) ;
    }
    [attrType release];
    if (nError)
        [DSoException raiseWithStatus:nError];
    dsFlushRecord( mRecRef );
}

- (void)removeAttribute:(const char*)inAttributeType index:(unsigned int)inIndex
{
    tDirStatus					nError			= eDSNoErr;
    tAttributeValueEntryPtr		attrValuePtr	= nil;
    DSRef						dirRef			= [mDirectory verifiedDirRef];
    DSoDataNode				   *attrType		= nil;

    attrType = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inAttributeType];
    nError = dsGetRecordAttributeValueByIndex (mRecRef, [attrType dsDataNode],
                                            inIndex, &attrValuePtr);
    if (nError)
    {
        [attrType release];
        [DSoException raiseWithStatus:nError];
    }

    nError = dsRemoveAttributeValue (mRecRef, [attrType dsDataNode], attrValuePtr->fAttributeValueID);
    dsDeallocAttributeValueEntry (dirRef, attrValuePtr);
    [attrType release];
    if (nError)
        [DSoException raiseWithStatus:nError];
    dsFlushRecord( mRecRef );
}

- (void)removeRecord
{
    tDirStatus		err ;
	
    if (err = dsDeleteRecord (mRecRef))
        [DSoException raiseWithStatus:err];
    // mRecRef will now be invalid... should I set it to 0 to trigger any asserts on anything that tries to use it?
}

@end
#pragma mark

// ----------------------------------------------------------------------------
// Protected Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** Protected Instance Methods ****

@implementation DSoRecord (DSoRecordPrivate)

- (DSoRecord*)initInNode:(DSoNode*)inParent recordRef:(tRecordReference)inRecRef
{
    [self init];
    mParent		= [inParent retain];
    mDirectory  = [inParent directory];
    mRecRef		= inRecRef;
    mName		= [[self getAttribute:kDSNAttrRecordName] retain];
    return self;
}

- (DSoRecord*)initInNode:(DSoNode*)inParent type:(const char*)inType name:(NSString*)inName create:(BOOL)inShouldCreate
{
    tDirStatus nError;

    [self init];

    mDirectory = [inParent directory];
	if (inShouldCreate)
	{
		DSoDataNode *type, *name;
		type = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory cString:inType];
		name = [(DSoDataNode*)[DSoDataNode alloc] initWithDir:mDirectory string:inName];
		nError = dsCreateRecordAndOpen ([inParent dsNodeReference], [type dsDataNode],
								  [name dsDataNode], &mRecRef) ;
		[type release];
		[name release];
		if (nError)
			[DSoException raiseWithStatus:nError];
	}
    mName = [inName retain];
    mParent = [inParent retain];
    mType = [[NSString alloc] initWithCString:inType];
    
    if( [mType isEqualToString:@kDSStdRecordTypeUsers] ||
        [mType isEqualToString:@kDSStdRecordTypeGroups] ||
        [mType isEqualToString:@kDSStdRecordTypeComputerLists] ||
        [mType isEqualToString:@kDSStdRecordTypeComputers] )
    {
        CFUUIDRef uuidRef = CFUUIDCreate( NULL );
        NSString* uuidString = [(NSString*)CFUUIDCreateString( NULL, uuidRef ) autorelease];
        CFRelease( uuidRef );
        [self setAttribute:kDS1AttrGeneratedUID value:uuidString];
    }
    
    return self;
}

/*
 * _getAllAttributesIncludeValues:
 *
 * This method makes the following assumptions:
 * 1) The short names (RecordName) of all records in the node for this record
 *	are unique.
 * 2) dsGetRecordList() will always return first the record whose RecordName 
 *	matches the record search name criteria, and any records whose RealNames
 *	match the search name critera will never be returned first.
 */
- (id)_getAllAttributesIncludeValues:(BOOL)inIncludeVals
{
    return [self _getAttributes:[NSArray arrayWithObject:@kDSAttributesAll] includeValues:inIncludeVals];
}

- (id)_getAttributes:(NSArray*)inAttributes includeValues:(BOOL)inIncludeVals
{
    tContextData 		localcontext	= 0;
    tRecordEntryPtr		pRecEntry		= nil;
    tAttributeListRef	attrListRef		= 0;
    DSoBuffer		   *recordBuf		= nil;
    DSoDataList		   *recName			= [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory string:mName];
    DSoDataList		   *recType			= [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory string:mType];
    DSoDataList		   *attrType		= [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory values:inAttributes];
    id					userAttributes  = nil;
    tDirStatus 			err				= eDSNoErr;
    UInt32              returnCount		= 0;
    UInt32              foundCount		= 0;

    // default to an 8k buffer initially, the inline DS buffer is 16k so plenty of room
    recordBuf = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:8192];

    do {
        err = dsGetRecordList([mParent dsNodeReference], [recordBuf dsDataBuffer], [recName dsDataList], eDSExact, [recType dsDataList], [attrType dsDataList], !inIncludeVals, &returnCount, &localcontext);
        if (!err && returnCount > 0)
        {
            err = dsGetRecordEntry([mParent dsNodeReference],[recordBuf dsDataBuffer],1,&attrListRef, &pRecEntry );
            foundCount += returnCount;
            if (inIncludeVals)
            {
                userAttributes = [DSoAttributeUtils getAttributesAndValuesInNode: mParent
													fromBuffer: recordBuf
													listReference: attrListRef
													count: pRecEntry->fRecordAttributeCount];
            }
            else
            {
                userAttributes = [DSoAttributeUtils getAttributesInNode: mParent
													fromBuffer: recordBuf
													listReference: attrListRef
													count: pRecEntry->fRecordAttributeCount];
            }
			if (pRecEntry != NULL)
			{
				dsDeallocRecordEntry([mDirectory dsDirRef], pRecEntry);
			}
			if (attrListRef != 0)
			{
				dsCloseAttributeList(attrListRef);
			}
        }
        else if (err == eDSBufferTooSmall)
        {
            [recordBuf grow:2 * [recordBuf getBufferSize]];
        }
    } while (err == eDSBufferTooSmall || (err == eDSNoErr && localcontext != 0));
    
    if (localcontext != 0)
        dsReleaseContinueData([mParent dsNodeReference], localcontext);
    
    [recordBuf release];
    [recName release];
    [attrType release];
    [recType release];
    if (err)
        [DSoException raiseWithStatus:err];
    else if (foundCount != 1)
        [[DSoException name:@"eDSInvalidRecordName"
                     reason:@"DSoRecord's getAllAttributes failed because more than one matching name was found in the node.  This should never happen."
                     status:eDSInvalidRecordName] raise];
    
    return userAttributes;
}

/***************************
* _GetAttrValuePtrByValue *
***************************/
- (tAttributeValueEntryPtr) getAttrValuePtrForTypeNode:(DSoDataNode*)inAttrType value:(id)inAttrValue
{
    register unsigned int	i = 1 ;
    tDirStatus				err ;
    tAttributeEntryPtr		entryPtr ;
    tAttributeValueEntryPtr	attrPtr ;
    DSRef					dirRef = [mDirectory dsDirRef];
    const char              *pAttrValue = nil;
    int                     pAttrValueLen = 0;

    if( [inAttrValue isKindOfClass:[NSString class]] ) {
        pAttrValue = [inAttrValue UTF8String];
        pAttrValueLen = strlen( pAttrValue );
    } else if( [inAttrValue isKindOfClass:[NSData class]] ) {
        pAttrValue = [inAttrValue bytes];
        pAttrValueLen = [inAttrValue length];
    } else {
        @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoRecord getAttrValuePtrForTypeNode:] value was not NSString nor NSData" userInfo: nil];
    }

    if (err = dsGetRecordAttributeInfo (mRecRef, [inAttrType dsDataNode], &entryPtr))
        [DSoException raiseWithStatus:err];

#warning can use dsGetAttributeValueByValue instead of looping values

    while (i <= entryPtr->fAttributeValueCount) {
        // walk through the list of values for this attribute
        if (err = dsGetRecordAttributeValueByIndex (mRecRef, [inAttrType dsDataNode], i++,
                                                    &attrPtr))
            continue ;
        // if the lengths are the same and the contents are the same
        if (pAttrValueLen == attrPtr->fAttributeValueData.fBufferLength && 
            memcmp(pAttrValue, attrPtr->fAttributeValueData.fBufferData, attrPtr->fAttributeValueData.fBufferLength) == 0) {
    
            dsDeallocAttributeEntry (dirRef, entryPtr) ;
            return attrPtr ;
        }
        dsDeallocAttributeValueEntry (dirRef, attrPtr) ;
    }
    dsDeallocAttributeEntry (dirRef, entryPtr) ;
    [DSoException raiseWithStatus:eDSAttributeNotFound];
    return nil;   // For the sake of the compiler not complaining
}


@end
