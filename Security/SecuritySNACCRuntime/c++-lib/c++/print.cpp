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


// file: .../c++-lib/src/print.C
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
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/c++/print.cpp,v 1.4 2001/06/28 22:49:58 mb Exp $
// $Log: print.cpp,v $
// Revision 1.4  2001/06/28 22:49:58  mb
// Saved 4 bytes of data when compiling with -DNDEBUG
//
// Revision 1.3  2001/06/27 23:57:50  dmitch
// Reimplement partial fix for Radar 2664258: Print() routines are now empty stubs in NDEBUG config.
//
// Revision 1.2  2001/06/27 23:09:15  dmitch
// Pusuant to Radar 2664258, avoid all cerr-based output in NDEBUG configuration.
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.2  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.1  2000/06/15 18:44:58  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:36  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.1  1999/02/25 05:21:56  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:26:09  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  20:34:55  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:18:33  rj
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
// Revision 1.2  1994/08/28  10:01:22  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:12  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-incl.h"

#ifndef	NDEBUG
// the generated operator << routines for aggregate types use these globals
unsigned short int indentG = 0;
unsigned short int stdIndentG = 4;
#endif

void
Indent (ostream &os, unsigned short int i)
{
#ifndef	NDEBUG
  while (i-->0)
    os <<  ' ';
#endif	
}

ostream &operator << (ostream &os, const AsnType &v)
{
#ifndef	NDEBUG
  v.Print (os);
#endif	
  return os;
}
