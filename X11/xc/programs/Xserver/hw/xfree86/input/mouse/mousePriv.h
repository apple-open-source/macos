/* $XFree86: xc/programs/Xserver/hw/xfree86/input/mouse/mousePriv.h,v 1.12 2004/02/13 23:58:44 dawes Exp $ */
/*
 * Copyright (c) 1997-1999 by The XFree86 Project, Inc.
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

#ifndef _X_MOUSEPRIV_H
#define _X_MOUSEPRIV_H

#if 0
# define MOUSEINITDEBUG
# define MOUSEDATADEBUG
#endif

#include "mouse.h"
#include "xf86Xinput.h"                                                                                              
/* Private interface for the mouse driver. */

typedef enum  {
    AUTOPROBE_H_NOPROTO,
    AUTOPROBE_H_GOOD,
    AUTOPROBE_H_AUTODETECT,
    AUTOPROBE_H_VALIDATE1,
    AUTOPROBE_H_VALIDATE2,
    AUTOPROBE_H_SETPROTO,
    AUTOPROBE_NOPROTO,
    AUTOPROBE_COLLECT,
    AUTOPROBE_CREATE_PROTOLIST,
    AUTOPROBE_GOOD,
    AUTOPROBE_AUTODETECT,
    AUTOPROBE_VALIDATE1,
    AUTOPROBE_VALIDATE2,
    AUTOPROBE_SWITCHSERIAL,
    AUTOPROBE_SWITCH_PROTOCOL
} mseAutoProbeStates;

typedef struct {
    const char *	name;
    int			class;
    const char **	defaults;
    MouseProtocolID	id;
} MouseProtocolRec, *MouseProtocolPtr;

#define NUM_MSE_AUTOPROBE_BYTES 24  /* multiple of 3,4 and 6 byte packages */
#define NUM_MSE_AUTOPROBE_TOTAL 64
#define NUM_AUTOPROBE_PROTOS 17


typedef struct {
    int		current;
    Bool	inReset;
    CARD32	lastEvent;
    CARD32	expires;
    Bool	soft;
    int		goodCount;
    int		badCount;
    int		protocolID;
    int		count;
    char	data[NUM_MSE_AUTOPROBE_TOTAL];
    mseAutoProbeStates autoState;
    MouseProtocolID protoList[NUM_AUTOPROBE_PROTOS];
    int		serialDefaultsNum;
    int		prevDx, prevDy;
    int		accDx, accDy;
    int		acc;
    CARD32	pnpLast;
    Bool	disablePnPauto;
} mousePrivRec, *mousePrivPtr;

/* mouse proto flags */
#define MPF_NONE		0x00
#define MPF_SAFE		0x01

/* pnp.c */
MouseProtocolID MouseGetPnpProtocol(InputInfoPtr pInfo);
Bool ps2Reset(InputInfoPtr pInfo);
Bool ps2EnableDataReporting(InputInfoPtr pInfo);
Bool ps2SendPacket(InputInfoPtr pInfo, unsigned char *bytes, int len);
int ps2GetDeviceID(InputInfoPtr pInfo);

#endif /* _X_MOUSE_H */
