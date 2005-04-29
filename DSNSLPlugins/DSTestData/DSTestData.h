/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *  @header DSTestData
 */

#ifndef _DSTestData_
#define _DSTestData_

#include "CNSLPlugin.h"

#define kProtocolPrefixPlainStr		"DSTestData"
#define kProtocolPrefixStr			"/DSTestData"
#define kProtocolPrefixSlashStr		"/DSTestData/"

#define kNoZoneLabel					"*"

#define kNBPThreadUSleepOnCount					25				// thread yield after reporting this number of items
#define kNBPThreadUSleepInterval				2500			// number of microseconds to pause the thread

#define kMaxZonesOnTryOne			1000
#define kMaxServicesOnTryOne		1000

#define kOnTheFlySetup						'otfs'

#define kWriteDSTestConfigXMLData			'wcfg'
#define kReadDSTestConfigXMLData			'rcfg'

#define kReadDSTestStaticDataXMLData		'rxml'
#define kWriteDSTestStaticDataXMLData		'wxml'

#define kReadDSTestStaticDataFromFile		'rffl'

#define kAddNewTopLevelNode					'adtn'
#define kAddNewNode							'adnn'

class DSTestData : public CNSLPlugin
{
public:
                                DSTestData				( void );
                                ~DSTestData				( void );
    
    virtual sInt32				InitPlugin				( void );
	virtual	void				ActivateSelf						( void );
	virtual	void				DeActivateSelf						( void );

			void				AddNode					( CFStringRef nodePathRef, tDataList* nodePathList, CFMutableDictionaryRef serviceList );

            Boolean				IsScopeInReturnList		( const char* scope );
            void				AddResult				( const char* url );
    
            uInt32				fSignature;

            void				SetLocalZone			( const char* zone );
    const	char*				GetLocalZone			( void ) { return mLocalNodeString; }
protected:
    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual Boolean 			IsLocalNode				( const char *inNode );
    
    virtual	Boolean				ReadOnlyPlugin			( void ) { return false; }
    virtual	Boolean				IsClientAuthorizedToCreateRecords ( sCreateRecord *inData ) { return true; }

    virtual void				NewNodeLookup			( void );		// this should fire off some threads in the subclass
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );    
	virtual	sInt32				DoPlugInCustomCall					( sDoPlugInCustomCall *inData );
	
    virtual	sInt32				RegisterService			( tRecordReference recordRef, CFDictionaryRef service );
    virtual	sInt32				DeregisterService		( tRecordReference recordRef, CFDictionaryRef service );

			sInt32				DoOnTheFlySetup						( sDoPlugInCustomCall *inData );
			sInt32				FillOutCurrentStaticDataWithXML		( sDoPlugInCustomCall *inData );
			sInt32				SaveNewStaticDataFromXML			( sDoPlugInCustomCall *inData );
			sInt32				LoadStaticDataFromFile				( CFStringRef pathToFile );
			sInt32				SaveCurrentStateToDefaultDataFile	( void );
			sInt32				AddNewTopLevelNode					( sDoPlugInCustomCall *inData );
			sInt32				AddNewNode							( sDoPlugInCustomCall *inData );
			sInt32				RegisterStaticConfigData			( CFDictionaryRef parentNodeRef, CFStringRef parentPathRef );

private:
            char*						mLocalNodeString;		
            SCDynamicStoreRef			mNBPSCRef;
			CFMutableDictionaryRef		mStaticRegistrations;
			Boolean						mOnTheFlyEnabled;
			sInt32						mNumberOfNeighborhoodsToGenerate;
			sInt32						mNumberOfServicesToGenerate;

};

#endif
