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


// file: .../c++-lib/inc/asn-bits.h - ASN.1 BIT STRING type
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
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/inc/asn-bits.h,v 1.2 2001/06/28 23:36:11 dmitch Exp $
// $Log: asn-bits.h,v $
// Revision 1.2  2001/06/28 23:36:11  dmitch
// Removed SccsId statics. numToHexCharTblG table now const. Radar 2705410.
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.5  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.4  2000/12/07 22:29:50  dmitch
// Thread-safe mods: added strStkG, strStkUnusedBitsG arguments to FillBitStringStk .
//
// Revision 1.3  2000/08/24 20:00:25  dmitch
// Added BitOcts() accessor.
//
// Revision 1.2  2000/06/15 18:48:22  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// 2000/8/24 dmitch at Apple
// Added BitOcts() accessor.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.3  1999/03/21 02:07:31  mb
// Added Copy to every AsnType.
//
// Revision 1.2  1999/03/18 22:35:27  mb
// Made all destructors virtual.
//
// Revision 1.1  1999/02/25 05:21:40  mb
// Added snacc c++ library
//
// Revision 1.6  1997/02/16 20:25:33  rj
// check-in of a few cosmetic changes
//
// Revision 1.5  1995/07/24  17:53:51  rj
// #if TCL ... #endif wrapped into #if META ... #endif
//
// changed `_' to `-' in file names.
//
// Revision 1.4  1995/02/18  19:26:18  rj
// remove const from arguments that are passed by value.
//
// Revision 1.3  1994/10/08  04:17:57  rj
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
// Revision 1.2  1994/08/28  10:00:44  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:25  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_bits_h_
#define _asn_bits_h_

#ifdef	__APPLE__
#include "str-stk.h"
#endif

extern const char	numToHexCharTblG[];

#define TO_HEX( fourBits)	(numToHexCharTblG[(fourBits) & 0x0F])

class AsnBits: public AsnType
{
private:
  bool				BitsEquiv (const AsnBits &ab) const;
  void				BDecConsBits (BUF_TYPE b, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);
  #ifdef	__APPLE__
  void				FillBitStringStk (BUF_TYPE b, AsnLen elmtLen0, 
			AsnLen &bytesDecoded, ENV_TYPE env,
			StrStk &strStkG, unsigned short int  &strStkUnusedBitsG);
  #else
  void				FillBitStringStk (BUF_TYPE b, AsnLen elmtLen0, 
			AsnLen &bytesDecoded, ENV_TYPE env);
  #endif

protected:
  size_t			bitLen;
  char				*bits;

public:

				AsnBits()				{ bits = NULL; bitLen = 0; }
				AsnBits (size_t numBits)		{ Set (numBits); }
				AsnBits (const char *bitOcts, size_t numBits)
									{ Set (bitOcts, numBits); }
				AsnBits (const AsnBits &b)		{ Set (b); }
#ifndef _IBM_ENC_
  virtual		~AsnBits();
#else
  virtual		~AsnBits()				{ mem_mgr_ptr->Put ((void *) bits); } // Guido Grassel, 11.8.93
#endif /* _IBM_ENC_ */

  virtual AsnType	*Clone() const;
  virtual AsnType	*Copy() const;

  AsnBits			&operator = (const AsnBits &b)		{ ReSet (b); return *this; }

  // overwrite existing bits and bitLen values
  void				Set (size_t numBits);
  void				Set (const char *bitOcts, size_t numBits);
  void				Set (const AsnBits &b);

  // free old bits value, the reset bits and bitLen values
  void				ReSet (size_t numBits);
  void				ReSet (const char *bitOcts, size_t numBits);
  void				ReSet (const AsnBits &b);

  bool				operator == (const AsnBits &ab) const	{ return BitsEquiv (ab); }
  bool				operator != (const AsnBits &ab) const	{ return !BitsEquiv (ab); }

  void				SetBit (size_t);
  void				ClrBit (size_t);
  bool				GetBit (size_t) const;

  // Apple addenda: this is just too useful to exclude. 
  const char		*BitOcts() const { return bits; }

  size_t			BitLen() const				{ return bitLen; }

  AsnLen			BEncContent (BUF_TYPE b);
  void				BDecContent (BUF_TYPE b, AsnTag tagId, AsnLen elmtLen, AsnLen &bytesDecoded, ENV_TYPE env);
  AsnLen			BEnc (BUF_TYPE b);
  void				BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS

  void				Print (ostream &) const;

#if META
  static const AsnBitsTypeDesc	_desc;

  const AsnTypeDesc		*_getdesc() const;

#if TCL
  int				TclGetVal (Tcl_Interp *) const;
  int				TclSetVal (Tcl_Interp *, const char *val);
#endif /* TCL */
#endif /* META */
};

#endif /* conditional include */
