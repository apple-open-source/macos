/*
 *  CSLPNodeLookupThread.h
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
 
#ifndef _CSLPNodeLookupThread_
#define _CSLPNodeLookupThread_ 1

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "SLPDefines.h"

#include "CNSLNodeLookupThread.h"

class CSLPNodeLookupThread : public CNSLNodeLookupThread
{
public:
                            CSLPNodeLookupThread		( CNSLPlugin* parentPlugin );
    virtual					~CSLPNodeLookupThread		();
    
	virtual void*			Run							( void );
			void			Cancel						( void ) { mCanceled = true; }
			Boolean			IsCanceled					( void ) { return mCanceled; }
			void			DoItAgain					( void ) { mDoItAgain = true; }
protected:
            void			GetRecFavNames				( void );
private:
    SLPHandle				mSLPRef;
	Boolean					mCanceled;
	Boolean					mDoItAgain;
};
#endif		// #ifndef