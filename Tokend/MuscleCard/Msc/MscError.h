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
 *  MscError.h
 *  TokendMuscle
 */

#ifndef _MSCERROR_H_
#define _MSCERROR_H_

#include <security_utilities/debugging.h>
#include <security_utilities/errors.h>
#include <PCSC/musclecard.h>
#include <PCSC/pcsclite.h>

class MscError : public Security::CommonError
{
protected:
    MscError(int err);
public:
    const int error;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
    virtual const char *what () const throw ();
    
    static void check(OSStatus status)	{ if (status!=MSC_SUCCESS && status!=SCARD_S_SUCCESS) throwMe(status); }
    static void throwMe(int err) __attribute__((noreturn));

protected:
	IFDEBUG(void debugDiagnose(const void *id) const;)
	IFDEBUG(const char *mscerrorstr(int err) const;)
};

#endif /* !_MSCERROR_H_ */

