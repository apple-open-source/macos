/*
 *  CSMBServiceLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CSMBServiceLookupThread_
#define _CSMBServiceLookupThread_ 1

#include "CNSLServiceLookupThread.h"

class CSMBServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            CSMBServiceLookupThread		( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual					~CSMBServiceLookupThread	();
        
	virtual void*			Run							( void );
protected:
            char*			GetMachineName				( char* machineAddress );
			OSStatus		SMBServiceLookupNotifier	( char* machineName );
			char*			GetNextMachine				( char** buffer );
private:
};
#endif		// #ifndef