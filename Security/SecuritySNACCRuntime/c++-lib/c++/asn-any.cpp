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


// file: .../c++-lib/src/asn-any.C
//
// MS 92
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
//
// ------------------------------------------------------------------------
// -  J.G. Van Dyke & Associates, Inc. Modification History of SNACC 1.3  -
// ------------------------------------------------------------------------
// 
// All modification are relative to the v1.3 of SNACC.  We used SunOS 4.1.3's 
// SCCS.  The revision #'s start at 1.1,  which is the original version from 
// SNACC 1.3.
// 
// 
// ../SCCS/s.asn-any.C:
// 
// D 1.2	98/05/01 13:47:09 pleonber	2 1	00046/00007/00164
// added destructor and copy for CSM_Buffer handling.
// 
// D 1.1	98/05/01 13:19:19 pleonber	1 0	00171/00000/00000
// date and time created 98/05/01 13:19:19 by pleonber
// 
// ----------------------- End of VDA Modifications ---------------------------
// 
// 
// 
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/c++/asn-any.cpp,v 1.4 2002/03/21 05:38:44 dmitch Exp $
// $Log: asn-any.cpp,v $
// Revision 1.4  2002/03/21 05:38:44  dmitch
// Radar 2868524: no more setjmp/longjmp in SNACC-generated code.
//
// Revision 1.3.44.1  2002/03/20 00:36:48  dmitch
// Radar 2868524: SNACC-generated code now uses throw/catch instead of setjmp/longjmp.
//
// Revision 1.3  2001/06/27 23:09:14  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.2  2001/06/25 22:44:17  dmitch
// Globalize hashTblLock with a ModuleNexus. Partial fix for Radar 2664258.
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.6  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.5  2000/12/20 00:51:37  dmitch
// Cosmetic changwe to resync with ../c++/asn-any.cpp.
//
// Revision 1.4  2000/12/20 00:43:14  dmitch
// Acquire and release hashTblLock via an StLock.
//
// Revision 1.3  2000/12/07 22:32:03  dmitch
// Thread-safe mods:  see comments for same file in ../c++/.
//
// Revision 1.2  2000/12/07 22:13:45  dmitch
// Thread-safe mods: added hashTblLock.
//
// Revision 1.1  2000/06/15 18:44:59  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:37  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.5  1999/03/21 02:07:35  mb
// Added Copy to every AsnType.
//
// Revision 1.4  1999/03/19 23:59:21  mb
// Invoke Print on our value since CSM_Buffer::Print now implements print too.
//
// Revision 1.3  1999/03/19 00:55:01  mb
// Made CSM_Buffer a subclass of AsnType.
//
// Revision 1.2  1999/03/18 22:35:28  mb
// Made all destructors virtual.
//
// Revision 1.1  1999/02/25 05:21:49  mb
// Added snacc c++ library
//
// Revision 1.6  1997/02/28 13:39:43  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.5  1997/02/16 20:26:01  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  20:12:48  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:20  rj
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
// Revision 1.2  1994/08/28  10:01:10  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:55  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-incl.h"
#include "sm_vdasnacc.h"

//#include "asn-config.h"
//#include "asn-len.h"
//#include "asn-tag.h"
//#include "asn-type.h"
//#include "asn-oid.h"
//#include "asn-int.h"
//#include "asn-any.h"


#ifdef	__APPLE__
#include <Security/threading.h>
#include <Security/globalizer.h>
Table *AsnAny::oidHashTbl = NULL;
Table *AsnAny::intHashTbl = NULL;
ModuleNexus<Mutex> hashTblLock;
#endif

// Define this ANY value's type to the one that the given id hashes
// to in the ANY table.
void
AsnAny::SetTypeByInt (AsnInt id)
{
    Hash hash;
    void *anyInfo;

    /* use int as hash string */
    AsnIntType idval = (AsnIntType) id;
    hash = MakeHash ((char*)&idval, sizeof (idval));
	#ifdef	__APPLE__
	StLock<Mutex> _(hashTblLock());
	#endif
	if (CheckForAndReturnValue (intHashTbl, hash, &anyInfo))
		ai = (AnyInfo*) anyInfo;
	else
		ai = NULL; /* indicates failure */

} /* SetAnyTypeByInt */

// Define this ANY value's type to the one that the given id hashes
// to in the ANY table.
void AsnAny::SetTypeByOid (AsnOid &id)
{
    Hash hash;
    void *anyInfo;

    /* use encoded oid as hash string */
    hash = MakeHash (id.Str(), id.Len());

	#ifdef	__APPLE__
	StLock<Mutex> _(hashTblLock());
	#endif

	if (CheckForAndReturnValue (oidHashTbl, hash, &anyInfo))
		ai = (AnyInfo*) anyInfo;
	else
		ai = NULL; /* indicates failure */

} /* SetAnyTypeByOid */



// Given an integer, intId, to hash on, the type and it's anyId
// are installed in the integer id hash tbl
void
AsnAny::InstallAnyByInt (AsnInt intId, int anyId, AsnType *type)
{
    AnyInfo *a;
    Hash h;

    a = new AnyInfo;
    //  Oid will be NULL and 0 len by default constructor
    a->anyId = anyId;
    a->intId = intId;
    a->typeToClone = type;

	#ifdef	__APPLE__
	StLock<Mutex> _(hashTblLock());
	#endif
	if (AsnAny::intHashTbl == NULL)
		AsnAny::intHashTbl = InitHash();

	AsnIntType idval = (AsnIntType) intId;
	h = MakeHash ((char*)&idval, sizeof (idval));
	Insert (AsnAny::intHashTbl, a, h);

}  /* InstallAnyByInt */


// given an OBJECT IDENTIFIER, oid, to hash on, the type and it's anyId
// are installed in the OBJECT IDENTIFIER id hash tbl
void
AsnAny::InstallAnyByOid (AsnOid &oid, int anyId, AsnType *type)
{
    AnyInfo *a;
    Hash h;

    a =  new AnyInfo;
    a->anyId = anyId;
    a->oid = oid;  // copy given oid
    a->typeToClone = type;

    h = MakeHash (oid.Str(), oid.Len());

	#ifdef	__APPLE__
	StLock<Mutex> _(hashTblLock());
	#endif
    if (AsnAny::oidHashTbl == NULL)
        AsnAny::oidHashTbl = InitHash();

    Insert (AsnAny::oidHashTbl, a, h);
}  /* InstallAnyByOid */


AsnType *AsnAny::Clone() const
{
  return new AsnAny;
}

AsnType *AsnAny::Copy() const
{
  return new AsnAny (*this);
}


//
// if you haven't set up the value properly
// this will croak (since it's a programming error
// - ie, you didn't initialize the data structure properly
//
AsnLen
AsnAny::BEnc (BUF_TYPE b)
{
    return value->BEnc (b);
}



void
AsnAny::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    if (ai == NULL)
		#if SNACC_EXCEPTION_ENABLE
		SnaccExcep::throwMe(-81);
		#else
        longjmp (env, -81);
		#endif
		
	// XXX This is wrong.
    value = static_cast<CSM_Buffer *>(ai->typeToClone->Clone());

    if (value == NULL)
		#if SNACC_EXCEPTION_ENABLE
		SnaccExcep::throwMe(-82);
		#else
        longjmp (env, -82);
		#endif
    else
        value->BDec (b, bytesDecoded, env);
}


void AsnAny::Print (ostream &os) const
{
#ifndef	NDEBUG
   value->Print(os);
#endif
}

#ifdef VDADER_RULES

AsnAny::~AsnAny()
{
    delete this->value;
}

AsnAny &AsnAny::operator = (const AsnAny &o) 
{ 
    if (this->ai)      // take care of most copies.
        delete this->ai;
    this->ai = NULL;
    if (o.ai)
    {
      this->ai = new AnyInfo;
      *this->ai = *o.ai;
    }
	/* __APPLE__ - I don't think this needs a lock since it's
	 * not modifying the hash tables */
    if (o.intHashTbl)
    {
      this->intHashTbl = o.intHashTbl; // same pointer.
    }
    if (o.oidHashTbl)
    {
      this->oidHashTbl = o.oidHashTbl;
    }

    if (o.value)
      this->value = static_cast<CSM_Buffer *>(o.value->Copy());

    return *this; 
}

#endif

