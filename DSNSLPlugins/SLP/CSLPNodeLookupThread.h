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
 *  @header CSLPNodeLookupThread
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
