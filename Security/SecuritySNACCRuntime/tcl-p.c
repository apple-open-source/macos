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


/*
 * file: tcl-p.c
 * purpose: check and return via exit code whether the tcl interface needs to be made
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/tcl-p.c,v 1.1.1.1 2001/05/18 23:14:05 mb Exp $
 * $Log: tcl-p.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:05  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:16  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:05:50  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.1  1995/07/25 22:24:48  rj
 * new file
 *
 */

#define COMPILER	1

#include "snacc.h"

main()
{
#if TCL
  return 0;
#else
  return 1;
#endif
}
