/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * mslp_sd.h : System dependent definitions, a prerequisite to all 
 *         other headers and files.
 *
 * Version: 1.4 
 * Date:    10/05/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#define MSLP_CLIENT 1  /* used by mslplib to initialize SD resources */
#define MSLP_SERVER 2  /* used by mslpd to initialize SD resources */
/* 
 * System dependencies require a definition for the following SD functions
 * which are macro defined to system dependent implementations:
 * 
 * EXPORT            calling convention to insure Windows DLLs created properly
 * SOCKET            defined to the correct data type for the sockets interface
 * SOCKETERR         value returned from socket calls indicating failure
 * OPEN_NETWORKING   called to initialize networking library (Windows)
 * CLOSE_NETWORKING  called to free networking library (Windows)
 * CLOSESOCKET       called to close sockets
 * gethostname       assumed - for Solaris prior to 2.6, mslp includes it
 *
 * SDGetTime         system specific time function, returns long (num seconds)
 *
 * - used only if EXTRA_MSGS are defined for coordinating IPC -
 *
 * SDLock
 * SDUnlock
 * SDGetMutex
 * SDFreeMutex
 * SDDefaultRegfile
 * SDDefaultTempfile
 *
 */
 
#ifdef WIN32 

  #include "compat/w32/mslp_w32.h"

#elif defined(SOLARIS) 

  #include "compat/solaris/mslp_sol.h"

#elif defined(LINUX) 

  #include "mslp_linux.h"

#elif defined (MAC_OS_X)

  #include "mslp_macosx.h"

#else

  #error You MUST define LINUX, SOLARIS or WIN32.

#endif
