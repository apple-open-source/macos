/* 
 * Copyright (c) 1998  Metro Link Incorporated
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/magellan/magellan.h,v 1.4 2002/06/07 21:03:27 alanh Exp $ */

#ifndef	_MAGELLAN_H_
#define _MAGELLAN_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define MAGELLAN_PACKET_SIZE		256
#define MAGELLAN_RES			200
/*
These numbers are difficult to pick because the user can change a
multiplier using the buttons which changes the output range. However, many
programs use this range to perform operations of their own and the XInput
extension expects these to be constant for a given device. Hence, I've picked
values that are slightly higher than any reported by my test device in default
(no multiplier) mode. The documentation says the range is roughly +/- 400 but
I have seen numbers close to 1800 using the "sensitivity" setting as set below.
Some programs (GLUT) may get upset if the device reports a value greater than 
XInput reported it could.
*/
#define MAGELLAN_MIN            -1800
#define MAGELLAN_MAX            +1800

#define MagellanAttention "\r\r"		/* get device's attention */
#define MagellanInitString "z\r"		/* switch in 3D mode */
#define MagellanShortBeep "b9\r"		/* a short beep */
#define MagellanMode "m3\r"			/* translation and rotation data ON */
#define MagellanPeriod "pAA\r"			/* transmit every 60 ms data */
#define MagellanNullRadius "nH\r"		/* null radius to value 8 */
#define MagellanZero "z\r"			/* detect zero position */
#define MagellanSensitivity "q00\r"		/* no extra sensitivity */
#define MagellanVersion "vQ\r"			/* get version string */
#define MagellanModeOff "\r\rm0\r"

typedef enum
{
	magellan_normal
}
MAGELLANState;

#define MagellanNibble(Value)       (Value&0x0F)

#define milisleep(ms) xf86usleep (ms * 1000)

typedef struct _MagellanPrivateRec
{
	XISBuffer *buffer;
	unsigned char packet_type;
	char packet[MAGELLAN_PACKET_SIZE];	/* packet being/just read */
	int packeti;				/* index into packet */
        int expected_len;
	MAGELLANState lex_mode;
	int old_buttons;
}
MAGELLANPrivateRec, *MAGELLANPrivatePtr;

/******************************************************************************
 *		Declarations
 *****************************************************************************/
#ifdef XFreeLOADER
static MODULESETUPPROTO( MAGELLANSetupProc );
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
static Bool QueryHardware (MAGELLANPrivatePtr);
static void NewPacket (MAGELLANPrivatePtr priv);
static Bool MAGELLANGetPacket (MAGELLANPrivatePtr priv);

static InputInfoPtr
MagellanPreInit(InputDriverPtr drv, IDevPtr dev, int flags);

#ifdef BELL_FEEDBACK_SUPPORT
static void MagellanBellCtrl( DeviceIntPtr, BellCtrl *);
static void MagellanBellSound(int percent, DeviceIntPtr dev, pointer ctrl, int
unknown);
#endif
/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif


