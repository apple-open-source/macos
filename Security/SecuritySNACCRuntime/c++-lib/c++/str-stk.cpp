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


// file: .../c++-lib/src/str-stk.C
//
// MS 92/07/06
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
// $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/c++/Attic/str-stk.cpp,v 1.2 2002/02/07 04:30:04 mb Exp $
// $Log: str-stk.cpp,v $
// Revision 1.2  2002/02/07 04:30:04  mb
// Fixes required to build with gcc3.
// Merged from branch PR-2848996
// Bug #: 2848996
// Submitted by:
// Reviewed by: Turly O'Connor <turly@apple.com>
//
// Revision 1.1.1.1.12.1  2002/02/06 23:45:03  mb
// Changes to allow building with gcc3
//
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/12/07 22:16:57  dmitch
// Thread-safe mods: removed global StrStk strStkG.
//
//
// 2000/12/7 dmitch
// #ifdef'd out strStkG for thread safety
//
// Revision 1.1  2000/06/15 18:44:58  dmitch
// These snacc-generated source files are now checked in to allow cross-platform build.
//
// Revision 1.2  2000/06/08 20:05:37  dmitch
// Mods for X port. These files are actually machine generated and probably don't need to be in CVS....
//
// Revision 1.1.1.1  2000/03/09 01:00:06  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/06/04 21:43:21  mb
// Fixed several memory leaks.
//
// Revision 1.1  1999/02/25 05:21:57  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:26:11  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/24  20:34:57  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:15:22  rj
// fixed both Copy()'s name and implementation to CopyOut() that always returns the number of bytes copied out instead of 0 in case less than the requested amount is available.
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.2  1994/08/28  10:01:24  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:21:13  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#include "asn-config.h"
#include "str-stk.h"

#ifndef	__APPLE__
/* clients each have their own for OS X */
// global for use by AsnBits and AsnOcts

StrStk strStkG (128, 64);
#endif	/* 0 */

StrStk::StrStk (int stkSize, int growIncrement)
{
  stk = new struct Elmt[stkSize];
  size = stkSize;
  growSize = growIncrement;
}

StrStk::~StrStk ()
{
  delete stk;
}

void StrStk::Reset()
{
  nextFreeElmt = 0;
  totalByteLen = 0;
}

void StrStk::Push (char *str, size_t strLen)
{
  if (nextFreeElmt >= size)
  {
    struct Elmt *tmpStk;
    // alloc bigger stack and copy old elmts to it
    tmpStk = new struct Elmt[size + growSize];
    for (size_t i = 0; i < size; i++)
      tmpStk[i] = stk[i];
    delete stk;
    stk = tmpStk;
    size += growSize;
  }
  totalByteLen += strLen;
  stk[nextFreeElmt].str = str;
  stk[nextFreeElmt++].len = strLen;
}

/*
 * copy string pieces (buffer refs) into single block.
 * assumes that the buf is at least totalByteLen byte long.
 */
void StrStk::CopyOut (char *buf)
{
  unsigned long int curr;
  char *bufCurr;

  bufCurr = buf;
  for (curr = 0; curr < nextFreeElmt; curr++)
  {
      memcpy (bufCurr, stk[curr].str, stk[curr].len);
      bufCurr += stk[curr].len;
  }
}
