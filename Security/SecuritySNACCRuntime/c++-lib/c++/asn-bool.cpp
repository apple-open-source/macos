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


// file: .../c++-lib/src/asn-bool.C - methods for AsnBool (ASN.1 BOOLEAN) class
//
// MS 92/06/16
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
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/asn-bool.cpp,v 1.3 2002/03/21 05:38:44 dmitch Exp $
// $Log: asn-bool.cpp,v $
// Revision 1.3  2002/03/21 05:38:44  dmitch
// Radar 2868524: no more setjmp/longjmp in SNACC-generated code.
//
// Revision 1.2.44.1  2002/03/20 00:36:49  dmitch
// Radar 2868524: SNACC-generated code now uses throw/catch instead of setjmp/longjmp.
//
// Revision 1.2  2001/06/27 23:09:14  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.1.1.1  2001/05/18 23:14:05  mb
// Move from private repository to open source repository
//
// Revision 1.2  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.1  2000/06/15 18:44:57  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:34  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/03/21 02:07:35  mb
// Added Copy to every AsnType.
//
// Revision 1.1  1999/02/25 05:21:50  mb
// Added snacc c++ library
//
// Revision 1.7  1997/02/28 13:39:44  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.6  1997/02/16 20:26:03  rj
// check-in of a few cosmetic changes
//
// Revision 1.5  1995/07/24  20:10:43  rj
// call constructor with additional pdu and create arguments.
//
// #if TCL ... #endif wrapped into #if META ... #endif
//
// changed `_' to `-' in file names.
//
// Revision 1.4  1994/10/08  04:18:22  rj
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
// Revision 1.3  1994/08/31  23:30:48  rj
// use the bool built-in where applicable, and a replacement type otherwise.
//
// Revision 1.2  1994/08/28  10:01:11  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:58  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-bool.h"

AsnType *AsnBool::Clone() const
{
  return new AsnBool;
}

AsnType *AsnBool::Copy() const
{
  return new AsnBool (*this);
}

AsnLen AsnBool::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l = BEncContent (b);
    BEncDefLenTo127 (b, l);
    l++;
    l += BEncTag1 (b, UNIV, PRIM, BOOLEAN_TAG_CODE);
    return l;
}

void AsnBool::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    if (BDecTag (b, bytesDecoded, env) != MAKE_TAG_ID (UNIV, PRIM, BOOLEAN_TAG_CODE))
    {
	Asn1Error << "AsnBool::BDec: ERROR tag on BOOLEAN wrong." << endl;
	#if SNACC_EXCEPTION_ENABLE
	SnaccExcep::throwMe(-51);
	#else
	longjmp (env, -51);
	#endif
    }
    elmtLen = BDecLen (b, bytesDecoded, env);

    BDecContent (b, MAKE_TAG_ID (UNIV, PRIM, BOOLEAN_TAG_CODE), elmtLen, bytesDecoded, env);
}

// Decodes the content of a BOOLEAN and sets this object's value
// to the decoded value. Flags an error if the length is wrong
// or a read error occurs.
void AsnBool::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
    if (elmtLen != 1)
    {
        Asn1Error << "AsnBool::BDecContent: ERROR - boolean value too long." << endl;
		#if SNACC_EXCEPTION_ENABLE
		SnaccExcep::throwMe(-5);
		#else
        longjmp (env, -5);
		#endif
    }

    value = (b.GetByte() != 0);
    bytesDecoded++;

    if (b.ReadError())
    {
        Asn1Error << "AsnBool::BDecContent: ERROR - decoded past end of data " << endl;
		#if SNACC_EXCEPTION_ENABLE
		SnaccExcep::throwMe(-6);
		#else
        longjmp (env, -6);
		#endif
    }
}

AsnLen AsnBool::BEncContent (BUF_TYPE b)
{
    b.PutByteRvs (value ? 0xFF : 0);
    return 1;
}

// print the BOOLEAN's value in ASN.1 value notation to the given ostream
void AsnBool::Print (ostream &os) const
{
#ifndef	NDEBUG
  os << (value ? "TRUE" : "FALSE");
#endif
}

#if META

const AsnTypeDesc AsnBool::_desc (NULL, NULL, false, AsnTypeDesc::BOOLEAN, NULL);

const AsnTypeDesc *AsnBool::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnBool::TclGetVal (Tcl_Interp *interp) const
{
  Tcl_SetResult (interp, value ? "TRUE" : "FALSE", TCL_STATIC);
  return TCL_OK;
}

int AsnBool::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  int valval;

  if (Tcl_GetBoolean (interp, (char*) valstr, &valval) != TCL_OK)
    return TCL_ERROR;

  value = valval;

  return TCL_OK;
}

#endif /* TCL */
#endif /* META */
