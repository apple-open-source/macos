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
 * file: policy.h
 *
 */

/*
 * enable the snacc compiler's Tcl interface generating code?
 * set it to 0 or 1.
 */
#ifndef NO_TCL
#define NO_TCL		0
#endif

/*
 * enable code for meta code generation?
 * the Tcl code needs it.
 */
#ifndef NO_META
#define NO_META		NO_TCL
#endif

/*
 * enable code for CORBA IDL generation?
 */
#ifndef IDL
#define IDL		1
#endif
