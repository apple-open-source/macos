/* 
 * Copyright (c) 1999  Machine Vision Holdings Incorporated
 * Author: David Woodhouse <David.Woodhouse@mvhi.com>
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/dynapro/xf86Dyna.h,v 1.2 1999/08/22 05:57:37 dawes Exp $ */

#ifndef	_DYNAPRO_H_
#define _DYNAPRO_H_

/******************************************************************************
 *		Definitions
 *				structs, typedefs, #defines, enums
 *****************************************************************************/

#define DYNAPRO_PACKET_SIZE		3

typedef enum
{
	Dynapro_byte0, Dynapro_byte1, Dynapro_byte2
}
DynaproState;


typedef struct _DynaproPrivateRec
{
	int min_x;				/* Minimum x reported by calibration        */
	int max_x;				/* Maximum x                    */
	int min_y;				/* Minimum y reported by calibration        */
	int max_y;				/* Maximum y                    */
	Bool button_down;			/* is the "button" currently down */
	int button_number;			/* which button to report */
	int reporting_mode;			/* TS_Raw or TS_Scaled */

	int screen_num;				/* Screen associated with the device        */
	int screen_width;			/* Width of the associated X screen     */
	int screen_height;			/* Height of the screen             */
	int proximity;
	int swap_xy;
	XISBuffer *buffer;
	unsigned char packet[DYNAPRO_PACKET_SIZE];	/* packet being/just read */
	DynaproState lex_mode;
}
DynaproPrivateRec, *DynaproPrivatePtr;


/******************************************************************************
 *		Declarations
 *****************************************************************************/

static Bool DeviceControl (DeviceIntPtr, int);
static void ReadInput (InputInfoPtr);
static int ControlProc (InputInfoPtr, xDeviceCtl *);
static void CloseProc (InputInfoPtr);
static int SwitchMode (ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc (InputInfoPtr, int, int, int, int, int, int, int, int, int *, int *);
static Bool QueryHardware (DynaproPrivatePtr);
static Bool DynaproGetPacket (DynaproPrivatePtr priv);

static InputInfoPtr
DynaproPreInit(InputDriverPtr drv, IDevPtr dev, int flags);

static void
DynaproPtrCtrl(DeviceIntPtr device, PtrCtrl *ctrl);


#endif /* _DYNAPRO_H_ */
