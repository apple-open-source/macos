/*
 *  CSMBNodeLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CSMBNodeLookupThread_
#define _CSMBNodeLookupThread_ 1

#include "CNSLNodeLookupThread.h"

class CSMBNodeLookupThread : public CNSLNodeLookupThread
{
public:
                            CSMBNodeLookupThread			( CNSLPlugin* parentPlugin );
    virtual					~CSMBNodeLookupThread			();
    
	virtual void*			Run								( void );

protected:
			void			DoMasterBrowserLookup			( const char* winsServer );
			char*			GetNextMasterBrowser			( char** buffer );
			OSStatus		DoMachineLookup					( const char* machineAddress, const char* winsServer );
			char*			GetNextWorkgroup				( char** buffer );

private:
};
#endif		// #ifndef