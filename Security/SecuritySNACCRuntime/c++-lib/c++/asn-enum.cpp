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


// file: .../c++-lib/src/asn-enum.C - methods for AsnEnum (ASN.1 ENUMERATED) class
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
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/c++/asn-enum.cpp,v 1.3 2002/03/21 05:38:44 dmitch Exp $
// $Log: asn-enum.cpp,v $
// Revision 1.3  2002/03/21 05:38:44  dmitch
// Radar 2868524: no more setjmp/longjmp in SNACC-generated code.
//
// Revision 1.2.44.1  2002/03/20 00:36:49  dmitch
// Radar 2868524: SNACC-generated code now uses throw/catch instead of setjmp/longjmp.
//
// Revision 1.2  2001/06/26 23:49:52  dmitch
// Was cerr, is Asn1Error.
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
// Revision 1.2  2000/06/08 20:05:35  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/03/21 02:07:35  mb
// Added Copy to every AsnType.
//
// Revision 1.1  1999/02/25 05:21:51  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/28 13:39:44  wan
// Modifications collected for new version 1.3: Bug fixes, tk4.2.
//
// Revision 1.4  1995/08/17 15:19:52  rj
// AsnEnumTypeDesc gets its own TclGetVal and TclSetVal functions.
//
// Revision 1.3  1995/07/24  20:14:49  rj
// Clone() added, or else the _desc would be wrong (and the wrong BEnc etc... would get called) for Clone-d objects.
//
// call constructor with additional pdu and create arguments.
//
// changed `_' to `-' in file names.
//
// Revision 1.2  1994/10/08  05:26:37  rj
// comment leader fixed.
//
// Revision 1.1  1994/10/08  05:24:03  rj
// functions extracted from ../inc/asn_enum.h

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-int.h"
#include "asn-enum.h"

AsnType *AsnEnum::Clone() const
{
  Asn1Error << "AsnEnum::Clone() called" << endl;
  abort();
  return NULL;
}

AsnType *AsnEnum::Copy() const
{
  Asn1Error << "AsnEnum::Copy() called" << endl;
  abort();
  return NULL;
}

AsnLen AsnEnum::BEnc (BUF_TYPE b)
{
    AsnLen l;
    l = BEncContent (b);
    BEncDefLenTo127 (b, l);
    l++;
    l += BEncTag1 (b, UNIV, PRIM, ENUM_TAG_CODE);
    return l;
}

void AsnEnum::BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnLen elmtLen;
    if (BDecTag (b, bytesDecoded, env) != MAKE_TAG_ID (UNIV, PRIM, ENUM_TAG_CODE))
    {
	Asn1Error << "AsnEnum::BDec: ERROR tag on ENUMERATED is wrong." << endl;
	#if SNACC_EXCEPTION_ENABLE
	SnaccExcep::throwMe(-52);
	#else
	longjmp (env,-52);
	#endif
    }

    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecContent (b, MAKE_TAG_ID (UNIV, PRIM, ENUM_TAG_CODE), elmtLen, bytesDecoded, env);
}

#if META

const AsnEnumTypeDesc AsnEnum::_desc (NULL, NULL, false, AsnTypeDesc::ENUMERATED, NULL, NULL);

const AsnTypeDesc *AsnEnum::_getdesc() const
{
  return &_desc;
}

#if TCL

int AsnEnum::TclGetVal (Tcl_Interp *interp) const
{
  const AsnNameDesc *n = _getdesc()->getnames();
  if (n)
  {
    for (; n->name; n++)
      if (n->value == value)
      {
	Tcl_SetResult (interp, (char*)n->name, TCL_STATIC);
	return TCL_OK;
      }
  }
  char valstr[80];
  sprintf (valstr, "%d", value);
  Tcl_AppendResult (interp, "illegal numeric enumeration value ", valstr, " for type ", _getdesc()->getmodule()->name, ".", _getdesc()->getname(), NULL);
  Tcl_SetErrorCode (interp, "SNACC", "ILLENUM", NULL);
  return TCL_ERROR;
}

int AsnEnum::TclSetVal (Tcl_Interp *interp, const char *valstr)
{
  const AsnNameDesc *n = _getdesc()->getnames();
  if (n)
  {
    for (; n->name; n++)
      if (!strcmp (n->name, valstr))
      {
	value = n->value;
	return TCL_OK;
      }
  }
  Tcl_SetErrorCode (interp, "SNACC", "ILLENUM", NULL);
  Tcl_AppendResult (interp, "illegal symbolic enumeration value \"", valstr, "\" for type ", _getdesc()->getmodule()->name, ".", _getdesc()->getname(), NULL);
  return TCL_ERROR;
}

#endif /* TCL */
#endif /* META */
