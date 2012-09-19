/*
 * Copyright © 2003-2012 Apple Inc. All rights reserved.
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
* © Copyright 2002 Apple Inc.  All rights reserved.
*
* IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in 
* consideration of your agreement to the following terms, and your use, installation, 
* modification or redistribution of this Apple software constitutes acceptance of these
* terms.  If you do not agree with these terms, please do not use, install, modify or 
* redistribute this Apple software.
*
* In consideration of your agreement to abide by the following terms, and subject to these 
* terms, Apple grants you a personal, non exclusive license, under Apple’s copyrights in this 
* original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute 
* the Apple Software, with or without modifications, in source and/or binary forms; provided 
* that if you redistribute the Apple Software in its entirety and without modifications, you 
* must retain this notice and the following text and disclaimers in all such redistributions 
* of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple 
* Computer, Inc. may be used to endorse or promote products derived from the Apple Software 
* without specific prior written permission from Apple. Except as expressly stated in this 
* notice, no other rights or licenses, express or implied, are granted by Apple herein, 
* including but not limited to any patent rights that may be infringed by your derivative 
* works or by other works in which the Apple Software may be incorporated.
* 
* The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
* EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-
* INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE 
* SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 
*
* IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
* REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
* WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
* OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include "printInterpretedError.h"

#include <unistd.h>

void printInterpretedError(char *s, IOReturn err)
{
// These should be defined somewhere, but I can't find them. These from Accessing hardware.

#if 0
static struct{int err; char *where;} systemSources[] = {
    {0, "kernel"},
    {1, "user space library"},
    {2, "user space servers"},
    {3, "old ipc errors"},
    {4, "mach-ipc errors"},
    {7, "distributed ipc"},
    {0x3e, "user defined errors"},
    {0x3f, "(compatibility) mach-ipc errors"}
    };
#endif

UInt32 system, sub, code;
    
    fprintf(stderr, "%s (0x%08X) ", s, err);
    
    system = err_get_system(err);
    sub = err_get_sub(err);
    code = err_get_code(err);
    
    if(system == err_get_system(sys_iokit))
    {
        if(sub == err_get_sub(sub_iokit_usb))
        {
            fprintf(stderr, "USB error %ld(0x%lX) ", code, code);
        }
        else if(sub == err_get_sub(sub_iokit_common))
        {
            fprintf(stderr, "IOKit common error %ld(0x%lX) ", code, code);
        }
        else
        {
            fprintf(stderr, "IOKit error %ld(0x%lX) from subsytem %ld(0x%lX) ", code, code, sub, sub);
        }
    }
    else
    {
        fprintf(stderr, "error %ld(0x%lX) from system %ld(0x%lX) - subsytem %ld(0x%lX) ", code, code, system, system, sub, sub);
    }
}
