/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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


/*
 * cssmerrors
 */
#ifndef _H_CSSMERRORS
#define _H_CSSMERRORS

#include <security_utilities/errors.h>
#include <Security/cssmtype.h>

namespace Security
{

//
// A CSSM-originated error condition, represented by a CSSM_RETURN value.
// This can represent both a convertible base error, or a module-specific
// error condition.
//
class CssmError : public CommonError {
protected:
    CssmError(CSSM_RETURN err);
public:
    const CSSM_RETURN error;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
    virtual const char *what () const throw ();

    static CSSM_RETURN merge(CSSM_RETURN error, CSSM_RETURN base);
    
	static void check(CSSM_RETURN error)	{ if (error != CSSM_OK) throwMe(error); }
    static void throwMe(CSSM_RETURN error) __attribute__((noreturn));

	//
	// Obtain a CSSM_RETURN from any CommonError
	//
	static CSSM_RETURN cssmError(CSSM_RETURN error, CSSM_RETURN base);
	static CSSM_RETURN cssmError(const CommonError &error, CSSM_RETURN base);
};



} // end namespace Security


#endif //_H_CSSMERRORS
