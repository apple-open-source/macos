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


// file: .../c++-lib/inc/asn-real.h - ASN.1 REAL type
//
//  Mike Sample
//  92/07/02
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
// $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/inc/Attic/asn-real.h,v 1.2 2001/06/21 21:57:00 dmitch Exp $
// $Log: asn-real.h,v $
// Revision 1.2  2001/06/21 21:57:00  dmitch
// Avoid global const PLUS_INFINITY, MINUS_INFINITY
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/15 18:48:24  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/03/21 02:07:33  mb
// Added Copy to every AsnType.
//
// Revision 1.1  1999/02/25 05:21:45  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:25:42  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  17:53:59  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:09  rj
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
// Revision 1.2  1994/08/28  10:00:55  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:41  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_real_h_
#define _asn_real_h_

class AsnReal: public AsnType
{
protected:
  double			value;

public:
				AsnReal():
				  value (0.0)
				{}
				AsnReal (double val):
				  value (val)
				{}

  virtual AsnType	*Clone() const;
  virtual AsnType	*Copy() const;

				operator double() const			{ return value; }
  AsnReal			&operator = (double newvalue)		{ value = newvalue; return *this; }

  AsnLen			BEncContent (BUF_TYPE b);
  void				BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen			BEnc (BUF_TYPE b);
  void				BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);

  void				Print (ostream &os) const;

  PDU_MEMBER_MACROS

#if META
  static const AsnRealTypeDesc	_desc;

  const AsnTypeDesc		*_getdesc() const;

#if TCL
  int				TclGetVal (Tcl_Interp *) const;
  int				TclSetVal (Tcl_Interp *, const char *val);
#endif /* TCL */
#endif /* META */
};

extern double AsnPlusInfinity();
extern double AsnMinusInfinity();

#define PLUS_INFINITY 	AsnReal(AsnPlusInfinity())
#define MINUS_INFINITY	AsnReal(AsnMinusInfinity())

#endif /* conditional include */
