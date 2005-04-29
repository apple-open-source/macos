/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86Resources.h,v 1.16 2004/02/13 23:58:38 dawes Exp $ */

/*
 * Copyright (c) 1999-2002 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _XF86_RESOURCES_H

#define _XF86_RESOURCES_H

#include "xf86str.h"

#define _END {ResEnd,0,0}

#define _VGA_EXCLUSIVE \
		{ResExcMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
		{ResExcMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
		{ResExcMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF},\
		{ResExcIoBlock  | ResBios | ResBus,     0x03B0,     0x03BB},\
		{ResExcIoBlock  | ResBios | ResBus,     0x03C0,     0x03DF}

#define _VGA_SHARED \
		{ResShrMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
		{ResShrMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
		{ResShrMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF},\
		{ResShrIoBlock  | ResBios | ResBus,     0x03B0,     0x03BB},\
		{ResShrIoBlock  | ResBios | ResBus,     0x03C0,     0x03DF}

#define _VGA_SHARED_MEM \
		{ResShrMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
		{ResShrMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
 		{ResShrMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF}

#define _VGA_SHARED_IO \
		{ResShrIoBlock  | ResBios | ResBus,     0x03B0,     0x03BB},\
		{ResShrIoBlock  | ResBios | ResBus,     0x03C0,     0x03DF}

/*
 * Exclusive unused VGA:  resources unneeded but cannot be disabled.
 * Like old Millennium.
 */
#define _VGA_EXCLUSIVE_UNUSED \
	{ResExcUusdMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
	{ResExcUusdMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
	{ResExcUusdMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF},\
	{ResExcUusdIoBlock  | ResBios | ResBus,     0x03B0,     0x03BB},\
	{ResExcUusdIoBlock  | ResBios | ResBus,     0x03C0,     0x03DF}

/*
 * Shared unused VGA:  resources unneeded but cannot be disabled
 * independently.  This is used to determine if a device needs RAC.
 */
#define _VGA_SHARED_UNUSED \
	{ResShrUusdMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
	{ResShrUusdMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
	{ResShrUusdMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF},\
	{ResShrUusdIoBlock  | ResBios | ResBus,     0x03B0,     0x03BB},\
	{ResShrUusdIoBlock  | ResBios | ResBus,     0x03C0,     0x03DF}

/*
 * Sparse versions of the above for those adapters that respond to all ISA
 * aliases of VGA ports.
 */
#define _VGA_EXCLUSIVE_SPARSE \
	{ResExcMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
	{ResExcMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
	{ResExcMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF},\
	{ResExcIoSparse | ResBios | ResBus,     0x03B0,     0x03F8},\
	{ResExcIoSparse | ResBios | ResBus,     0x03B8,     0x03FC},\
	{ResExcIoSparse | ResBios | ResBus,     0x03C0,     0x03E0}

#define _VGA_SHARED_SPARSE \
	{ResShrMemBlock | ResBios | ResBus, 0x000A0000, 0x000AFFFF},\
	{ResShrMemBlock | ResBios | ResBus, 0x000B0000, 0x000B7FFF},\
	{ResShrMemBlock | ResBios | ResBus, 0x000B8000, 0x000BFFFF},\
	{ResShrIoSparse | ResBios | ResBus,     0x03B0,     0x03F8},\
	{ResShrIoSparse | ResBios | ResBus,     0x03B8,     0x03FC},\
	{ResShrIoSparse | ResBios | ResBus,     0x03C0,     0x03E0}

#define _8514_EXCLUSIVE \
	{ResExcIoSparse | ResBios | ResBus, 0x02E8, 0x03F8}

#define _8514_SHARED \
	{ResShrIoSparse | ResBios | ResBus, 0x02E8, 0x03F8}

/* Predefined resources */
extern resRange resVgaExclusive[];
extern resRange resVgaShared[];
extern resRange resVgaIoShared[];
extern resRange resVgaMemShared[];
extern resRange resVgaUnusedExclusive[];
extern resRange resVgaUnusedShared[];
extern resRange resVgaSparseExclusive[];
extern resRange resVgaSparseShared[];
extern resRange res8514Exclusive[];
extern resRange res8514Shared[];

/* Less misleading aliases for xf86SetOperatingState() */
#define resVgaMem resVgaMemShared
#define resVgaIo  resVgaIoShared
#define resVga    resVgaShared

/* Old style names */
#define RES_EXCLUSIVE_VGA   resVgaExclusive
#define RES_SHARED_VGA      resVgaShared
#define RES_EXCLUSIVE_8514  res8514Exclusive
#define RES_SHARED_8514     res8514Shared

#define _PCI_AVOID_PC_STYLE \
	{ResExcIoSparse | ResBus, 0x0100, 0x0300},\
	{ResExcIoSparse | ResBus, 0x0200, 0x0200},\
        {ResExcMemBlock | ResBus, 0xA0000,0xFFFFF}

extern resRange PciAvoid[];

#define RES_UNDEFINED NULL
#endif
