/*
 *  CNBPNodeLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CNBPNodeLookupThread_
#define _CNBPNodeLookupThread_ 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "zonelist.h"
#include "serverlist.h"
//#include "GenericNBPURL.h"

#include "CNSLNodeLookupThread.h"

class CNBPNodeLookupThread : public CNSLNodeLookupThread
{
public:
                                CNBPNodeLookupThread		( CNSLPlugin* parentPlugin );
    virtual						~CNBPNodeLookupThread		();
    
	virtual void*				Run							( void );
    
            void				DoLocalZoneLookup			( void );
protected:
            char*				mBuffer;
            NBPNameAndAddress*	mNABuffer;
    
private:
};
#endif		// #ifndef