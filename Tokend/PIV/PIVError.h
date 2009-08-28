/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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
 *  PIVError.h
 *  TokendPIV
 */

/* ---------------------------------------------------------------------------
 *
 *		MODIFY
 *		- Fill in your token specific error codes below
 *
 * ---------------------------------------------------------------------------
*/

#ifndef _PIVERROR_H_
#define _PIVERROR_H_

#include "SCardError.h"


/** Entered PIN is not correct and pin was blocked. */
#define PIV_AUTHENTICATION_FAILED_0        0x6300
/** Entered PIN is not correct, 1 try left. */
#define PIV_AUTHENTICATION_FAILED_1        0x6301
/** Entered PIN is not correct, 2 tries left. */
#define PIV_AUTHENTICATION_FAILED_2        0x6302
/** Entered PIN is not correct, 3 tries left. */
#define PIV_AUTHENTICATION_FAILED_3        0x6303

class PIVError : public Tokend::SCardError
{
protected:
    PIVError(uint16_t sw);
    virtual ~PIVError() throw ();
public:
	OSStatus osStatus() const;
	virtual const char *what () const throw ();

    static void check(uint16_t sw)	{ if (sw != SCARD_SUCCESS) throwMe(sw); }
    static void throwMe(uint16_t sw) __attribute__((noreturn));
    
protected:
#if MAX_OS_X_VERSION_MIN_REQUIRED <= MAX_OS_X_VERSION_10_5
    IFDEBUG(void debugDiagnose(const void *id) const;)
#endif
    IFDEBUG(const char *errorstr(uint16_t sw) const;)
};

#endif /* !_CACERROR_H_ */

