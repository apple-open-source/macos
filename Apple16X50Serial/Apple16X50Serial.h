/*
Copyright (c) 1997-2002 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50Serial.h
 * Common types, macros, and helpers used by all compilation units in the Apple16X50Serial project.
 *
 * 2002-02-15	dreece	created.
 */

#ifndef _APPLEPCI16X50SERIAL_H
#define _APPLEPCI16X50SERIAL_H

// Use the "Build Styles" picker under the "Targets" tab to select DEBUG or ASSERT builds,
// or uncomment one or both of the following lines:
//#define DEBUG
//#define ASSERT

extern "C" void conslog_putc(char);
#define IO_putc(c) conslog_putc(c)

#ifdef DEBUG
 #define DEBUG_IOLog(args...)	IOLog (args)
 #define DEBUG_putc(c)		conslog_putc(c)
 #ifdef ASSERT
  #warning DEBUG and ASSERT are defined - resulting kexts are not suitable for deployment
 #else
  #warning DEBUG is defined - resulting kexts are not suitable for deployment
 #endif
#else
 #define DEBUG_IOLog(args...)
 #define DEBUG_putc(c)
 #ifdef ASSERT
  #warning ASSERT is defined - resulting kexts are not suitable for deployment
 #endif
#endif
#ifdef ASSERT
#define assert(ex)				\
{						\
    if (!(ex)) {				\
        IOLog( __FILE__ ":%d :", __LINE__ );	\
        Debugger("assert(" #ex ") failed");	\
    }						\
}
#define	_KERN_ASSERT_H_
#endif

#include <IOKit/IOLib.h>

// selects between bits of a and b.  if a bit in m is set, then take the corresponding
// bit of b; otherwise, take the corresponding bit of a.
UInt32 static inline maskMux(UInt32 a, UInt32 b, UInt32 m) { return (a&(~m)) | (b&m); }

// if b is true, then return a with all m's bits set; otherwise return a with all m's
// bits cleared.
UInt32 static inline boolBit(UInt32 a, bool b, UInt32 m) { return b ? (a|m) : (a&(~m)); }

// force an ordinal to conform to exact boolean values
#define BOOLVAL(x)	((bool) (x) ? (true) : (false))
#define BOOLSTR(x)	((x) ? "true" : "false")

// return new value of (val) constrained such that min ≤ val ≤ max
#define CONSTRAIN(min,val,max)	( ((val)<(min)) ? (min) : (((val)>(max)) ? (max) : (val)) )

// release an object iff it is not null, set the refernce to null after release
#define RELEASE(obj) do { if(obj) { (obj)->release(); (obj)=NULL; } } while (0)

// log a capabilities byte - each bit that is set in 'byte' will cause the
// corresponding string to be IOLog'd.  'lab' specifies a label to log first.
#define IOLogByte(byte, lab, s7, s6, s5, s4, s3, s2, s1, s0)		\
if (byte) {								\
    int i, any=0; char *str[8]={ s0, s1, s2, s3, s4, s5, s6, s7 };	\
        IOLog("%s=<", (lab));						\
            for (i=0;i<8;i++)							\
                if (byte&(1<<i)) {						\
                    if (any++) IO_putc(',');					\
                        if (str[i]) IOLog(str[i]); else IOLog("%d?",i);		\
                }								\
                IO_putc('>');							\
}

// OSString property commonly used in kexts, but no define is otherwise available
#define kCFBundleIdentifierKey	"CFBundleIdentifier"

// OSString property used by the Network Preference panel to display the name of a port
#define kNPProductNameKey	"Product Name"

// OSString property used by the Network Preference panel to display the name of a port
#define kLocationKey	"Location"

// OSNumber Property that describes the master clock frequency of each UART
#define kMasterClock	"Master Clock"

#endif //_APPLEPCI16X50SERIAL_H