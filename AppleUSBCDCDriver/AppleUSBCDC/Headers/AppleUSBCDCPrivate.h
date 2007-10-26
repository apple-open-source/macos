/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to Event LoG (via kprintf and Firewire) - LDEBUG must also be set
#define USE_IOL		0			// to IOLog - LDEBUG must also be set

#define Sleep_Time	20

#define Log IOLog
#if USE_ELG
	#undef Log
	#define Log	kprintf
#endif

#if LDEBUG
    #if USE_ELG
		#define XTRACE(ID,A,B,STRING) {Log("%8x %8x %8x %8x AppleUSBCDC: " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf());}
    #else /* not USE_ELG */
        #if USE_IOL
            #define XTRACE(ID,A,B,STRING) {Log("%8x %8x %8x %8x AppleUSBCDC: " STRING "\n",(unsigned int)(ID),(unsigned int)(A),(unsigned int)(B), (unsigned int)IOThreadSelf()); IOSleep(Sleep_Time);}
        #else
            #define XTRACE(id, x, y, msg)
        #endif /* USE_IOL */
    #endif /* USE_ELG */
#else /* not LDEBUG */
    #define XTRACE(id, x, y, msg)
    #undef USE_ELG
    #undef USE_IOL
#endif /* LDEBUG */

#define ALERT(A,B,STRING)	Log("%8x %8x AppleUSBCDC: " STRING "\n", (unsigned int)(A), (unsigned int)(B))