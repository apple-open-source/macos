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
 *  @header CNBPServiceLookupThread
 */
 
#ifndef _CNBPServiceLookupThread_
#define _CNBPServiceLookupThread_ 1

#include "zonelist.h"
#include "serverlist.h"

#include "CNSLServiceLookupThread.h"

// results for ConvertToLocalZoneIfThereAreNoZones
enum {
	kNotConverted,
	kConvertedToLocal,
	kMustSearchZoneNameAppleTalk
};


class CNBPServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            CNBPServiceLookupThread				( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual					~CNBPServiceLookupThread			();
        
	virtual void*			Run									( void );
    
            void			SetDefaultNeighborhoodNamePtr		( const char *name );

protected:
            OSStatus		DoLookupOnService					( char* service, char *zone );
	virtual short			ConvertToLocalZoneIfThereAreNoZones	( char* zoneName );
            int				NBPGetServerList					( char *service,
                                                                char *curr_zone,
                                                                struct NBPNameAndAddress *buffer,
                                                                long *actualCount );
            

private:
            CFStringRef				mServiceListRef;
            char*					mBuffer;
            NBPNameAndAddress*		mNABuffer;
    const	char*					mDefaultNeighborhoodName;
};

#endif		// #ifndef
