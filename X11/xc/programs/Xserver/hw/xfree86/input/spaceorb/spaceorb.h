/* 
 * Author: Guido Heumer <gheumer@hons.cs.usyd.edu.au>
 *
 * Template driver used: Copyright (c) 1998  Metro Link Incorporated
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the Metro Link shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Metro Link.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/spaceorb/spaceorb.h,v 1.4 2001/10/28 03:33:58 tsi Exp $ */

#ifndef	_SPACEORB_H_
#define _SPACEORB_H_

/* check if it works without

#include <xf86Module.h> */

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define SPACEORB_PACKET_SIZE		60
#define SPACEORB_R_BODY_LEN			50
#define SPACEORB_K_BODY_LEN			4
#define SPACEORB_D_BODY_LEN			11
#define SPACEORB_R_PACKET			'R'
#define SPACEORB_K_PACKET			'K'
#define SPACEORB_D_PACKET			'D'

typedef enum
{
	SPACEORB_normal, SPACEORB_body, SPACEORB_Dbody
}
SPACEORBState;

#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 8) | (byte1))

typedef struct _SPACEORBPrivateRec
{
	XISBuffer *buffer;
	unsigned char packet_type;
	unsigned char packet[SPACEORB_PACKET_SIZE];	/* packet being/just read */
	int packeti;				/* index into packet */
	int expected_len;
	SPACEORBState lex_mode;
	int old_buttons;
}
SPACEORBPrivateRec, *SPACEORBPrivatePtr;

/******************************************************************************
 *		Declarations
 *****************************************************************************/
#ifdef XFree86LOADER
static MODULESETUPPROTO(SPACEORBSetupProc);
static void TearDownProc (pointer p);
#endif
static Bool DeviceControl (DeviceIntPtr, int);
static Bool DeviceOn (DeviceIntPtr);
static Bool DeviceOff (DeviceIntPtr);
static Bool DeviceClose (DeviceIntPtr);
static Bool DeviceInit (DeviceIntPtr);
static void ReadInput (LocalDevicePtr);
static int ControlProc (LocalDevicePtr, xDeviceCtl *);
static void CloseProc (LocalDevicePtr);
static int SwitchMode (ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc (LocalDevicePtr, int, int, int, int, int, int, int, int, int *, int *);
static Bool QueryHardware (SPACEORBPrivatePtr);
static void NewPacket (SPACEORBPrivatePtr priv);
static Bool SPACEORBGetPacket (SPACEORBPrivatePtr priv);

static InputInfoPtr
SpaceorbPreInit(InputDriverPtr drv, IDevPtr dev, int flags);
/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
