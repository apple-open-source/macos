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


// file: .../c++-lib/inc/str-stk.h - maintains a stack of the components of a bit string or octet string so they can be copied into a single chunk
//
// MS 92/07/06
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
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/inc/str-stk.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: str-stk.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:18  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/15 18:48:25  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.2  1999/06/04 21:43:20  mb
// Fixed several memory leaks.
//
// Revision 1.1  1999/02/25 05:21:48  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:25:56  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/25  21:09:14  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:15:30  rj
// fixed both Copy()'s name and implementation to CopyOut() that always returns the number of bytes copied out instead of 0 in case less than the requested amount is available.
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.2  1994/08/28  10:01:01  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:49  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _str_stk_h_
#define _str_stk_h_

#ifndef _IBM_ENC_
class StrStk
#else
#include "shmmgr.h"   // Guido Grassel 4.8.93

class StrStk: public MemMgr     // Guido Grassel 12.8.93
#endif /* _IBM_ENC_ */
{
public:
  struct Elmt
  {
    char			*str;
    size_t			len;
  }				*stk;
  size_t			size;
  size_t			growSize;
  size_t			nextFreeElmt;
  size_t			totalByteLen;

				StrStk (int stkSize, int growIncrement);
				~StrStk ();

  void				Reset();

  void				Push (char *str, size_t strLen);

  // copy string pieces (buffer refs) into single block.
  // assumes that the buf is at least totalByteLen byte long.
  void				CopyOut (char *buf);

};

#endif /* conditional include */
