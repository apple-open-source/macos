/*
 *  CRFServiceLookupThread.h
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Thu Feb 28 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#ifndef _CRFServiceLookupThread_
#define _CRFServiceLookupThread_ 1

#include "CNSLServiceLookupThread.h"

class CRFServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            CRFServiceLookupThread		( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual					~CRFServiceLookupThread		();
        
	virtual void*			Run							( void );

protected:
            void			GetRecordsFromType			( void );
            void			ReadRecordFromFile			( void );
            void			GetRecFavServices			( Boolean recents, char* serviceType, CNSLDirNodeRep* nodeToSearch );


private:
};
#endif		// #ifndef