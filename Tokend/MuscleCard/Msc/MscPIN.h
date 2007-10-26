/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  MscPIN.h
 *  TokendMuscle
 */

#ifndef _MSCPIN_H_
#define _MSCPIN_H_

#include "MscWrappers.h"
#include "MscTokenConnection.h"
#include <PCSC/musclecard.h>

class MscPIN
{
public:
    MscPIN() { }
    MscPIN(MscTokenConnection *connection) { mConnection = connection; }
    virtual ~MscPIN() {};
    
	void create(unsigned int pinNum,unsigned int pinAttempts,const char *PIN, size_t PINSize,
		const char *unblockPIN, size_t unblockPINSize);
	void change(unsigned int pinNum,const char *oldPIN, size_t oldPINSize,const char *newPIN, size_t newPINSize);
	void unblock(unsigned int pinNum,const char *unblockCode, size_t unblockCodeSize);
	void list(MSCUShort16& mask);

protected:
	MscTokenConnection *mConnection;
};

#endif /* !_MSCPIN_H_ */

