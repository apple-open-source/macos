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


// file: .../c++-lib/inc/asn-incl.h - includes all of the asn1 library files
//
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
// ../SCCS/s.asn-incl.h:
// 
// D 1.2	98/05/01 13:14:40 pleonber	2 1	00006/00000/00059
// added #include for sm_vdasnacc.h
// 
// D 1.1	98/05/01 13:13:30 pleonber	1 0	00059/00000/00000
// date and time created 98/05/01 13:13:30 by pleonber
// 
// ----------------------- End of VDA Modifications ---------------------------
// 
// 
// 
// $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/inc/Attic/asn-incl.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: asn-incl.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.4  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.3  2001/01/10 01:12:03  dmitch
// Rearranged #includes so sm_vdasnacc.h always sees asn-buf.h.
//
// Revision 1.2  2000/06/15 18:48:23  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.1  1999/02/25 05:21:42  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:25:37  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  17:52:33  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:01  rj
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
// Revision 1.2  1994/08/28  10:00:49  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:33  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifdef _IBM_ENC_
#define ChoiceUnion
#endif /* _IBM_ENC_ */


#include "asn-config.h"
#include "asn-buf.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-type.h"
#include "asn-int.h"
#include "asn-bool.h"
#include "asn-real.h"
#include "asn-oid.h"
#include "asn-octs.h"
#include "asn-bits.h"
#include "asn-enum.h"
#include "asn-null.h"
#ifdef VDADER_RULES
#include "sm_vdasnacc.h"
#endif
#include "asn-any.h"
#include "asn-useful.h"
#include "print.h"
