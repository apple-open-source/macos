/*
 *  CNBPServiceLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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