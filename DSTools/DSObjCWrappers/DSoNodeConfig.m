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
 * @header DSoNodeConfig
 */


#import "DSoNodeConfig.h"

#import "DSoBuffer.h"
#import "DSoDirectory.h"
#import "DSoRecord.h"
#import "DSoAttributeUtils.h"
#import "DSoDataList.h"
#import "DSoException.h"
#import "DSoRecordPriv.h"

#include <Security/Authorization.h>
#include <DirectoryService/DirServicesConstPriv.h>

@implementation DSoNodeConfig

- (DSoNodeConfig*)init
{
	[super init];
	[mTypeList release];
	mTypeList = [[NSArray alloc] initWithObjects:
				@kDSStdRecordTypePlugins,
				@kDSStdRecordTypeAttributeTypes,
				@kDSStdRecordTypeRecordTypes,
		nil];
	return self;
}

- (DSoNodeConfig*)initWithDir:(DSoDirectory*)inDir
{
    tDataListPtr	dlpNodeName = nil;
    DSRef			dirRef		= 0;
    DSoBuffer      *bufNodeList = nil;
    const char     *cNodeName   = nil;
    UInt32			nodeCount   = 0;
    tDirStatus		nError		= eDSNoErr;
	tContextData	context		= 0;
	
    [self init];

    dirRef = [inDir verifiedDirRef];
    bufNodeList = [[DSoBuffer alloc] initWithDir:inDir];
	do {
		nError = dsFindDirNodes (dirRef, [bufNodeList dsDataBuffer], NULL,
								 eDSConfigNodeName, &nodeCount, &context) ;
		if (nError == eDSBufferTooSmall) {
			[bufNodeList grow: [bufNodeList getBufferSize] * 2];
		}
	} while (nError == eDSBufferTooSmall);
	
	if (context != 0) {
		dsReleaseContinueData(dirRef, context);
	}

    nError = dsGetDirNodeName (dirRef, [bufNodeList dsDataBuffer], 1, &dlpNodeName);

    cNodeName = dsGetPathFromList (dirRef, dlpNodeName, "/") ;
    mNodeName = [[NSString alloc] initWithCString:cNodeName];
    free ((void *) cNodeName) ;

    nError = dsOpenDirNode (dirRef, dlpNodeName, &mNodeRef) ;
    dsDataListDeallocate (dirRef, dlpNodeName) ;
    free (dlpNodeName) ;

    mDirectory = [inDir retain];
    [bufNodeList release];

    return self;
}

- (DSoRecord*) findRecord:(NSString*)inName ofType:(const char*)inType
{
    return [[[DSoRecord alloc] initInNode:self type:inType name:inName create:NO] autorelease] ;
}

- (NSArray*) getPluginList
{
    return [self findRecordNames:@kDSRecordsAll
					ofType:kDSStdRecordTypePlugins
					matchType:eDSExact];
}

- (NSDictionary*)getAttributesAndValuesForPlugin:(NSString*)inPluginName
{
    tContextData 		localcontext		= 0;
    tRecordEntryPtr		pRecEntry			= nil;
    tAttributeListRef	attrListRef			= 0;
    DSoDataList		   *recName				= nil;
    DSoDataList		   *recType				= [[DSoDataList alloc] initWithDir:mDirectory cString:kDSStdRecordTypePlugins];
    DSoDataList		   *attrType			= [[DSoDataList alloc] initWithDir:mDirectory cString:kDSAttributesAll];
    DSoBuffer		   *recordBuf			= [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:4096];
    id					pluginAttributes	= nil;
    tDirStatus 			err					= eDSNoErr;
    UInt32				i					= 0;
	UInt32				returnCount			= 0;
	UInt32				totalCount			= 0;
	UInt16				len					= 0;
	
	id sysVersion = [self getAttribute: kDS1AttrOperatingSystemVersion];
	if (sysVersion == nil || [sysVersion isEqual: @"10.6"]) {
		recName = [[DSoDataList alloc] initWithDir:mDirectory cString:kDSRecordsAll];
	}
	else {
		recName = [[DSoDataList alloc] initWithDir:mDirectory cString:[inPluginName UTF8String]];
	}

	do {
		err = dsGetRecordList([self dsNodeReference], [recordBuf dsDataBuffer], [recName dsDataList], eDSExact, [recType dsDataList],
							  [attrType dsDataList], FALSE, &returnCount, &localcontext);
		if (err == eDSBufferTooSmall) {
			[recordBuf grow: [recordBuf getBufferSize] * 2];
			continue;
		}
		
		for (i = 1; i <= returnCount && pluginAttributes == nil; i++) {
			err = dsGetRecordEntry([self dsNodeReference], [recordBuf dsDataBuffer], i, &attrListRef, &pRecEntry);
			if (err == eDSNoErr) {
				memcpy(&len, &pRecEntry->fRecordNameAndType.fBufferData, 2);
				if (!strncmp([inPluginName UTF8String], pRecEntry->fRecordNameAndType.fBufferData + 2, len)) {
					pluginAttributes = [DSoAttributeUtils getAttributesAndValuesInNode: self
																			fromBuffer: recordBuf
																		 listReference: attrListRef
																				 count: pRecEntry->fRecordAttributeCount];
				}
				
				dsDeallocRecordEntry([mDirectory dsDirRef], pRecEntry);
				dsCloseAttributeList(attrListRef);
			}
		}
		
		totalCount += returnCount;
	} while (pluginAttributes == nil && (err == eDSBufferTooSmall || (err == eDSNoErr && localcontext != 0)));
		
	if (localcontext != 0) {
		dsReleaseContinueData([self dsNodeReference], localcontext);
	}
	
    [recordBuf release];
    [recName release];
    [attrType release];
    [recType release];

    if (err)
        [DSoException raiseWithStatus:err];

    return pluginAttributes;
}

- (BOOL)pluginEnabled:(NSString*)inPluginName
{
    NSDictionary *pluginAttribs = [self getAttributesAndValuesForPlugin:inPluginName];
	
    return [[(NSArray*)[pluginAttribs objectForKey:@kDS1AttrFunctionalState] objectAtIndex:0] isEqualToString:@"Active"];
}

- (void)setPlugin:(NSString*)inPluginName enabled:(BOOL)enabled
{
    [self setPlugin:inPluginName enabled:enabled withAuthorization:NULL];
}

- (void)setPlugin:(NSString*)inPluginName enabled:(BOOL)enabled withAuthorization:(void*)inAuthExtForm
{
	tContextData 		localcontext = 0;
    tRecordEntryPtr		pRecEntry = NULL;
	tAttributeEntryPtr  pAttrEntry = NULL;
	tAttributeValueEntryPtr  pAttrValueEntry = NULL;
    tAttributeListRef	attrListRef = 0;
    tAttributeValueListRef	attrValueListRef = 0;
	char*				nameStr = NULL;

    DSoDataList			*recName = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSRecordsAll];
    DSoDataList			*recType = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSStdRecordTypePlugins];
    DSoDataList			*attrType = [(DSoDataList*)[DSoDataList alloc] initWithDir:mDirectory cString:kDSAttributesAll];
    DSoBuffer			*recordBuf = [[DSoBuffer alloc] initWithDir:mDirectory bufferSize:4096];

    tDirStatus 			err = eDSNoErr;
    UInt32				i, j, returnCount = 0;
	BOOL				wasEnabled = NO;
	int					pluginIndex = 0;

	do {
		returnCount = 0;
		err = dsGetRecordList([self dsNodeReference], [recordBuf dsDataBuffer], 
			[recName dsDataList], eDSExact, [recType dsDataList], [attrType dsDataList], 
			FALSE, &returnCount, &localcontext);
		if (err == eDSBufferTooSmall)
		{
			[recordBuf grow:2 * [recordBuf getBufferSize]];
			continue;
		}
		for (i = 1; i <= returnCount; i++)
		{
			err = dsGetRecordEntry([self dsNodeReference],[recordBuf dsDataBuffer],i,&attrListRef, &pRecEntry );
			if (err == eDSNoErr)
				err = dsGetRecordNameFromEntry( pRecEntry, &nameStr );
			if (nameStr != NULL && strcmp([inPluginName UTF8String], nameStr) == 0)
			{
				// Get all attributes
				for ( j = 1; j <= pRecEntry->fRecordAttributeCount; j++ )
				{
					err = dsGetAttributeEntry( [self dsNodeReference], [recordBuf dsDataBuffer], attrListRef, j, 
						&attrValueListRef, &pAttrEntry );
					if (err != eDSNoErr || pAttrEntry == NULL) continue;
					
					if (!strcmp(pAttrEntry->fAttributeSignature.fBufferData,kDS1AttrFunctionalState))
					{
						// Get only one attribute value even if there are more
						err = dsGetAttributeValue( [self dsNodeReference], 
							[recordBuf dsDataBuffer], 1, attrValueListRef, 
							&pAttrValueEntry );
						if ( err == eDSNoErr && pAttrValueEntry != NULL
							&& pAttrValueEntry->fAttributeValueData.fBufferData != NULL)
						{
							wasEnabled = strcmp(pAttrValueEntry->fAttributeValueData.fBufferData,
												"Active") == 0;
						}
					}
					else if (!strcmp(pAttrEntry->fAttributeSignature.fBufferData,kDS1AttrPluginIndex))
					{
						err = dsGetAttributeValue( [self dsNodeReference], 
							[recordBuf dsDataBuffer], 1, attrValueListRef, 
							&pAttrValueEntry );
						if ( err == eDSNoErr )
						{
							pluginIndex = pAttrValueEntry->fAttributeValueData.fBufferLength;
						}
					}
									
					dsCloseAttributeValueList( attrValueListRef );
					attrValueListRef = 0;
					if ( pAttrValueEntry != NULL ) {
						dsDeallocAttributeValueEntry( [mDirectory dsDirRef], pAttrValueEntry );
						pAttrValueEntry = NULL;
					}
					if ( pAttrEntry != NULL ) {
						dsDeallocAttributeEntry( [mDirectory dsDirRef], pAttrEntry );
						pAttrEntry = NULL;
					}
				} // loop over j -- all attributes
				
				i = returnCount; // Abort the search loop by forcing the iterator to the max.
				if (localcontext != 0)
				{
					dsReleaseContinueData([self dsNodeReference], localcontext);
					localcontext = 0;
				}
			}
			if (nameStr != NULL)
			{
				free(nameStr);
				nameStr = NULL;
			}
			dsDeallocRecordEntry([mDirectory dsDirRef], pRecEntry);
			dsCloseAttributeList(attrListRef);
		}
	} while (err == eDSBufferTooSmall || (err == eDSNoErr && localcontext != 0));
	
	if (localcontext != 0) {
		dsReleaseContinueData([self dsNodeReference], localcontext);
	}
	
	if (err == eDSNoErr && wasEnabled != enabled) {
		// need to toggle the state
		AuthorizationExternalForm authExtForm;
		if (inAuthExtForm == NULL) {
			bzero(&authExtForm,sizeof(authExtForm));
			inAuthExtForm = &authExtForm;
		}

		err = [self customCall:1000+pluginIndex withAuthorization:inAuthExtForm];
	}
	
    [recordBuf release];
    [recName release];
    [attrType release];
    [recType release];

    if (err)
        [DSoException raiseWithStatus:err];
}

- (NSArray*) findRecordTypes
{
	NSMutableArray     *setOfTypes  = (NSMutableArray*)[super findRecordTypes];
    
    if ([setOfTypes count] > 0) 
        return setOfTypes;
    
    setOfTypes = [mTypeList mutableCopy];

    // Alphabetize the list.
    [setOfTypes sortUsingSelector:@selector(caseInsensitiveCompare:)];
    return [setOfTypes autorelease];
}

@end
