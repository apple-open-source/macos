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


// file: .../c++-lib/src/asn-null.C
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
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/c++/asn-null.cpp,v 1.3 2002/03/21 05:38:44 dmitch Exp $
// $Log: asn-null.cpp,v $
// Revision 1.3  2002/03/21 05:38:44  dmitch
// Radar 2868524: no more setjmp/longjmp in SNACC-generated code.
//
// Revision 1.2.44.1  2002/03/20 00:36:49  dmitch
// Radar 2868524: SNACC-generated code now uses throw/catch instead of setjmp/longjmp.
//
// Revision 1.2  2001/06/27 23:09:14  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.2  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.1  2000/06/15 18:44:57  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:35  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/03/21 02:07:36  mb
// Added Copy to every AsnType.
//
// Revision 1.1  1999/02/25 05:21:52  mb
// Added snacc c++ library
//
// Revision 1.5  1995/08/17 15:38:19  rj
// set Tcl's errorCode variable
//
// Revision 1.4  1995/07/24  20:18:27  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:26  rj
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
// Revision 1.2  1994/08/28  10:01:15  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:04  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-null.h"

AsnType *AsnNull::Clone() const
{
  return new AsnNull;
}

AsnType *AsnNull::Copy() const
{
  return new AsnNull (*this);
}

void AsnNull::BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env)
{
    if (elmtLen != 0)
    {
        Asn1Error << "AsnNull::BDecContent: ERROR - NULL values len is non-zero" << endl;
 		#if SNACC_EXCEPTION_ENABLE
		SnaccExcep::throwMe(-13);
		#else
        longjmp (env, -13);
	    #endif
    }
} /* AsnNull::BDecContent */

AsnLen AsnNull::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l = BEncContent (b);
    BEncDefLenTo127 (b, l);
    l++;
    l += BEncTag1 (b, UNIV, PRIM, NULLTYPE_TAG_CODE);
    return l;
}

void AsnNull::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    if (BDecTag (b, bytesDecoded, env) != MAKE_TAG_ID (UNIV, PRIM, NULLTYPE_TAG_CODE))
    {
	Asn1Error << "AsnNull::BDec: ERROR tag on NULL is wrong." << endl;
	#if SNACC_EXCEPTION_ENABLE
	SnaccExcep::throwMe(-55);
	#else
	longjmp (env, -55);
	#endif
    }

    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecContent (b, MAKE_TAG_ID (UNIV, PRIM, NULLTYPE_TAG_CODE), elmtLen, bytesDecoded, env);
}

void AsnNull::Print (ostream &os) const
{
#ifndef	NDEBUG
    os << "NULL";
#endif
}

#if META

const AsnNullTypeDesc AsnNull::_desc (NULL, NULL, false, AsnTypeDesc::NUL_, NULL);

const AsnTypeDesc *AsnNull::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnNull::TclGetVal (Tcl_Interp *interp) const
{
  return TCL_OK;
}

int AsnNull::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  if (*valstr)
  {
    Tcl_AppendResult (interp, "illegal non-null value `", valstr, "' for type ", _getdesc()->getmodule()->name, ".", _getdesc()->getname(), NULL);
    Tcl_SetErrorCode (interp, "SNACC", "ILLNULL", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

#endif /* TCL */
#endif /* META */
