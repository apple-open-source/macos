/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef __CAuthFileUtils__
#define __CAuthFileUtils__

#include <PasswordServer/AuthFile.h>
#include <PasswordServer/DES.h>

class CAuthFileUtils
{
    public:
    
                                        CAuthFileUtils();
        virtual                         ~CAuthFileUtils();
    	
		virtual void					getGMTime(struct tm *outGMT);
        virtual void					getGMTime(BSDTimeStructCopy *outGMT);

		virtual long					slotToOffset(long slot);
        
		// DES accessors
        virtual void					DESEncode(void *data, unsigned long inDataLen);
        virtual void					DESDecode(void *data, unsigned long inDataLen);
		virtual void					DESAutoDecode(void *data);
		
    protected:
		
		KeysArray mDESKeyArray;
};

#endif

