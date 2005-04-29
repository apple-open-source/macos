/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
/*
 *  HighLevelDirServicesMini.c
 *  
 *
 *  Created by Forest Hill on Mon Nov 05 2001.
 *  Copyright (c) 2001 Apple COmputer, Inc. All rights reserved.
 *
 */
#include "HighLevelDirServicesMini.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach-o/dyld.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>
#include <CoreFoundation/CFString.h>

#pragma mark ••• Prototypes for Private Functions •••
tDirStatus		_HLDSBuildDataListFromCFArray( CFArrayRef inArray, const tDirReference inDirRef, tDataListPtr* outAllocatedDataList );
tDirStatus		_HLDSGetAttributeValuesFromRecordsByName( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
					CFArrayRef inRecordNames, CFArrayRef inAttributesToGet, CFArrayRef* outAttributeValues, dsBool limitNumRecordsToNumNames );
tDirStatus		_HLDSLegacySetAttributeValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
					char createIfNecessary, const char** inAttributeValues, unsigned long inNumValues );
tDirStatus		_HLDSLegacySetAttributeCFValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
					char createIfNecessary, CFArrayRef inAttributeValues );

#pragma mark -

tDirStatus HLDSGetAttributeValuesFromRecord( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
	const char* inRecordName, CFArrayRef inAttributesToGet, CFDictionaryRef* outAttributeValues )
{
	tDirStatus			dirStatus = eDSNoErr;
	CFArrayRef			attributesValues = NULL;
	CFMutableArrayRef	recordNames = CFArrayCreateMutable( NULL, 1, &kCFTypeArrayCallBacks );
	CFStringRef			recordNameCFString = CFStringCreateWithCString( NULL, inRecordName, kCFStringEncodingUTF8 );

	CFArraySetValueAtIndex( recordNames, 0, recordNameCFString );
	CFRelease( recordNameCFString );
	
	if( dirStatus == eDSNoErr )
		dirStatus = _HLDSGetAttributeValuesFromRecordsByName( inDirRef, inDirNodeRef, inDSRecordType, recordNames, inAttributesToGet, &attributesValues, true );
	
	CFRelease( recordNames );
	
	if( attributesValues != NULL )
	{
		if( CFArrayGetCount( attributesValues ) < 1 )
			*outAttributeValues = NULL;
		else
		{
			*outAttributeValues = CFArrayGetValueAtIndex( attributesValues, 0 );
			CFRetain( *outAttributeValues );
			CFRelease( attributesValues );
		}
	}
	
	return dirStatus;
}

tDirStatus HLDSGetAttributeValuesFromRecordsByName( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
	CFArrayRef inRecordNames, CFArrayRef inAttributesToGet, CFArrayRef* outAttributeValues )
{
	return _HLDSGetAttributeValuesFromRecordsByName( inDirRef, inDirNodeRef, inDSRecordType, inRecordNames, inAttributesToGet, outAttributeValues, false );
}

tDirStatus HLDSGetAttributeValuesFromRecordsByAttributeValue( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
	const char* inAttributeToSearchOn, CFStringRef inAttributeValueToSearchFor, tDirPatternMatch inPatternMatch, CFArrayRef inAttributesToGet, CFArrayRef* outAttributeValues )
{
	tDirStatus					dirStatus = eDSNoErr;
	dsBool						loopAgain, attributeIsText;
	tDataBufferPtr				dataBuff = NULL;
	unsigned long				i;
	unsigned long				j;
	unsigned long				k;
	tDataListPtr				recTypes = NULL;
	tDataListPtr				attributesToGetList = NULL;
	unsigned long				recCount = 0;
	unsigned long				buffSize;
	tContextData				continueData = NULL;
	CFMutableArrayRef			mutableValues = NULL;
	CFMutableArrayRef			mutableAttributesAndValues = NULL;
	CFMutableDictionaryRef		mutableDict = NULL;
	CFStringRef					cfString = NULL;
	tDataNodePtr				searchByValueAttrType = NULL;
	tDataNodePtr				searchByValueAttrValue = NULL;

	tRecordEntry*				recEntry = NULL;
	tAttributeListRef			attrListRef = 0;
	tAttributeEntry*			attrEntry = NULL;
	tAttributeValueListRef		valueRef = 0;
	tAttributeValueEntry*		attrValueEntry = NULL;
	char*						stringBuffer = NULL;
	char*						stringBufferToUse = NULL;
	const char*					constStringBufferToUse = NULL;
	CFIndex						stringBufferSize = 0;
	
	stringBufferSize = 256;
	stringBuffer = malloc( stringBufferSize );
	if( stringBuffer == NULL )
		return eMemoryAllocError;

	mutableAttributesAndValues = CFArrayCreateMutable( NULL, recCount, &kCFTypeArrayCallBacks );
	if( dirStatus == eDSNoErr )
	{
		dataBuff = dsDataBufferAllocate( inDirRef, 1 * 1024 ); // allocate a 1k buffer
		if( dataBuff == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
	{
		recTypes = dsBuildListFromStrings( inDirRef, inDSRecordType, NULL );
		if( recTypes == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
	{
		//build the value to search for
		constStringBufferToUse = CFStringGetCStringPtr( inAttributeValueToSearchFor, kCFStringEncodingUTF8 );
		if( constStringBufferToUse == NULL )
		{
			if( ( CFStringGetLength( inAttributeValueToSearchFor ) * 2 ) > ( stringBufferSize - 1 ) )
			{
				free ( stringBuffer );
				stringBufferSize = ( CFStringGetLength( inAttributeValueToSearchFor ) * 2 ) + 1;
				stringBuffer = malloc( stringBufferSize );
				if( stringBuffer == NULL )
					return eMemoryAllocError;
			}
			CFStringGetCString( inAttributeValueToSearchFor, stringBuffer, stringBufferSize, kCFStringEncodingUTF8 );
			constStringBufferToUse = stringBuffer;
		}
		searchByValueAttrValue = dsDataNodeAllocateString( inDirRef, constStringBufferToUse );
		if( searchByValueAttrValue == NULL )
			dirStatus = eMemoryAllocError;
	}
	
	if( dirStatus == eDSNoErr )
	{
		//build the attribute name to search for the above value in
		searchByValueAttrType = dsDataNodeAllocateString( inDirRef, inAttributeToSearchOn );
		if( searchByValueAttrType == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
		dirStatus = _HLDSBuildDataListFromCFArray( inAttributesToGet, inDirRef, &attributesToGetList );
	
	for( loopAgain=true; loopAgain && ( dirStatus == eDSNoErr ); )
	{
		loopAgain=false;
		dirStatus = dsDoAttributeValueSearchWithData( inDirNodeRef, dataBuff, recTypes, searchByValueAttrType,
			inPatternMatch, searchByValueAttrValue, attributesToGetList, false, &recCount, &continueData );
		if ( dirStatus == eDSBufferTooSmall )
		{
			buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDirRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDirRef, buffSize * 2 );
			if( dataBuff == NULL )
				dirStatus = eMemoryAllocError;
			else
			{
				loopAgain = true;
				dirStatus = eDSNoErr;
			}
			if( continueData != NULL )
			{
				dsReleaseContinueData( inDirNodeRef, continueData );
				continueData = NULL;
			}
		}
		
		if( continueData != NULL )
			loopAgain = true;
			
		for( i=1; ( dirStatus == eDSNoErr ) && ( i<=recCount ); i++ )
		{
			if( dirStatus == eDSNoErr )
				dirStatus = dsGetRecordEntry( inDirNodeRef, dataBuff, i, &attrListRef, &recEntry );
			
			//create a dictionary to hold the attribute names and their respective array of values
			if( dirStatus == eDSNoErr )
				mutableDict = CFDictionaryCreateMutable( NULL, recEntry->fRecordAttributeCount, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			if( mutableDict == NULL )
				dirStatus = eMemoryAllocError;
			for( j=1, attributeIsText=true; attributeIsText && ( dirStatus == eDSNoErr ) && ( j<=recEntry->fRecordAttributeCount ); j++ )
			{
				if( dirStatus == eDSNoErr )
					dirStatus = dsGetAttributeEntry( inDirNodeRef, dataBuff, attrListRef, j, &valueRef, &attrEntry );

				//create an array to hold all of the attribute values
				if( dirStatus == eDSNoErr )
					mutableValues = CFArrayCreateMutable( NULL, attrEntry->fAttributeValueCount, &kCFTypeArrayCallBacks );
				if( mutableValues == NULL )
					dirStatus = eMemoryAllocError;
				for( k=1; attributeIsText && ( dirStatus == eDSNoErr ) && ( k<=attrEntry->fAttributeValueCount ); k++ )
				{
					dirStatus = dsGetAttributeValue( inDirNodeRef, dataBuff, k, valueRef, &attrValueEntry );
					if( dirStatus == eDSNoErr )
					{
						if( attrValueEntry->fAttributeValueData.fBufferSize >= ( attrValueEntry->fAttributeValueData.fBufferLength + 1 ) )
						{
							attrValueEntry->fAttributeValueData.fBufferData[attrValueEntry->fAttributeValueData.fBufferLength] = '\0';
							stringBufferToUse = attrValueEntry->fAttributeValueData.fBufferData;
						}
						else
						{
							if( ( attrValueEntry->fAttributeValueData.fBufferLength + 1 ) > stringBufferSize )
							{
								free( stringBuffer );
								stringBufferSize = attrValueEntry->fAttributeValueData.fBufferLength + 1;
								stringBuffer = malloc( stringBufferSize );
								if( stringBuffer == NULL )
									dirStatus = eMemoryAllocError;
							}
							memmove( stringBuffer, attrValueEntry->fAttributeValueData.fBufferData, attrValueEntry->fAttributeValueData.fBufferLength );
							stringBuffer[attrValueEntry->fAttributeValueData.fBufferLength] = '\0';
							stringBufferToUse = stringBuffer;
						}
						if( dirStatus == eDSNoErr )
						{
							cfString = CFStringCreateWithCString( NULL, stringBufferToUse, kCFStringEncodingUTF8 );
							if( cfString == NULL )
								cfString = CFStringCreateWithCString( NULL, "<Non-Text Value>", kCFStringEncodingUTF8 );
							if( cfString == NULL )
								dirStatus = eMemoryAllocError;
						}
					}
					if( ( dirStatus == eDSNoErr ) && attributeIsText )
						CFArrayAppendValue( mutableValues, cfString );
					if( cfString != NULL )	//release so that it's only retained by the array
					{
						CFRelease( cfString );
						cfString = NULL;
					}

					if( attrValueEntry != NULL )
					{
						dsDeallocAttributeValueEntry( inDirRef, attrValueEntry );
						attrValueEntry = NULL;
					}
				}
				if( dirStatus == eDSNoErr )
				{
					cfString = NULL;
					cfString = CFStringCreateWithCString( NULL, attrEntry->fAttributeSignature.fBufferData, kCFStringEncodingUTF8 );
					if( cfString == NULL )
						dirStatus = eMemoryAllocError;
				}
				if( dirStatus == eDSNoErr )
					CFDictionarySetValue( mutableDict, cfString, mutableValues );
				if( cfString != NULL )	//release so that it's only retained by the dictionary
				{
					CFRelease( cfString );
					cfString = NULL;
				}
				if( mutableValues != NULL )	//release so that it's only retained by the dictionary
				{
					CFRelease( mutableValues );
					mutableValues = NULL;
				}

				if( attrEntry != NULL )
				{
					dsDeallocAttributeEntry	( inDirRef, attrEntry );
					attrEntry = NULL;
				}
				if( valueRef != 0 )
				{
					dsCloseAttributeValueList( valueRef );
					valueRef = 0;
				}
			}
			if( ( dirStatus == eDSNoErr ) && attributeIsText )
				CFArrayAppendValue( mutableAttributesAndValues, mutableDict );

			if( mutableDict != NULL )	//release so that it's only retained by the array
			{
				CFRelease( mutableDict );
				mutableDict = NULL;
			}
			if( recEntry != NULL )
			{
				dsDeallocRecordEntry( inDirRef, recEntry );
				recEntry = NULL;
			}
			if( attrListRef != 0 )
			{
				dsCloseAttributeList( attrListRef );
				attrListRef = 0;
			}
		}
		if( ( dirStatus != eDSNoErr ) && ( continueData != NULL ) )
		{
			dsReleaseContinueData( inDirNodeRef, continueData );
			continueData = NULL;
		}
	}

	if( searchByValueAttrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, searchByValueAttrType );
		searchByValueAttrType = NULL;
	}

	if( searchByValueAttrValue != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, searchByValueAttrValue );
		searchByValueAttrValue = NULL;
	}

	//cleanup
	if( stringBuffer != NULL )
		free( stringBuffer );

	if( recTypes != NULL )
	{
		dsDataListDeallocate( inDirRef, recTypes );
		free( recTypes );
		recTypes = NULL;
	}
	if( attributesToGetList != NULL )
	{
		dsDataListDeallocate( inDirRef, attributesToGetList );
		free( attributesToGetList );
		attributesToGetList = NULL;
	}
	if( attrValueEntry != NULL )
	{
		dsDeallocAttributeValueEntry	( inDirRef, attrValueEntry );
		attrValueEntry = NULL;
	}
	if( attrEntry != NULL )
	{
		dsDeallocAttributeEntry	( inDirRef, attrEntry );
		attrEntry = NULL;
	}
	if( valueRef != 0 )
	{
		dsCloseAttributeValueList( valueRef );
		valueRef = 0;
	}
	if( recEntry != NULL )
	{
		dsDeallocRecordEntry( inDirRef, recEntry );
		recEntry = NULL;
	}
	if( attrListRef != 0 )
	{
		dsCloseAttributeList( attrListRef );
		attrListRef = 0;
	}
	if( continueData != NULL )
	{
		dsReleaseContinueData( inDirNodeRef, continueData );
		continueData = NULL;
	}
	if( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( inDirRef, dataBuff );
		dataBuff = NULL;
	}

	if( dirStatus == eDSNoErr )
		*outAttributeValues = CFArrayCreateCopy( NULL, mutableAttributesAndValues );
	
	if( mutableAttributesAndValues != NULL )
	{
		CFRelease( mutableAttributesAndValues );
		mutableAttributesAndValues = NULL;
	}

	return dirStatus;
}

tDirStatus HLDSGetAttributeValuesFromRecordsByAttributeValues( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
	const char* inAttributeToSearchOn, CFArrayRef inAttributeValuesToSearchFor, tDirPatternMatch inPatternMatch, CFArrayRef inAttributesToGet,
	CFArrayRef* outRecsAttributesValues )
{
	tDirStatus					dirStatus = eDSNoErr;
	dsBool						loopAgain, attributeIsText;
	tDataBufferPtr				dataBuff = NULL;
	unsigned long				i;
	unsigned long				j;
	unsigned long				k;
	tDataListPtr				recTypes = NULL;
	tDataListPtr				valuesToSearchFor = NULL;
	tDataListPtr				attributeList = NULL;
	unsigned long				recCount = 0;
	unsigned long				buffSize;
	tContextData				continueData = NULL;
	CFMutableArrayRef			mutableValues = NULL;
	CFMutableArrayRef			mutableAttributesAndValues = NULL;
	CFMutableDictionaryRef		mutableDict = NULL;
	CFStringRef					cfString = NULL;
	tDataNodePtr				searchByValueAttrType = NULL;

	tRecordEntry*				recEntry = NULL;
	tAttributeListRef			attrListRef = 0;
	tAttributeEntry*			attrEntry = NULL;
	tAttributeValueListRef		valueRef = 0;
	tAttributeValueEntry*		attrValueEntry = NULL;
	char*						stringBuffer = NULL;
	char*						stringBufferToUse = NULL;
	CFIndex						stringBufferSize = 0;
	
	if( ( inAttributeValuesToSearchFor == NULL ) || ( CFArrayGetCount( inAttributeValuesToSearchFor ) == 0 ) )
	{
		*outRecsAttributesValues = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
		return eDSNoErr;
	}
	
	stringBufferSize = 256;
	stringBuffer = malloc( stringBufferSize );
	if( stringBuffer == NULL )
		return eMemoryAllocError;

	mutableAttributesAndValues = CFArrayCreateMutable( NULL, recCount, &kCFTypeArrayCallBacks );
	if( dirStatus == eDSNoErr )
	{
		dataBuff = dsDataBufferAllocate( inDirRef, 1 * 1024 ); // allocate a 1k buffer
		if( dataBuff == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
	{
		recTypes = dsBuildListFromStrings( inDirRef, inDSRecordType, NULL );
		if( recTypes == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
		dirStatus = _HLDSBuildDataListFromCFArray( inAttributeValuesToSearchFor, inDirRef, &valuesToSearchFor );

	if( dirStatus == eDSNoErr )
	{
		//build the attribute name to search for the above values in
		searchByValueAttrType = dsDataNodeAllocateString( inDirRef, inAttributeToSearchOn );
		if( searchByValueAttrType == NULL )
			dirStatus = eMemoryAllocError;
	}
	
	if( dirStatus == eDSNoErr )
		dirStatus = _HLDSBuildDataListFromCFArray( inAttributesToGet, inDirRef, &attributeList );
	
	for( loopAgain=true; loopAgain && ( dirStatus == eDSNoErr ); )
	{
		loopAgain=false;
		dirStatus = dsDoMultipleAttributeValueSearchWithData( inDirNodeRef, dataBuff, recTypes, searchByValueAttrType,
			inPatternMatch, valuesToSearchFor, attributeList, false,  &recCount, &continueData );
		if ( dirStatus == eDSBufferTooSmall )
		{
			buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDirRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDirRef, buffSize * 2 );
			if( dataBuff == NULL )
				dirStatus = eMemoryAllocError;
			else
			{
				loopAgain = true;
				dirStatus = eDSNoErr;
				continue;
			}
		}
		
		if( continueData != NULL )
			loopAgain = true;
			
		for( i=1; ( dirStatus == eDSNoErr ) && ( i<=recCount ); i++ )
		{
			if( dirStatus == eDSNoErr )
				dirStatus = dsGetRecordEntry( inDirNodeRef, dataBuff, i, &attrListRef, &recEntry );
			
			//create a dictionary to hold the attribute names and their respective array of values
			if( dirStatus == eDSNoErr )
				mutableDict = CFDictionaryCreateMutable( NULL, recEntry->fRecordAttributeCount, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			if( mutableDict == NULL )
				dirStatus = eMemoryAllocError;
			for( j=1, attributeIsText=true; attributeIsText && ( dirStatus == eDSNoErr ) && ( j<=recEntry->fRecordAttributeCount ); j++ )
			{
				if( dirStatus == eDSNoErr )
					dirStatus = dsGetAttributeEntry( inDirNodeRef, dataBuff, attrListRef, j, &valueRef, &attrEntry );

				//create an array to hold all of the attribute values
				if( dirStatus == eDSNoErr )
					mutableValues = CFArrayCreateMutable( NULL, attrEntry->fAttributeValueCount, &kCFTypeArrayCallBacks );
				if( mutableValues == NULL )
					dirStatus = eMemoryAllocError;
				for( k=1; attributeIsText && ( dirStatus == eDSNoErr ) && ( k<=attrEntry->fAttributeValueCount ); k++ )
				{
					dirStatus = dsGetAttributeValue( inDirNodeRef, dataBuff, k, valueRef, &attrValueEntry );
					if( dirStatus == eDSNoErr )
					{
						if( attrValueEntry->fAttributeValueData.fBufferSize >= ( attrValueEntry->fAttributeValueData.fBufferLength + 1 ) )
						{
							attrValueEntry->fAttributeValueData.fBufferData[attrValueEntry->fAttributeValueData.fBufferLength] = '\0';
							stringBufferToUse = attrValueEntry->fAttributeValueData.fBufferData;
						}
						else
						{
							if( ( attrValueEntry->fAttributeValueData.fBufferLength + 1 ) > stringBufferSize )
							{
								free( stringBuffer );
								stringBufferSize = attrValueEntry->fAttributeValueData.fBufferLength + 1;
								stringBuffer = malloc( stringBufferSize );
								if( stringBuffer == NULL )
									dirStatus = eMemoryAllocError;
							}
							memmove( stringBuffer, attrValueEntry->fAttributeValueData.fBufferData, attrValueEntry->fAttributeValueData.fBufferLength );
							stringBuffer[attrValueEntry->fAttributeValueData.fBufferLength] = '\0';
							stringBufferToUse = stringBuffer;
						}
						if( dirStatus == eDSNoErr )
						{
							cfString = CFStringCreateWithCString( NULL, stringBufferToUse, kCFStringEncodingUTF8 );
							if( cfString == NULL )
								cfString = CFStringCreateWithCString( NULL, "<Non-Text Value>", kCFStringEncodingUTF8 );
							if( cfString == NULL )
								dirStatus = eMemoryAllocError;
						}
					}
					if( ( dirStatus == eDSNoErr ) && attributeIsText )
						CFArrayAppendValue( mutableValues, cfString );
					if( cfString != NULL )	//release so that it's only retained by the array
					{
						CFRelease( cfString );
						cfString = NULL;
					}

					if( attrValueEntry != NULL )
					{
						dsDeallocAttributeValueEntry( inDirRef, attrValueEntry );
						attrValueEntry = NULL;
					}
				}
				if( dirStatus == eDSNoErr )
				{
					cfString = NULL;
					cfString = CFStringCreateWithCString( NULL, attrEntry->fAttributeSignature.fBufferData, kCFStringEncodingUTF8 );
					if( cfString == NULL )
						dirStatus = eMemoryAllocError;
				}
				if( dirStatus == eDSNoErr )
					CFDictionarySetValue( mutableDict, cfString, mutableValues );
				if( cfString != NULL )	//release so that it's only retained by the dictionary
				{
					CFRelease( cfString );
					cfString = NULL;
				}
				if( mutableValues != NULL )	//release so that it's only retained by the dictionary
				{
					CFRelease( mutableValues );
					mutableValues = NULL;
				}

				if( attrEntry != NULL )
				{
					dsDeallocAttributeEntry	( inDirRef, attrEntry );
					attrEntry = NULL;
				}
				if( valueRef != 0 )
				{
					dsCloseAttributeValueList( valueRef );
					valueRef = 0;
				}
			}
			if( ( dirStatus == eDSNoErr ) && attributeIsText )
				CFArrayAppendValue( mutableAttributesAndValues, mutableDict );

			if( mutableDict != NULL )	//release so that it's only retained by the array
			{
				CFRelease( mutableDict );
				mutableDict = NULL;
			}
			if( recEntry != NULL )
			{
				dsDeallocRecordEntry( inDirRef, recEntry );
				recEntry = NULL;
			}
			if( attrListRef != 0 )
			{
				dsCloseAttributeList( attrListRef );
				attrListRef = 0;
			}
		}
		if( loopAgain && ( dirStatus == eDSNoErr ) && ( dataBuff->fBufferSize < ( 128 * 1024 ) ) )
		{
			buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDirRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDirRef, ( buffSize * 3 ) / 2 );
			if( dataBuff == NULL )
				dirStatus = eMemoryAllocError;
		}
	}

	//cleanup
	if( stringBuffer != NULL )
		free( stringBuffer );

	if( recTypes != NULL )
	{
		dsDataListDeallocate( inDirRef, recTypes );
		free( recTypes );
		recTypes = NULL;
	}
	if( valuesToSearchFor != NULL )
	{
		dsDataListDeallocate( inDirRef, valuesToSearchFor );
		free( valuesToSearchFor );
		valuesToSearchFor = NULL;
	}

	if( searchByValueAttrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, searchByValueAttrType );
		free( searchByValueAttrType );
		searchByValueAttrType = NULL;
	}

	if( attributeList != NULL )
	{
		dsDataListDeallocate( inDirRef, attributeList );
		free( attributeList );
		attributeList = NULL;
	}
	if( attrValueEntry != NULL )
	{
		dsDeallocAttributeValueEntry( inDirRef, attrValueEntry );
		attrValueEntry = NULL;
	}
	if( attrEntry != NULL )
	{
		dsDeallocAttributeEntry	( inDirRef, attrEntry );
		attrEntry = NULL;
	}
	if( valueRef != 0 )
	{
		dsCloseAttributeValueList( valueRef );
		valueRef = 0;
	}
	if( recEntry != NULL )
	{
		dsDeallocRecordEntry( inDirRef, recEntry );
		recEntry = NULL;
	}
	if( attrListRef != 0 )
	{
		dsCloseAttributeList( attrListRef );
		attrListRef = 0;
	}
	if( continueData != NULL )
	{
		dsReleaseContinueData( inDirRef, continueData );
		continueData = NULL;
	}
	if( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( inDirRef, dataBuff );
		dataBuff = NULL;
	}

	if( dirStatus == eDSNoErr )
		*outRecsAttributesValues = CFArrayCreateCopy( NULL, mutableAttributesAndValues );
	
	if( mutableAttributesAndValues != NULL )
	{
		CFRelease( mutableAttributesAndValues );
		mutableAttributesAndValues = NULL;
	}

	return dirStatus;
}

#pragma mark -

tDirStatus HLDSSetAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
	char createIfNecessary, const char* inAttributeValue )
{
	if( inAttributeValue == NULL )
		inAttributeValue = "";
	tDirStatus dirStatus = HLDSSetAttributeValues( inDirRef, inRecordRef, inAttributeName, createIfNecessary, &inAttributeValue, 1 );
	if( ( dirStatus == eDSSchemaError ) && ( strlen( inAttributeValue ) == 0 ) )
	{
		inAttributeValue = kHLDSEmptyAttributeValueForRequiredAttributes;
		dirStatus = HLDSSetAttributeValues( inDirRef, inRecordRef, inAttributeName, createIfNecessary, &inAttributeValue, 1 );
	}
	return dirStatus;
}

tDirStatus HLDSSetAttributeValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
	char createIfNecessary, const char** inAttributeValues, unsigned long inNumValues )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataList attrValuesDataList={};
	tDataNodePtr attrType = NULL;
	unsigned long i;
	dsBool nativeSetAttributeValuesAvailable = true;

	// work around <rdar://problem/3825319>
	if( ( inNumValues == 0 ) || ( ( inNumValues == 1 ) && ( inAttributeValues[0][0] == '\0' ) ) )
		return HLDSRemoveAttribute( inDirRef, inRecordRef, inAttributeName );

	for( i=0; ( i<inNumValues ) && ( dirStatus == eDSNoErr ); i++ )
	{
		if( i == 0  )
			dirStatus = dsBuildListFromStringsAlloc ( inDirRef, &attrValuesDataList, inAttributeValues[i], NULL );
		else
			dirStatus = dsAppendStringToListAlloc( inDirRef, &attrValuesDataList, inAttributeValues[i] );
	}

	attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );
	
	if( dirStatus == eDSNoErr )
	{
		dirStatus = dsSetAttributeValues( inRecordRef, attrType, &attrValuesDataList );
		switch( dirStatus )
		{
			case eDSNoErr:
			default:
				break;
			
			case eNotHandledByThisNode:
			case eUnknownAPICall:
			case eUndefinedError:
				nativeSetAttributeValuesAvailable = false;
				break;
		}
	}
										
	if( attrValuesDataList.fDataNodeCount != 0 )
		dsDataListDeallocate( inDirRef, &attrValuesDataList );

	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}

	if( !nativeSetAttributeValuesAvailable )
		dirStatus = _HLDSLegacySetAttributeValues( inDirRef, inRecordRef, inAttributeName, createIfNecessary, inAttributeValues, inNumValues );
	
	return dirStatus;
}

tDirStatus HLDSSetAttributeCFValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
	char createIfNecessary, CFArrayRef inAttributeValues )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataList attrValuesDataList={};
	tDataNodePtr attrType = NULL;
	CFIndex i;
	CFIndex numValues = CFArrayGetCount( inAttributeValues );
	CFStringRef attributeValue = NULL;
	const char* attributeValueCString = NULL;
	char* allocatedCString = NULL;
	CFIndex bufferSize = 0;
	dsBool nativeSetAttributeValuesAvailable = true;

	if( numValues == 0 )
		return HLDSRemoveAttribute( inDirRef, inRecordRef, inAttributeName );

	for( i=0; ( i<numValues ) && ( dirStatus == eDSNoErr ); i++ )
	{
		attributeValue = CFArrayGetValueAtIndex( inAttributeValues, i );
		attributeValueCString = CFStringGetCStringPtr( attributeValue, kCFStringEncodingUTF8 );
		if( attributeValueCString == NULL )
		{
			bufferSize = 4 * CFStringGetLength( attributeValue );	//allow four eight-byte  chars for every  char, jsut to be save
			allocatedCString = malloc( bufferSize );
			if( !CFStringGetCString( attributeValue, allocatedCString, bufferSize, kCFStringEncodingUTF8 ) )
			{
				dirStatus = eMemoryAllocError;
				free( allocatedCString );
				allocatedCString = NULL;
			}
			attributeValueCString = allocatedCString;
		}

		if( dirStatus == eDSNoErr )
		{
			if( i == 0  )
			{
				// work around <rdar://problem/3825319>
				if( ( numValues == 1 ) && ( attributeValueCString[0] == '\0' ) )
				{
					if( allocatedCString != NULL )
						free( allocatedCString );
					return HLDSRemoveAttribute( inDirRef, inRecordRef, inAttributeName );
				}

				dirStatus = dsBuildListFromStringsAlloc ( inDirRef, &attrValuesDataList, attributeValueCString, NULL );
			}
			else
				dirStatus = dsAppendStringToListAlloc( inDirRef, &attrValuesDataList, attributeValueCString );
		}
	}

	if( dirStatus == eDSNoErr )
		attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );
	
	if( dirStatus == eDSNoErr )
	{
		dirStatus = dsSetAttributeValues( inRecordRef, attrType, &attrValuesDataList );
		switch( dirStatus )
		{
			case eDSNoErr:
			default:
				break;
			
			case eNotHandledByThisNode:
			case eUnknownAPICall:
			case eUndefinedError:
				nativeSetAttributeValuesAvailable = false;
				break;
		}
	}
										
	if( allocatedCString != NULL )
	{
		free( allocatedCString );
		allocatedCString = NULL;
	}

	if( attrValuesDataList.fDataNodeCount != 0 )
		dsDataListDeallocate( inDirRef, &attrValuesDataList );

	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}

	if( !nativeSetAttributeValuesAvailable )
		dirStatus = _HLDSLegacySetAttributeCFValues( inDirRef, inRecordRef, inAttributeName, createIfNecessary, inAttributeValues );
	
	return dirStatus;
}

tDirStatus HLDSSetBinaryAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef,
	const char* inAttributeName, dsBool createIfNecessary, CFDataRef inAttributeValue )
{
	CFArrayRef theArray = NULL;
	tDirStatus dirStatus = eDSNoErr;
	
	theArray = CFArrayCreate( NULL, (const void**)&inAttributeValue, 1, NULL );
	if( theArray == NULL )
		return eMemoryAllocError;
		
	dirStatus = HLDSSetBinaryAttributeValues( inDirRef, inRecordRef, inAttributeName, createIfNecessary, theArray  );
	
	CFRelease( theArray );
	
	return dirStatus;
}

tDirStatus HLDSSetBinaryAttributeValues( const tDirReference inDirRef, tRecordReference inRecordRef,
	const char* inAttributeName, dsBool createIfNecessary, CFArrayRef inAttributeValues )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataNodePtr attrType = NULL;
	tDataNodePtr attrValue = NULL;
	tAttributeValueEntryPtr newAttrValue = NULL;
	tAttributeValueEntryPtr attrValueEntry = NULL;
	CFIndex i;
	CFIndex j;
	CFIndex dataLength = 0;
	CFIndex numValues;
	const UInt8* dataBtyes = NULL;
	tAccessControlEntry theACLEntry;
	CFDataRef theData = NULL;

	attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );

	numValues = CFArrayGetCount( inAttributeValues );
	for( i=0, j=1; ( i<numValues ) && ( dirStatus == eDSNoErr ); i++, j++ )
	{
		theData = CFArrayGetValueAtIndex( inAttributeValues, i );

		if( theData == NULL )
			dirStatus = eDSNullParameter;
		else
		{
			dataBtyes = CFDataGetBytePtr( theData );
			dataLength = CFDataGetLength( theData );
			if( dataLength == 0 )
				continue;	//zero length is the same thing as deleting
		}
		
		attrValue = NULL;
		newAttrValue = NULL;
		attrValueEntry = NULL;

		if( dirStatus == eDSNoErr )
			dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, j, &attrValueEntry );

		switch( dirStatus )
		{
			case eDSNoErr:
				newAttrValue = dsAllocAttributeValueEntry( inDirRef, attrValueEntry->fAttributeValueID,
					(void*)dataBtyes, dataLength );
				dirStatus = dsSetAttributeValue( inRecordRef, attrType, newAttrValue );
				break;
			case eDSAttributeNotFound:
				attrValue = dsDataNodeAllocateBlock( inDirRef, dataLength, dataLength, (void*)dataBtyes );
				memset( &theACLEntry, 0, sizeof( theACLEntry ) );
				dirStatus = dsAddAttribute( inRecordRef, attrType, &theACLEntry, attrValue );
				break;
			case eDSInvalidIndex:
			case eDSIndexOutOfRange:
			case eDSIndexNotFound:
				attrValue = dsDataNodeAllocateBlock( inDirRef, dataLength, dataLength, (void*)dataBtyes );
				dirStatus = dsAddAttributeValue( inRecordRef, attrType, attrValue);
				break;
			default:
				break;
		}
		if( attrValue != NULL )
		{
			dsDataNodeDeAllocate( inDirRef, attrValue );
			attrValue = NULL;
		}
		if( newAttrValue != NULL )
		{
			dsDeallocAttributeValueEntry ( inDirRef, newAttrValue );
			newAttrValue = NULL;
		}
		if( attrValueEntry != NULL )
		{
			dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
			attrValueEntry = NULL;
		}
	}

	if( dirStatus == eDSNoErr )
	{
		for( ; dirStatus == eDSNoErr; j++ )
		{
			attrValueEntry = NULL;
			dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, j, &attrValueEntry );
			if( dirStatus == eDSNoErr )
				dirStatus = dsRemoveAttributeValue( inRecordRef, attrType, attrValueEntry->fAttributeValueID );
			
			if( attrValueEntry != NULL )
			{
				dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
				attrValueEntry = NULL;
			}
		}
		dirStatus = eDSNoErr;
	}

	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}
	return dirStatus;
}

tDirStatus HLDSAddAttribute( const tDirReference inDirRef, tRecordReference inRecordRef,
	const char* inAttributeName, const char* inAttributeValue )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataNodePtr attrName = NULL;
	tDataNodePtr attrValue = NULL;

	attrName = dsDataNodeAllocateString(inDirRef, inAttributeName );
	if ( attrName != NULL )
	{
		attrValue = dsDataNodeAllocateString( inDirRef, inAttributeValue );
		if ( attrValue != NULL )
		{
			dirStatus = dsAddAttribute(inRecordRef, attrName, NULL, attrValue );
			dirStatus = dsDataNodeDeAllocate( inDirRef, attrValue );
			attrValue = NULL;
		}
		dirStatus = dsDataNodeDeAllocate( inDirRef, attrName );
		attrName = NULL;
	}
	
	return dirStatus;
}

tDirStatus HLDSAddAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
	char createIfNecessary, const char* inAttributeValue )
{
	tDirStatus				dirStatus = eDSNoErr;
	tDataNodePtr			attrType = NULL;
	tDataNodePtr			attrValue;
	tAttributeEntryPtr		attrEntry;

	attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );
	attrValue = dsDataNodeAllocateString( inDirRef, inAttributeValue );

	dirStatus = dsGetRecordAttributeInfo( inRecordRef, attrType, &attrEntry );
	switch( dirStatus )
	{
		case eDSNoErr:
		{
			dirStatus = dsAddAttributeValue( inRecordRef, attrType, attrValue );
			break;
		}
			
		case eDSAttributeNotFound:	//create the attribute if it's not there
		{
			tAccessControlEntryPtr theACLEntry = (tAccessControlEntry *)malloc( sizeof( tAccessControlEntry ) );
			dirStatus = dsAddAttribute( inRecordRef, attrType, theACLEntry, attrValue );
			break;
		}
		
		default:
			break;
	}

	if( attrValue != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrValue );
		attrValue = NULL;
	}
	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}
	return dirStatus;
}

tDirStatus HLDSRemoveAttribute( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataNodePtr attrName = NULL;

	attrName = dsDataNodeAllocateString(inDirRef, inAttributeName );
	if ( attrName != NULL )
	{
		dirStatus = dsRemoveAttribute( inRecordRef, attrName);

		dsDataNodeDeAllocate( inDirRef, attrName );
		attrName = NULL;
	}
	
	return dirStatus;
}

tDirStatus HLDSRemoveAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName, const char* inAttributeValue )
{
	tDirStatus					dirStatus = eDSNoErr;
	tDataNodePtr				attrType = NULL;
	tAttributeValueEntryPtr		attrValueEntry;
	tAttributeEntryPtr			attrEntry;
	unsigned long				i;
	char						done = false;

	attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );
	dirStatus = dsGetRecordAttributeInfo( inRecordRef, attrType, &attrEntry );
	for( i=1; !done && (i<=attrEntry->fAttributeValueCount); i++ )
	{
		attrValueEntry = NULL;

		dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, i, &attrValueEntry );
		switch( dirStatus )
		{
			case eDSNoErr:
			{
				if( strcmp( (char*)&(attrValueEntry->fAttributeValueData.fBufferData), inAttributeValue ) == 0 )
				{
					dirStatus = dsRemoveAttributeValue( inRecordRef, attrType, attrValueEntry->fAttributeValueID );
					done = true;
				}
				break;
			}
			default:
				break;
		}
	
		if( attrValueEntry != NULL )
		{
			dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
			attrValueEntry = NULL;
		}
	}
	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}
	return dirStatus;
}

#pragma mark -

CFStringRef HLDSCreateFabricatedGUID( int inID, CFStringRef inRecordType )
{
	dsBool isGroup = false;
	
	if( CFStringCompare( inRecordType, CFSTR( kDSStdRecordTypeGroups ), 0 ) == kCFCompareEqualTo )
		isGroup = true;
	else if( CFStringCompare( inRecordType, CFSTR( kDSStdRecordTypeUsers ), 0 ) != kCFCompareEqualTo )
		return NULL;	// some other  non-supported record type was passed in

	return CFStringCreateWithFormat( NULL, NULL, CFSTR( "%s%8.8X" ),
		isGroup ? kFabricatedGroupGUIDPrefix : kFabricatedUserGUIDPrefix, inID );
}

int HLDSGetIDFromFabricatedGUID( CFStringRef theGUID )
{
	CFStringRef hexEncodedID = NULL;
	int theID = -1;
	char hexIDCStr[9];

	//make sure it really is a fabricated GUID
	if( !CFStringHasPrefix( theGUID, CFSTR( kFabricatedUserGUIDPrefix ) ) &&
			!CFStringHasPrefix( theGUID, CFSTR( kFabricatedGroupGUIDPrefix ) ) )
		return -1;
	
	hexEncodedID = CFStringCreateWithSubstring( NULL, theGUID, CFRangeMake( strlen( kFabricatedUserGUIDPrefix ), 8 ) );
	
	if( !CFStringGetCString( hexEncodedID, hexIDCStr, 9, kCFStringEncodingUTF8 ) )
		return -1;
	
	CFRelease( hexEncodedID );
	
	sscanf( hexIDCStr, "%X", &theID );
	
	return theID;
}

#pragma mark -

tDirStatus HLDSIsLegacyGroup( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, char* inGroupName, dsBool* outIsLegacy, CFArrayRef* outShortNameMembers )
{
	tDirStatus dirStatus = eDSNoErr;
	CFStringRef desiredAttrNames[] = { CFSTR( kDSNAttrGroupMembers ), CFSTR( kDSNAttrGroupMembership ), CFSTR( kDSNAttrRecordName ) };
	CFDictionaryRef groupAttrsVals= NULL;
	CFArrayRef shortNameMembers = NULL;
	CFArrayRef guidMembers = NULL;
	CFArrayRef recordNames = NULL;
	CFIndex shortNameMembersCount = 0;
	CFIndex guidMembersCount = 0;
	CFArrayRef attributesToGet = CFArrayCreate( NULL,  (const void**)desiredAttrNames, 3, &kCFTypeArrayCallBacks );
	
	dirStatus = HLDSGetAttributeValuesFromRecord( inDirRef, inDirNodeRef, kDSStdRecordTypeGroups, inGroupName,
		attributesToGet, &groupAttrsVals );
	
	if( dirStatus == eDSNoErr )
	{
		if( groupAttrsVals == NULL )
			dirStatus = eDSRecordNotFound;
		else
		{
			recordNames = CFDictionaryGetValue( groupAttrsVals, CFSTR( kDSNAttrRecordName ) );
			if( recordNames == NULL )
				dirStatus = eDSRecordNotFound;
			else if( CFArrayGetCount( recordNames ) < 1 )
				dirStatus = eDSRecordNotFound;
		}
	}

	if( dirStatus == eDSNoErr )
	{
		shortNameMembers = CFDictionaryGetValue( groupAttrsVals, CFSTR( kDSNAttrGroupMembership ) );
		guidMembers = CFDictionaryGetValue( groupAttrsVals, CFSTR( kDSNAttrGroupMembers ) );
		
		if( shortNameMembers != NULL )
		{
			shortNameMembersCount = CFArrayGetCount( shortNameMembers );
			if( outShortNameMembers != NULL )
			{
				*outShortNameMembers = shortNameMembers;
				CFRetain( shortNameMembers );
			}
		}
		if( guidMembers != NULL )
			guidMembersCount = CFArrayGetCount( guidMembers );
		
		if( outIsLegacy != NULL )
			*outIsLegacy = ( ( guidMembersCount == 0 ) && ( shortNameMembersCount > 0 ) );
	}
	
	if( groupAttrsVals != NULL )
		CFRelease( groupAttrsVals );
	
	return dirStatus;
}

#pragma mark -
#pragma mark ••• Private Functions •••

tDirStatus _HLDSBuildDataListFromCFArray( CFArrayRef inArray, const tDirReference inDirRef, tDataListPtr* outAllocatedDataList )
{
	CFIndex				i;
	CFIndex				cfIndex;
	tDirStatus			dirStatus = eDSNoErr;
	dsBool				cStringAllocd = false;
	CFStringRef			cfString;
	const char*			cStringPtr = NULL;
	char*				cString = NULL;

	for( i=0; ( dirStatus == eDSNoErr ) && ( i<CFArrayGetCount( inArray ) ); i++ )
	{
		if( dirStatus == eDSNoErr )
		{
			cfString = CFArrayGetValueAtIndex( inArray, i );
			if( cfString == NULL )
				dirStatus = eParameterError;
		}
	
		if( dirStatus == eDSNoErr )
		{
			cStringAllocd = false;
			cStringPtr = CFStringGetCStringPtr( cfString, kCFStringEncodingUTF8 );
			if( cStringPtr == NULL )	//couldn't get the string ptr directly.
			{
				cfIndex = CFStringGetLength( cfString );
				cfIndex = CFStringGetMaximumSizeForEncoding( cfIndex + 1, kCFStringEncodingUTF8 );
				cString = malloc( cfIndex + 1 );
				cStringAllocd = true;
				if( cString == NULL )
					dirStatus = eMemoryAllocError;
				if( ( dirStatus == eDSNoErr ) && !CFStringGetCString( cfString, cString, cfIndex + 1, kCFStringEncodingUTF8 ) )
					dirStatus = eUndefinedError;
			}
		}
	
		if( dirStatus == eDSNoErr )
		{
			if( i == 0 )
			{
				if( strlen( cStringPtr == NULL ? cString : cStringPtr ) == 0 )
					*outAllocatedDataList = dsBuildListFromStrings( inDirRef, kDSRecordsAll, NULL );
				else
					*outAllocatedDataList = dsBuildListFromStrings( inDirRef, cStringPtr == NULL ? cString : cStringPtr, NULL );
				if( *outAllocatedDataList == NULL )
					dirStatus = eMemoryAllocError;
			}
			else
				dirStatus = dsAppendStringToListAlloc( inDirRef, *outAllocatedDataList, cStringPtr == NULL ? cString : cStringPtr );
		}
		if( ( cStringAllocd ) && ( cString != NULL ) )
		{
			free( cString );
			cString = NULL;
		}
	}

	return dirStatus;
}

tDirStatus _HLDSGetAttributeValuesFromRecordsByName( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
	CFArrayRef inRecordNames, CFArrayRef inAttributesToGet, CFArrayRef* outAttributeValues, dsBool limitNumRecordsToNumNames )
{
	tDirStatus					dirStatus = eDSNoErr;
	dsBool						loopAgain, attributeIsText;
	tDataBufferPtr				dataBuff = NULL;
	unsigned long				i;
	unsigned long				j;
	unsigned long				k;
	tDataListPtr				recTypes = NULL;
	tDataListPtr				recNames = NULL;
	tDataListPtr				attributeList = NULL;
	unsigned long				recCount = 0;
	unsigned long				buffSize;
	tContextData				continueData = NULL;
	CFMutableArrayRef			mutableValues = NULL;
	CFMutableArrayRef			mutableAttributesAndValues = NULL;
	CFMutableDictionaryRef		mutableDict = NULL;
	CFStringRef					cfString = NULL;

	tRecordEntry*				recEntry = NULL;
	tAttributeListRef			attrListRef = 0;
	tAttributeEntry*			attrEntry = NULL;
	tAttributeValueListRef		valueRef = 0;
	tAttributeValueEntry*		attrValueEntry = NULL;
	char*						stringBuffer = NULL;
	char*						stringBufferToUse = NULL;
	CFIndex						stringBufferSize = 0;

	if( ( inRecordNames == NULL ) || ( CFArrayGetCount( inRecordNames ) == 0 ) )
	{
		*outAttributeValues = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
		return eDSNoErr;
	}
	
	if( limitNumRecordsToNumNames )
		recCount = CFArrayGetCount( inRecordNames );
	
	stringBufferSize = 256;
	stringBuffer = malloc( stringBufferSize );
	if( stringBuffer == NULL )
		return eMemoryAllocError;

	mutableAttributesAndValues = CFArrayCreateMutable( NULL, recCount, &kCFTypeArrayCallBacks );
	if( dirStatus == eDSNoErr )
	{
		dataBuff = dsDataBufferAllocate( inDirRef, 1 * 1024 ); // allocate a 1k buffer
		if( dataBuff == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
	{
		recTypes = dsBuildListFromStrings( inDirRef, inDSRecordType, NULL );
		if( recTypes == NULL )
			dirStatus = eMemoryAllocError;
	}

	if( dirStatus == eDSNoErr )
		dirStatus = _HLDSBuildDataListFromCFArray( inRecordNames, inDirRef, &recNames );
	
	if( dirStatus == eDSNoErr )
		dirStatus = _HLDSBuildDataListFromCFArray( inAttributesToGet, inDirRef, &attributeList );
	
	for( loopAgain=true; loopAgain && ( dirStatus == eDSNoErr ); )
	{
		loopAgain=false;
		dirStatus = dsGetRecordList( inDirNodeRef, dataBuff, recNames, eDSExact, recTypes,
			attributeList, false, &recCount, &continueData );
		if ( dirStatus == eDSBufferTooSmall )
		{
			buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDirRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDirRef, buffSize * 2 );
			if( dataBuff == NULL )
				dirStatus = eMemoryAllocError;
			else
			{
				loopAgain = true;
				dirStatus = eDSNoErr;
			}
			if( continueData != NULL )
			{
				dsReleaseContinueData( inDirNodeRef, continueData );
				continueData = NULL;
			}
		}
		
		if( continueData != NULL )
			loopAgain = true;
			
		for( i=1; ( dirStatus == eDSNoErr ) && ( i<=recCount ); i++ )
		{
			if( dirStatus == eDSNoErr )
				dirStatus = dsGetRecordEntry( inDirNodeRef, dataBuff, i, &attrListRef, &recEntry );
			
			//create a dictionary to hold the attribute names and their respective array of values
			if( dirStatus == eDSNoErr )
				mutableDict = CFDictionaryCreateMutable( NULL, recEntry->fRecordAttributeCount, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			if( mutableDict == NULL )
				dirStatus = eMemoryAllocError;
			for( j=1, attributeIsText=true; attributeIsText && ( dirStatus == eDSNoErr ) && ( j<=recEntry->fRecordAttributeCount ); j++ )
			{
				if( dirStatus == eDSNoErr )
					dirStatus = dsGetAttributeEntry( inDirNodeRef, dataBuff, attrListRef, j, &valueRef, &attrEntry );

				//create an array to hold all of the attribute values
				if( dirStatus == eDSNoErr )
					mutableValues = CFArrayCreateMutable( NULL, attrEntry->fAttributeValueCount, &kCFTypeArrayCallBacks );
				if( mutableValues == NULL )
					dirStatus = eMemoryAllocError;
				for( k=1; attributeIsText && ( dirStatus == eDSNoErr ) && ( k<=attrEntry->fAttributeValueCount ); k++ )
				{
					dirStatus = dsGetAttributeValue( inDirNodeRef, dataBuff, k, valueRef, &attrValueEntry );
					if( dirStatus == eDSNoErr )
					{
						if( attrValueEntry->fAttributeValueData.fBufferSize >= ( attrValueEntry->fAttributeValueData.fBufferLength + 1 ) )
						{
							attrValueEntry->fAttributeValueData.fBufferData[attrValueEntry->fAttributeValueData.fBufferLength] = '\0';
							stringBufferToUse = attrValueEntry->fAttributeValueData.fBufferData;
						}
						else
						{
							if( ( attrValueEntry->fAttributeValueData.fBufferLength + 1 ) > stringBufferSize )
							{
								free( stringBuffer );
								stringBufferSize = attrValueEntry->fAttributeValueData.fBufferLength + 1;
								stringBuffer = malloc( stringBufferSize );
								if( stringBuffer == NULL )
									dirStatus = eMemoryAllocError;
							}
							memmove( stringBuffer, attrValueEntry->fAttributeValueData.fBufferData, attrValueEntry->fAttributeValueData.fBufferLength );
							stringBuffer[attrValueEntry->fAttributeValueData.fBufferLength] = '\0';
							stringBufferToUse = stringBuffer;
						}
						if( dirStatus == eDSNoErr )
						{
							cfString = CFStringCreateWithCString( NULL, stringBufferToUse, kCFStringEncodingUTF8 );
							if( cfString == NULL )
								cfString = CFStringCreateWithCString( NULL, "<Non-Text Value>", kCFStringEncodingUTF8 );
							if( cfString == NULL )
								dirStatus = eMemoryAllocError;
						}
					}
					if( ( dirStatus == eDSNoErr ) && attributeIsText )
						CFArrayAppendValue( mutableValues, cfString );
					if( cfString != NULL )	//release so that it's only retained by the array
					{
						CFRelease( cfString );
						cfString = NULL;
					}

					if( attrValueEntry != NULL )
					{
						dsDeallocAttributeValueEntry( inDirRef, attrValueEntry );
						attrValueEntry = NULL;
					}
				}
				if( dirStatus == eDSNoErr )
				{
					cfString = NULL;
					cfString = CFStringCreateWithCString( NULL, attrEntry->fAttributeSignature.fBufferData, kCFStringEncodingUTF8 );
					if( cfString == NULL )
						dirStatus = eMemoryAllocError;
				}
				if( dirStatus == eDSNoErr )
					CFDictionarySetValue( mutableDict, cfString, mutableValues );
				if( cfString != NULL )	//release so that it's only retained by the dictionary
				{
					CFRelease( cfString );
					cfString = NULL;
				}
				if( mutableValues != NULL )	//release so that it's only retained by the dictionary
				{
					CFRelease( mutableValues );
					mutableValues = NULL;
				}

				if( attrEntry != NULL )
				{
					dsDeallocAttributeEntry	( inDirRef, attrEntry );
					attrEntry = NULL;
				}
				if( valueRef != 0 )
				{
					dsCloseAttributeValueList( valueRef );
					valueRef = 0;
				}
			}
			if( ( dirStatus == eDSNoErr ) && attributeIsText )
				CFArrayAppendValue( mutableAttributesAndValues, mutableDict );

			if( mutableDict != NULL )	//release so that it's only retained by the array
			{
				CFRelease( mutableDict );
				mutableDict = NULL;
			}
			if( recEntry != NULL )
			{
				dsDeallocRecordEntry( inDirRef, recEntry );
				recEntry = NULL;
			}
			if( attrListRef != 0 )
			{
				dsCloseAttributeList( attrListRef );
				attrListRef = 0;
			}
		}
		if( ( dirStatus != eDSNoErr ) && ( continueData != NULL ) )
		{
			dsReleaseContinueData( inDirNodeRef, continueData );
			continueData = NULL;
		}
		if( loopAgain && ( dirStatus == eDSNoErr ) && ( dataBuff->fBufferSize < ( 128 * 1024 ) ) )
		{
			buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDirRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDirRef, ( buffSize * 3 ) / 2 );
			if( dataBuff == NULL )
				dirStatus = eMemoryAllocError;
		}
	}

	//cleanup
	if( stringBuffer != NULL )
		free( stringBuffer );

	if( recTypes != NULL )
	{
		dsDataListDeallocate( inDirRef, recTypes );
		free( recTypes );
		recTypes = NULL;
	}
	if( recNames != NULL )
	{
		dsDataListDeallocate( inDirRef, recNames );
		free( recNames );
		recNames = NULL;
	}
	if( attributeList != NULL )
	{
		dsDataListDeallocate( inDirRef, attributeList );
		free( attributeList );
		attributeList = NULL;
	}
	if( attrValueEntry != NULL )
	{
		dsDeallocAttributeValueEntry	( inDirRef, attrValueEntry );
		attrValueEntry = NULL;
	}
	if( attrEntry != NULL )
	{
		dsDeallocAttributeEntry	( inDirRef, attrEntry );
		attrEntry = NULL;
	}
	if( valueRef != 0 )
	{
		dsCloseAttributeValueList( valueRef );
		valueRef = 0;
	}
	if( recEntry != NULL )
	{
		dsDeallocRecordEntry( inDirRef, recEntry );
		recEntry = NULL;
	}
	if( attrListRef != 0 )
	{
		dsCloseAttributeList( attrListRef );
		attrListRef = 0;
	}
	if( continueData != NULL )
	{
		dsReleaseContinueData( inDirNodeRef, continueData );
		continueData = NULL;
	}
	if( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( inDirRef, dataBuff );
		dataBuff = NULL;
	}

	if( dirStatus == eDSNoErr )
		*outAttributeValues = CFArrayCreateCopy( NULL, mutableAttributesAndValues );
	
	if( mutableAttributesAndValues != NULL )
	{
		CFRelease( mutableAttributesAndValues );
		mutableAttributesAndValues = NULL;
	}

	return dirStatus;
}

tDirStatus _HLDSLegacySetAttributeValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
	char createIfNecessary, const char** inAttributeValues, unsigned long inNumValues )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataNodePtr attrType = NULL;
	tDataNodePtr attrValue = NULL;
	tAttributeValueEntryPtr newAttrValue = NULL;
	tAttributeValueEntryPtr attrValueEntry = NULL;
	unsigned long i;
	unsigned long j;
	unsigned long k;
	dsBool attributeIsRecordName = false;
	tAccessControlEntry theACLEntry;

	attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );
	if( strcmp( inAttributeName, kDSNAttrRecordName ) == 0 )
		attributeIsRecordName = true;

	if( ( inNumValues == 1 ) && ( strlen( inAttributeValues[0] ) == 0 ) )
	{
		if( inAttributeValues[0] == NULL )
			dirStatus = eDSNullParameter;

		if( dirStatus == eDSNoErr )
		{
			dirStatus = dsRemoveAttribute( inRecordRef, attrType );
			switch( dirStatus )
			{
				case eDSNoErr:
					break;
				case eDSSchemaError:
					break;
				default:
				{
					attrValueEntry = NULL;
					dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, 1, &attrValueEntry );
					if( dirStatus == eDSNoErr )
					{
						newAttrValue = dsAllocAttributeValueEntry( inDirRef, attrValueEntry->fAttributeValueID,
							(void*)kHLDSEmptyAttributeValueForRequiredAttributes, strlen( kHLDSEmptyAttributeValueForRequiredAttributes ) );
						dirStatus = dsSetAttributeValue( inRecordRef, attrType, newAttrValue );
					}
					if( newAttrValue != NULL )
					{
						dsDeallocAttributeValueEntry ( inDirRef, newAttrValue );
						newAttrValue = NULL;
					}
					if( attrValueEntry != NULL )
					{
						dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
						attrValueEntry = NULL;
					}
					break;
				}
			}
		}
	}
	else
	{
		if( dirStatus == eDSNoErr )
		{   //delete all but the first value so we don't end up with an intermediate state where we have two of the same value
			for( k=2; dirStatus == eDSNoErr; k++ )
			{
				attrValueEntry = NULL;
				dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, k, &attrValueEntry );
				if( dirStatus == eDSNoErr )
					dirStatus = dsRemoveAttributeValue( inRecordRef, attrType, attrValueEntry->fAttributeValueID );
				
				if( attrValueEntry != NULL )
				{
					dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
					attrValueEntry = NULL;
				}
			}
			dirStatus = eDSNoErr;
		}

		for( i=0, j=1; ( i<inNumValues ) && ( dirStatus == eDSNoErr ); i++, j++ )
		{
			if( strlen( inAttributeValues[i] ) == 0 )	
				continue;

			if( inAttributeValues[i] == NULL )
				dirStatus = eDSNullParameter;
			
			if( ( i == 0 ) && attributeIsRecordName )	//don't allow setting the first short name with this function
				continue;

			attrValue = NULL;
			newAttrValue = NULL;
			attrValueEntry = NULL;
	
			if( dirStatus == eDSNoErr )
				dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, j, &attrValueEntry );
	
			switch( dirStatus )
			{
				case eDSNoErr:
					newAttrValue = dsAllocAttributeValueEntry( inDirRef, attrValueEntry->fAttributeValueID,
						(void*)inAttributeValues[i], strlen( inAttributeValues[i] ) );
					dirStatus = dsSetAttributeValue( inRecordRef, attrType, newAttrValue );
					break;
				case eDSAttributeNotFound:
					attrValue = dsDataNodeAllocateString( inDirRef, inAttributeValues[i] );
					memset( &theACLEntry, 0, sizeof( theACLEntry ) );
					dirStatus = dsAddAttribute( inRecordRef, attrType, &theACLEntry, attrValue );
					break;
				case eDSInvalidIndex:
				case eDSIndexOutOfRange:
				case eDSIndexNotFound:
					attrValue = dsDataNodeAllocateString( inDirRef, inAttributeValues[i] );
					dirStatus = dsAddAttributeValue( inRecordRef, attrType, attrValue);
					break;
				default:
					break;
			}
			if( attrValue != NULL )
			{
				dsDataNodeDeAllocate( inDirRef, attrValue );
				attrValue = NULL;
			}
			if( newAttrValue != NULL )
			{
				dsDeallocAttributeValueEntry ( inDirRef, newAttrValue );
				newAttrValue = NULL;
			}
			if( attrValueEntry != NULL )
			{
				dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
				attrValueEntry = NULL;
			}
		}
	}

	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}
	return dirStatus;
}

tDirStatus _HLDSLegacySetAttributeCFValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
	char createIfNecessary, CFArrayRef inAttributeValues )
{
	tDirStatus dirStatus = eDSNoErr;
	tDataNodePtr attrType = NULL;
	tDataNodePtr attrValue = NULL;
	tAttributeValueEntryPtr newAttrValue = NULL;
	tAttributeValueEntryPtr attrValueEntry = NULL;
	CFIndex i;
	CFIndex j;
	dsBool attributeIsRecordName = false;
	tAccessControlEntry theACLEntry;
	CFStringRef attributeValue = NULL;
	const char* attributeValueCString = NULL;
	char* allocatedCString = NULL;
	CFIndex bufferSize = 0;
	CFIndex numValues = CFArrayGetCount( inAttributeValues );

	attrType = dsDataNodeAllocateString( inDirRef, inAttributeName );
	if( strcmp( inAttributeName, kDSNAttrRecordName ) == 0 )
		attributeIsRecordName = true;

	if( numValues > 0 )
		attributeValue = CFArrayGetValueAtIndex( inAttributeValues, 0 );
	if( ( numValues == 1 ) && ( CFStringGetLength( attributeValue ) == 0 ) )
	{
		if( dirStatus == eDSNoErr )
		{
			dirStatus = dsRemoveAttribute( inRecordRef, attrType );
			switch( dirStatus )
			{
				case eDSNoErr:
					break;
				case eDSSchemaError:
					break;
				default:
				{
					attrValueEntry = NULL;
					dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, 1, &attrValueEntry );
					if( dirStatus == eDSNoErr )
					{
						newAttrValue = dsAllocAttributeValueEntry( inDirRef, attrValueEntry->fAttributeValueID,
							(void*)kHLDSEmptyAttributeValueForRequiredAttributes, strlen( kHLDSEmptyAttributeValueForRequiredAttributes ) );
						dirStatus = dsSetAttributeValue( inRecordRef, attrType, newAttrValue );
					}
					if( newAttrValue != NULL )
					{
						dsDeallocAttributeValueEntry ( inDirRef, newAttrValue );
						newAttrValue = NULL;
					}
					if( attrValueEntry != NULL )
					{
						dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
						attrValueEntry = NULL;
					}
					break;
				}
			}
		}
	}
	else
	{
		for( i=0, j=1; ( i<numValues ) && ( dirStatus == eDSNoErr ); i++, j++ )
		{
			attributeValue = CFArrayGetValueAtIndex( inAttributeValues, i );
			if( CFStringGetLength( attributeValue ) == 0 )	
				continue;

			if( attributeValue == NULL )
				dirStatus = eDSNullParameter;
			
			if( ( i == 0 ) && attributeIsRecordName )	//don't allow setting the first short name with this function
				continue;

			attrValue = NULL;
			newAttrValue = NULL;
			attrValueEntry = NULL;
	
			if( dirStatus == eDSNoErr )
				dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, j, &attrValueEntry );
	
			attributeValueCString = CFStringGetCStringPtr( attributeValue, kCFStringEncodingUTF8 );
			if( attributeValueCString == NULL )
			{
				bufferSize = 2 * CFStringGetLength( attributeValue );
				allocatedCString = malloc( bufferSize );
				if( !CFStringGetCString( attributeValue, allocatedCString, bufferSize, kCFStringEncodingUTF8 ) )
				{
					dirStatus = eMemoryAllocError;
					free( allocatedCString );
					allocatedCString = NULL;
				}
				attributeValueCString = allocatedCString;
			}
			switch( dirStatus )
			{
				case eDSNoErr:
					newAttrValue = dsAllocAttributeValueEntry( inDirRef, attrValueEntry->fAttributeValueID,
						(void*)attributeValueCString, strlen( attributeValueCString ) );
					dirStatus = dsSetAttributeValue( inRecordRef, attrType, newAttrValue );
					break;
				case eDSAttributeNotFound:
					attrValue = dsDataNodeAllocateString( inDirRef, attributeValueCString );
					memset( &theACLEntry, 0, sizeof( theACLEntry ) );
					dirStatus = dsAddAttribute( inRecordRef, attrType, &theACLEntry, attrValue );
					break;
				case eDSInvalidIndex:
				case eDSIndexOutOfRange:
				case eDSIndexNotFound:
					attrValue = dsDataNodeAllocateString( inDirRef, attributeValueCString );
					dirStatus = dsAddAttributeValue( inRecordRef, attrType, attrValue);
					break;
				default:
					break;
			}
			if( allocatedCString != NULL )
			{
				free( allocatedCString );
				allocatedCString = NULL;
			}
			if( attrValue != NULL )
			{
				dsDataNodeDeAllocate( inDirRef, attrValue );
				attrValue = NULL;
			}
			if( newAttrValue != NULL )
			{
				dsDeallocAttributeValueEntry ( inDirRef, newAttrValue );
				newAttrValue = NULL;
			}
			if( attrValueEntry != NULL )
			{
				dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
				attrValueEntry = NULL;
			}
		}
	
		if( dirStatus == eDSNoErr )
		{
			for( ; dirStatus == eDSNoErr; j++ )
			{
				attrValueEntry = NULL;
				dirStatus = dsGetRecordAttributeValueByIndex( inRecordRef, attrType, j, &attrValueEntry );
				if( dirStatus == eDSNoErr )
					dirStatus = dsRemoveAttributeValue( inRecordRef, attrType, attrValueEntry->fAttributeValueID );
				
				if( attrValueEntry != NULL )
				{
					dsDeallocAttributeValueEntry ( inDirRef, attrValueEntry );
					attrValueEntry = NULL;
				}
			}
			dirStatus = eDSNoErr;
		}
	}

	if( attrType != NULL )
	{
		dsDataNodeDeAllocate( inDirRef, attrType );
		attrType = NULL;
	}
	return dirStatus;
}
