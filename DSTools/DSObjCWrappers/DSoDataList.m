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
 * @header DSoDataList
 */


#import "DSoDataList.h"

#import "DSoDataNode.h"
#import "DSoException.h"

@implementation DSoDataList

- (id)init
{
    [super init];
    mDir = nil;
    return self;
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir value:(id)inValue
{
    @try 
    {
        if( self = [super init] )
        {
            tDirReference   dsRef   = [inDir dsDirRef];
            tDirStatus      nError  = eDSNoErr;
            
            if( [inValue isKindOfClass:[NSString class]] )
            {
                nError = dsAppendStringToListAlloc( dsRef, &mList, [inValue UTF8String] );
            }
            else if( [inValue isKindOfClass:[NSData class]] )
            {
                tDataNodePtr newNode = dsDataNodeAllocateBlock( dsRef, [inValue length], [inValue length], (tBuffer) [inValue bytes] );
                
                nError = dsDataListInsertAfter( dsRef, &mList, newNode, 0 );
                
                // we deallocate the datanode, cause the list creates a copy
                dsDataBufferDeAllocate( dsRef, newNode ); // don't capture error here
            }
            else
            {
                @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoDataList initWithDir:value:] value is not a valid NSString nor NSData" userInfo: nil];
            }
            
            if( nError != eDSNoErr ) {
                [DSoException raiseWithStatus: nError];
            }        
            
            mDir = [inDir retain];
        }        
    } @catch( NSException *exception ) {
        [self release];
        @throw;
    }
    
    return self;
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir string:(NSString*)inString
{
    return [self initWithDir: inDir value: inString];
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir values:(NSArray*)inValues
{
    @try
    {
        NSAutoreleasePool   *pool		= [NSAutoreleasePool new];
        NSEnumerator        *valEnum    = [inValues reverseObjectEnumerator]; // go in reverse
        tDirReference       dsRef       = [inDir dsDirRef];
        id                  object;
        tDirStatus			nError		= eDSNoErr;

        [super init];
        
        while( object = [valEnum nextObject] )
        {
            tDataNodePtr    newNode = nil;

            if( [object isKindOfClass:[NSString class]] )
            {
                newNode = dsDataNodeAllocateString( dsRef, [object UTF8String] );
            }
            else if( [object isKindOfClass:[NSData class]] )
            {
                newNode = dsDataNodeAllocateBlock( dsRef, [object length], [object length], (tBuffer) [object bytes] );
            }
            else
            {
                @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoDataList initWithDir:values:] values contains non NSString nor NSData" userInfo: nil];
            }
            
            // we build the list in reverse so we always add to the head of the list
            nError = dsDataListInsertAfter( dsRef, &mList, newNode, 0 );
            
            // we deallocate the datanode, cause the list creates a copy
            dsDataBufferDeAllocate( dsRef, newNode ); 
            
            if( nError != eDSNoErr ) {
                [DSoException raiseWithStatus: nError];
            }
        }
        
        [pool drain];

        mDir = [inDir retain];

    } @catch( NSException *exception ) {
        [self release];
        @throw;
    }
    
    return self;
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir strings:(NSArray*)inStrings
{
    return [self initWithDir: inDir values: inStrings];
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir cString:(const char*)inString
{
    return [self initWithDir:inDir cStrings: inString, NULL];
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir cStrings:(const char *)inString, ...
{
    tDirStatus  nError  = eDSNoErr;
	va_list		args; 
	
    va_start (args, inString) ;
    
    [self init];
    
    if (inString != NULL)
    {
        tDirReference   dsRef = [inDir dsDirRef];
        nError = dsBuildListFromStringsAllocV (dsRef, &mList, inString, args) ;
    }
        
    va_end (args) ;
    if (nError) {
        [self release];
        [DSoException raiseWithStatus:nError];
    }
    mDir = [inDir retain];
    return self;
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir separator:(char)inSep pattern:(NSString*)inPattern
{
    char		szSep[] = { inSep, '\0' } ;
    
    [self init];
    
    tDataList *tempList = dsBuildFromPath ([inDir dsDirRef], [inPattern UTF8String], szSep) ;
    if (tempList == NULL) {
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
    mList = *tempList;
    free( tempList );

    mDir = [inDir retain];
    
    return self;
}


- (DSoDataList*)initWithDataList:(DSoDataList*)inOrg
{
    [self init];
    
    mDir = inOrg->mDir;
    
    tDataList *tempList = dsDataListCopyList ([mDir dsDirRef], [inOrg dsDataList]) ;
    
    if (tempList == NULL) {
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
    mList = *tempList;
    free( tempList );
    
    [mDir retain]; // Since we didn't error, now we retain the object.
    return self;
}

- (DSoDataList*)initWithDir:(DSoDirectory*)inDir dsDataList:(tDataListPtr)inList
{
    [self init];
 
    mDir = [inDir retain];
    
	if (inList)
    {
        tDataList *tempList = dsDataListCopyList( [inDir dsDirRef], inList );
        
        if( tempList == NULL )
        {
            [self release];
            [DSoException raiseWithStatus:eDSAllocationFailed];
        }
        
        mList = *tempList;
        free( tempList );
    } 
        
    return self;
}

- (void)dealloc
{
    dsDataListDeallocate ([mDir dsDirRef], &mList) ;
    [mDir release];
    [super dealloc];
}

- (void)finalize
{
    dsDataListDeallocate ([mDir dsDirRef], &mList) ;
    [super finalize];
}

- (unsigned long)getCount
{
    return dsDataListGetNodeCount (&mList) ;
}

- (unsigned long)getDataLength
{
    return dsGetDataLength (&mList) ;
}

- (DSoDataNode*)objectAtIndex:(unsigned long)inIndex
{
    tDataNodePtr	dnTemp  = nil;
    tDirStatus		nError  = dsDataListGetNodeAlloc ([mDir dsDirRef], &mList, inIndex, &dnTemp);
	
    if (nError)
        [DSoException raiseWithStatus:nError] ;
    return [[[DSoDataNode alloc] initWithDir:mDir dsDataNode:dnTemp] autorelease]; 
}

- (void)append:(id)inValue
{
    tDirStatus      nError  = eDSNoErr;
    tDataNodePtr    newNode = nil;
    tDirReference   dsRef   = [mDir dsDirRef];
    
    if( [inValue isKindOfClass:[NSString class]] )
    {
        newNode = dsDataNodeAllocateString( dsRef, [inValue UTF8String] );
    }
    else if( [inValue isKindOfClass:[NSData class]] )
    {
        newNode = dsDataNodeAllocateBlock( dsRef, [inValue length], [inValue length], (tBuffer) [inValue bytes] );
    }
    else
    {
        @throw [NSException exceptionWithName: NSInvalidArgumentException reason:@"[DSoDataList append:] value is not a valid NSString nor NSData" userInfo:nil];
    }
    
    nError = dsDataListInsertAfter( dsRef, &mList, newNode, dsDataListGetNodeCount(&mList) );
    if( nError != eDSNoErr ) {
        [DSoException raiseWithStatus: nError];
    }
}

- (tDataListPtr)dsDataList
{
    return &mList;
}

@end
