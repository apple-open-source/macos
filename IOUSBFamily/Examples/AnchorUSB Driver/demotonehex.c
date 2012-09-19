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
* © Copyright 2001-2002 Apple Inc.  All rights reserved.
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
#include "IOKit/IOTypes.h"
#include "hex2c.h"
INTEL_HEX_RECORD demotonehex[] = {
   {
        16,
        0x1800,
        0,
        {0xe4,0x90,0x7f,0x94,0xf0,0x90,0x7f,0x97,0xf0,0x90,0x7f,0x9d,0x74,0x0f,0xf0,0xe4}
    },
    {
        16,
        0x1810,
        0,
        {0x90,0x7f,0x95,0xf0,0x90,0x7f,0x98,0x74,0xf0,0xf0,0x90,0x7f,0x9e,0xf0,0xe4,0xfb}
    },
    {
        16,
        0x1820,
        0,
        {0xfa,0xae,0x02,0xaf,0x03,0x7c,0x00,0x7d,0x20,0x12,0x18,0x60,0xed,0x4c,0x60,0x08}
    },
    {
        16,
        0x1830,
        0,
        {0x90,0x7f,0x98,0x74,0xf0,0xf0,0x80,0x05,0xe4,0x90,0x7f,0x98,0xf0,0x0b,0xbb,0x00}
    },
    {
        16,
        0x1840,
        0,
        {0x01,0x0a,0xeb,0x64,0x01,0x4a,0x70,0xd9,0x90,0x7f,0x97,0x74,0x0f,0xf0,0xe4,0x90}
    },
    {
        4,
        0x1850,
        0,
        {0x7f,0x9d,0xf0,0x22}
    },
    {
        3,
        0x0,
        0,
        {0x02,0x18,0x54}
    },
    {
        12,
        0x1854,
        0,
        {0x78,0x7f,0xe4,0xf6,0xd8,0xfd,0x75,0x81,0x07,0x02,0x18,0x00}
    },
    {
        16,
        0x1860,
        0,
        {0xc2,0xd5,0xec,0x30,0xe7,0x09,0xb2,0xd5,0xe4,0xc3,0x9d,0xfd,0xe4,0x9c,0xfc,0xee}
    },
    {
        16,
        0x1870,
        0,
        {0x30,0xe7,0x15,0xb2,0xd5,0xe4,0xc3,0x9f,0xff,0xe4,0x9e,0xfe,0x12,0x18,0x96,0xc3}
    },
    {
        16,
        0x1880,
        0,
        {0xe4,0x9d,0xfd,0xe4,0x9c,0xfc,0x80,0x03,0x12,0x18,0x96,0x30,0xd5,0x07,0xc3,0xe4}
    },
    {
        6,
        0x1890,
        0,
        {0x9f,0xff,0xe4,0x9e,0xfe,0x22}
    },
    {
        16,
        0x1896,
        0,
        {0xbc,0x00,0x0b,0xbe,0x00,0x29,0xef,0x8d,0xf0,0x84,0xff,0xad,0xf0,0x22,0xe4,0xcc}
    },
    {
        16,
        0x18a6,
        0,
        {0xf8,0x75,0xf0,0x08,0xef,0x2f,0xff,0xee,0x33,0xfe,0xec,0x33,0xfc,0xee,0x9d,0xec}
    },
    {
        16,
        0x18b6,
        0,
        {0x98,0x40,0x05,0xfc,0xee,0x9d,0xfe,0x0f,0xd5,0xf0,0xe9,0xe4,0xce,0xfd,0x22,0xed}
    },
    {
        16,
        0x18c6,
        0,
        {0xf8,0xf5,0xf0,0xee,0x84,0x20,0xd2,0x1c,0xfe,0xad,0xf0,0x75,0xf0,0x08,0xef,0x2f}
    },
    {
        16,
        0x18d6,
        0,
        {0xff,0xed,0x33,0xfd,0x40,0x07,0x98,0x50,0x06,0xd5,0xf0,0xf2,0x22,0xc3,0x98,0xfd}
    },
    {
        5,
        0x18e6,
        0,
        {0x0f,0xd5,0xf0,0xea,0x22}
    },
    {
        0,
        0x0,
        1,
        {0}
    }
};
