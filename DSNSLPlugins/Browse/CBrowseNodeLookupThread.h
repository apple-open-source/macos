/*
 *  CBrowseNodeLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CBrowseNodeLookupThread_
#define _CBrowseNodeLookupThread_ 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CNSLNodeLookupThread.h"

class CBrowseNodeLookupThread : public CNSLNodeLookupThread
{
public:
                                CBrowseNodeLookupThread		( CNSLPlugin* parentPlugin );
    virtual						~CBrowseNodeLookupThread		();
    
	virtual void*				Run							( void );
protected:
            char*				mBuffer;
    
private:
};
#endif		// #ifndef