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
 * file: .../c++-lib/inc/init.h
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-lib/inc/Attic/init.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
 * $Log: init.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:06  mb
 * Move from private repository to open source repository
 *
 * Revision 1.3  2001/05/05 00:59:18  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.2  2000/06/15 18:48:25  dmitch
 * Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
 *
 * Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
 * Base Fortissimo Tree
 *
 * Revision 1.1  1999/02/25 05:21:47  mb
 * Added snacc c++ library
 *
 * Revision 1.1  1995/07/27 09:22:35  rj
 * new file: .h file containing a declaration for a function defined in a C++ file, but with C linkage.
 *
 */

extern
#ifdef __cplusplus
	"C"
#endif
	     int Snacc_Init (Tcl_Interp *interp);
