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


// file: .../c++-lib/inc/asn-any.h - C++ class for any type
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
// ../SCCS/s.asn-any.h:
// 
// D 1.2	98/05/01 13:46:36 pleonber	2 1	00008/00000/00099
// added destructor and copy for CSM_Buffer handling (cleans up memory).
// 
// D 1.1	98/05/01 13:16:05 pleonber	1 0	00099/00000/00000
// date and time created 98/05/01 13:16:05 by pleonber
// 
// ----------------------- End of VDA Modifications ---------------------------
// 
// 
// 
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/inc/asn-any.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: asn-any.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.5  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.4  2000/12/22 20:33:26  mb
// New Security framework fase 1 complete.
//
// Revision 1.3  2000/12/07 22:14:38  dmitch
// Thread-safe mods: made oidHashTbl and intHashTbl private.
//
// Revision 1.2  2000/06/15 18:48:25  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.4  1999/03/21 02:07:31  mb
// Added Copy to every AsnType.
//
// Revision 1.3  1999/03/18 22:35:26  mb
// Made all destructors virtual.
//
// Revision 1.2  1999/02/26 00:32:55  mb
// Fix bug when not building with VDADER_RULES defined.
//
// Revision 1.1  1999/02/25 05:21:40  mb
// Added snacc c++ library
//
// Revision 1.4  1997/01/02 08:39:42  rj
// missing prototype added
//
// Revision 1.3  1994/10/08  04:17:56  rj
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
// Revision 1.2  1994/08/28  10:00:43  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:24  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_any_h_
#define _asn_any_h_

#ifdef _IBM_ENC_
#include "shmmgr.h"   // Guido Grassel 4.8.93
#endif /* _IBM_ENC_ */

#include "hash.h"

#ifdef	__APPLE__
#include <Security/threading.h>	/* for Mutex */
#endif

/* this is put into the hash table with the int or oid as the key */
#ifndef _IBM_ENC_
class AnyInfo
#else
class AnyInfo: public MemMgr   // Guido Grassel 4.8.93
#endif /* _IBM_ENC_ */
{
public:
  int				anyId;	// will be a value from the AnyId enum
  AsnOid			oid;	// will be zero len/null if intId is valid
  AsnInt			intId;
  AsnType			*typeToClone;
};

#if	defined(macintosh) || defined(__APPLE__)
class CSM_Buffer;
#endif

class AsnAny: public AsnType
{
#ifdef	__APPLE__
/* need a lock to protect these, declared as a static in the .cpp file.
 *...plus, I have no idea why these
 * were declared public. They are not used anywhere else.
 */
private:
  static Table			*oidHashTbl;	// all AsnAny class instances
  static Table			*intHashTbl;	// share these tables
public:
#else
public:
  static Table			*oidHashTbl;	// all AsnAny class instances
  static Table			*intHashTbl;	// share these tables
#endif
  AnyInfo			*ai;		// points to entry in hash tbl for this type
#if	defined(macintosh) || defined(__APPLE__)
// FIXME - needs work
  CSM_Buffer		*value;
#else
  AsnType			*value;
#endif
				AsnAny()				{ ai = NULL; value = NULL; }

  // class level methods
  static void			InstallAnyByInt (AsnInt intId, int anyId, AsnType *type);
  static void			InstallAnyByOid (AsnOid &oid,  int anyId, AsnType *type);

  int				GetId()	const				{ return ai ? ai->anyId : -1; }
  void				SetTypeByInt (AsnInt id);
  void				SetTypeByOid (AsnOid &id);

  virtual AsnType	*Clone() const;
  virtual AsnType	*Copy() const;

  AsnLen			BEnc (BUF_TYPE b);
  void				BDec (BUF_TYPE b, AsnLen &bytesDecoded, ENV_TYPE env);

  PDU_MEMBER_MACROS

  void				Print (ostream &) const;

#if 0
#if TCL
  int				TclGetDesc (Tcl_DString *) const;
  int				TclGetVal (Tcl_DString *) const;
  int				TclSetVal (Tcl_Interp *, const char *val);
  int				TclUnSetVal (Tcl_Interp *, const char *member);
#endif /* TCL */
#endif

#ifdef VDADER_RULES
  virtual		~AsnAny();
				AsnAny &operator = (const AsnAny &o);  
};

// AnyDefinedBy is currently the same as AsnAny:
typedef AsnAny			AsnAnyDefinedBy;

#else
};
#endif /* _conditional_include_ */

#endif /* _asn_any_h_ */
