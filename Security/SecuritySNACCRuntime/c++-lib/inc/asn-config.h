/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


// file: .../c++-lib/inc/asn-config.h - decoder alloc routines and buffer routines and other configuration stuff.
//
// MS 92/06/18
// Copyright (C) 1992 Michael Sample and the University of British Columbia
//
// This library is free software; you can redistribute it and/or
// modify it provided that this copyright/license information is retained
// in original form.
//
// If you modify this file, you must clearly indicate your changes.
//
// This source code is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/inc/asn-config.h,v 1.7 2002/06/12 18:36:31 dmitch Exp $
// $Log: asn-config.h,v $
// Revision 1.7  2002/06/12 18:36:31  dmitch
// Radar 2951933: Avoid including iostream in asn-config.h for NDEBUG builds.
//
// Revision 1.6.94.4  2002/06/11 22:59:03  dmitch
// Radar 2951933.
//
// Revision 1.6.94.3  2002/06/11 22:34:50  dmitch
// More endl cleanup.
//
// Revision 1.6.94.2  2002/06/11 22:27:28  dmitch
// Clean up endl declaration for NDEBUG case.
//
// Revision 1.6.94.1  2002/06/10 23:19:08  dmitch
// Radar 2934358 - avoid #include \<iostream\> in asn-config.h
//
// Revision 1.6  2002/04/18 18:58:08  dmitch
// Radar 2904404 - avoid deprecated iostream.h
//
// Revision 1.5.24.1  2002/04/17 00:48:53  dmitch
// Radar 2904404 - avoid deprecated iostream.h.
//
// Revision 1.5  2002/03/21 05:38:47  dmitch
// Radar 2868524: no more setjmp/longjmp in SNACC-generated code.
//
// Revision 1.4.32.4  2002/03/20 20:56:37  dmitch
// Further refinements for Radar 2868524: no more BDecPdu or BEncPdu.
//
// Revision 1.4.32.3  2002/03/20 02:54:08  dmitch
// Avoid the unused and uninitialized ENV_TYPE var in BDecPdu.
//
// Revision 1.4.32.2  2002/03/20 01:28:02  dmitch
// Added throw() to SnaccExcep destructor.
//
// Revision 1.4.32.1  2002/03/20 00:36:52  dmitch
// Radar 2868524: SNACC-generated code now uses throw/catch instead of setjmp/longjmp.
//
// Revision 1.4  2002/02/07 04:30:04  mb
// Fixes required to build with gcc3.
// Merged from branch PR-2848996
// Bug #: 2848996
// Submitted by:
// Reviewed by: Turly O'Connor <turly@apple.com>
//
// Revision 1.3.10.1  2002/02/06 23:45:04  mb
// Changes to allow building with gcc3
//
// Revision 1.3  2001/06/27 23:09:16  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.2  2001/06/26 23:47:26  dmitch
// Implemented AsnNullError as replacement for cerr in NDEBUG configuration.
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/15 18:48:23  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.3  1999/04/08 21:20:18  aram
// Fixed Asn1Free macro.
//
// Revision 1.2  1999/03/16 03:04:30  mb
// Added common base class SecObject and implemented signing of the digest in SecSigner.
//
// Revision 1.1  1999/02/25 05:21:41  mb
// Added snacc c++ library
//
// Revision 1.7  1995/09/07 18:48:36  rj
// AsnIntType and AsnUIntType introduced to replace (unsigned) long int at a lot of places.
// they shall provide 32 bit integer types on all platforms.
//
// Revision 1.6  1995/07/25  20:19:00  rj
// changed `_' to `-' in file names.
//
// Revision 1.5  1995/02/13  14:47:46  rj
// settings for IEEE_REAL_FMT/IEEE_REAL_LIB moved from {c_lib,c++_lib}/inc/asn_config.h to acconfig.h.
//
// Revision 1.4  1994/10/08  04:17:59  rj
// code for meta structures added (provides information about the generated code itself).
//
// code for Tcl interface added (makes use of the above mentioned meta code).
//
// virtual inline functions (the destructor, the Clone() function, BEnc(), BDec() and Print()) moved from inc/*.h to src/*.C because g++ turns every one of them into a static non-inline function in every file where the .h file gets included.
//
// made Print() const (and some other, mainly comparison functions).
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.3  1994/09/01  00:58:47  rj
// redundant code moved into ../../config.h.bot; semicolon removed from end of macro texts.
//
// Revision 1.2  1994/08/28  10:00:47  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:29  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_config_h_
#define _asn_config_h_

#include <ctype.h>  /* for isprint() in <<op on AsnOcts */

#ifdef	NDEBUG
/* just get forward declarations */
#include <iosfwd>
namespace std {
	extern ostream& endl(ostream& outs);
}
#else
#include <iostream>
#endif
/* assume these... */
using std::iostream;
using std::ostream;
using std::istream;
using std::endl;

#include <memory.h>
#include <string.h>
#include <setjmp.h>
#include <math.h> /* to get ieee conversion functions & pow */

#include "snacc.h"

using std::streamsize;

// used not only by AsnInt (asn-int.h), but by AsnNameDesc (meta.h) as well:
#if SIZEOF_INT == 4
#  define I		int
#else
#  if SIZEOF_LONG == 4
#    define I		long
#  else
#    if SIZEOF_SHORT == 4
#      define I		short
#    endif
#  endif
#endif
#ifdef I
  typedef I		AsnIntType;
  typedef unsigned I	AsnUIntType;
#  undef I
#else
#  error "can't find integer type which is 4 bytes in size"
#endif

/* used to test if optionals are present */
#define NOT_NULL( ptr)		((ptr) != NULL)


/*
 * Asn1Error (char *str)
 *  - configure error handler
 */
#ifndef	NDEBUG
#define Asn1Error		std::cerr
#else

/* silent ostream */
class Asn1ErrorClass;
class Asn1ErrorClass /* public ostream */
{
public:
     Asn1ErrorClass& put (char c) { return *this; };
     Asn1ErrorClass& write (const char * s, streamsize n) { return *this; };
     Asn1ErrorClass& flush () { return *this; };
	 template <class T>
	 Asn1ErrorClass &operator << (const T &) { return *this; }
};


#ifdef	__APPLE__

/* Avoid the static load-time init of AsnErrorClass */
#include <Security/globalizer.h>
extern ModuleNexus<Asn1ErrorClass> AsnNullError;

/* Avoid instantiating cerr and endl */
#define Asn1Error 	AsnNullError()
#define endl		AsnNullError()

#else	/* NDEBUG, !__APPLE__ */

/* This does not currently have an instantiation */
extern Asn1ErrorClass Asn1Error;

#endif	/* __APPLE__ */
#endif	/* NDEBUG */

/*
 * Asn1Warning (char *str)  - configure warning mechanism
 * (currently not called)
 */
#define Asn1Warning		Asn1Error


/*
 * configure memory scheme used by decoder to allocate memory
 * for the decoded value.  The include file name cannot be
 * be null.
 */
#define Asn1New( class)		new class
#define Asn1Delete( ptr)	delete ptr
#define Asn1Alloc( size)	new char[size]
#define Asn1Free( ptr)		delete[] ptr

#include "asn-buf.h"
#define BUF_TYPE		AsnBuf &

/*
 * Enables throw/catch as replacement for setjmp/longjmp in C++ lib. 
 * BDecPdu still returns int (1 = OK, 0 = fail) in either config.
 * The compiler gets this symbol from c-lib/inc/asn-config.h; runtime 
 * support gets this symbol from this file. There is currently no 
 * straightforward way to have one symbol used in both environments. 
 */
#define SNACC_EXCEPTION_ENABLE	1

/*
 * With the SNACC_EXCEPTION_ENABLE mods, ENV_TYPE is not used, though
 * it still appears in the BDec*() function.
 */
#if		SNACC_EXCEPTION_ENABLE
#define ENV_TYPE		int
#else
#define ENV_TYPE		jmp_buf
#endif	/* SNACC_EXCEPTION_ENABLE */

/*
 * Enable BEncPdu, BDecPdu. Same remarks apply as above w.r.t the 
 * c++ config file.
 */
#define SNACC_ENABLE_PDU		0
#if		SNACC_ENABLE_PDU

#if 	SNACC_EXCEPTION_ENABLE
/* return true if succeeded, false otherwise */
#define PDU_MEMBER_MACROS\
    int BEncPdu (BUF_TYPE b, AsnLen &bytesEncoded)\
    {\
	bytesEncoded = BEnc (b);\
	return !b.WriteError();\
    }\
\
    int BDecPdu (BUF_TYPE b, AsnLen &bytesDecoded)\
    {\
\
	bytesDecoded = 0;\
	try\
	{\
	    BDec (b, bytesDecoded, 0);\
	    return !b.ReadError();\
	}\
	catch(...) {\
	    return false;\
	}\
    }
#else	/* SNACC_EXCEPTION_ENABLE */
#define PDU_MEMBER_MACROS\
    int BEncPdu (BUF_TYPE b, AsnLen &bytesEncoded)\
    {\
	bytesEncoded = BEnc (b);\
	return !b.WriteError();\
    }\
\
    int BDecPdu (BUF_TYPE b, AsnLen &bytesDecoded)\
    {\
	ENV_TYPE env;\
	int val;\
\
	bytesDecoded = 0;\
	if ((val = setjmp (env)) == 0)\
	{\
	    BDec (b, bytesDecoded, env);\
	    return !b.ReadError();\
	}\
	else\
	    return false;\
    }
#endif	/* SNACC_EXCEPTION_ENABLE */
#else	/* !SNACC_ENABLE_PDU */
/* empty */
#define PDU_MEMBER_MACROS
#endif

/*
 * SNACC exception class
 */
#if		SNACC_EXCEPTION_ENABLE

#include <exception>

class SnaccExcep : public std::exception {
protected:
    SnaccExcep(int err) : mErrNum(err) { }
public:
    virtual ~SnaccExcep() throw() {};
	virtual int errNum() { return mErrNum; }
	static void throwMe(int err);		// implemented in cdsaUtils.cpp
protected:
	int mErrNum;
};
#endif	/* SNACC_EXCEPTION_ENABLE */

#endif /* conditional include */
