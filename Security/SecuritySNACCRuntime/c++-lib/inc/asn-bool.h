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


// file: .../c++-lib/inc/asn-bool.h  - c++ version of ASN.1 integer
//
// MS 92/06/15
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
// ../SCCS/s.asn-bool.h:
// 
// D 1.2	98/04/24 22:40:40 pleonber	2 1	00002/00000/00118
// added INSERT_VDA_COMMENTS for script that adds SCCS history
// 
// D 1.1	97/11/11 15:48:58 cmmaster	1 0	00118/00000/00000
// date and time created 97/11/11 15:48:58 by cmmaster
// 
// ----------------------- End of VDA Modifications ---------------------------
// 
// 
// 
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/inc/asn-bool.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: asn-bool.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/15 18:48:23  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/03/21 02:07:32  mb
// Added Copy to every AsnType.
//
// Revision 1.1  1999/02/25 05:21:41  mb
// Added snacc c++ library
//
// Revision 1.8  1995/09/07 18:45:13  rj
// use AsnBoolTypeDesc instead of AsnTypeDesc (no real difference, it is the same type).
//
// Revision 1.7  1995/07/24  17:53:54  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// changed `_' to `-' in file names.
//
// Revision 1.6  1995/02/18  19:17:19  rj
// add TRUE/FALSE for backwards compatibility.
//
// Revision 1.5  1995/02/18  12:41:31  rj
// a few more lines for the sake of backwards compatibility.
//
// Revision 1.4  1994/10/08  04:17:58  rj
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
// Revision 1.3  1994/08/31  23:32:13  rj
// use the bool built-in where applicable, and a replacement type otherwise.
//
// Revision 1.2  1994/08/28  10:00:45  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:27  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_bool_h_
#define _asn_bool_h_

#if GLASS
// for backwards compatibility:
#ifndef FALSE
enum { FALSE = false, TRUE = true };
#endif
#endif // GLASS

class AsnBool: public AsnType
{
protected:

// for backwards compatibility:
#if GLASS
#if BOOL_BUILTIN
  typedef bool			_bool;
#else
  enum
  {
    false = ::false,
    true = ::true
  };
#endif
#endif

  bool				value;

public:
				AsnBool (const bool val):
#if BOOL_BUILTIN
				  value (val)
#else
				  value (!!val)
#endif
				{}
				AsnBool()				{}

  virtual AsnType	*Clone() const;
  virtual AsnType	*Copy() const;

				operator bool() const			{ return value; }
  AsnBool			&operator = (bool newvalue)		{ value = newvalue; return *this; }

  AsnLen			BEnc (BUF_TYPE b);
  void				BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);

  AsnLen			BEncContent (BUF_TYPE b);
  void				BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS

  void				Print (ostream &) const;

#if META
  static const AsnBoolTypeDesc	_desc;

  const AsnTypeDesc		*_getdesc() const;

#if TCL
  int				TclGetVal (Tcl_Interp *) const;
  int				TclSetVal (Tcl_Interp *, const char *val);
#endif // TCL
#endif // META
};

#endif // conditional include
