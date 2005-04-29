/* 
 * Copyright (c) 2001 Edouard TISSERANT <tissered@esstin.u-nancy.fr>
 * Parts inspired from Shane Watts <shane@bofh.asn.au> Xfree 3 Acecad Driver
 * Thanks to Emily, from AceCad, For giving me documents.
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/acecad/acecad.h,v 1.3 2003/11/06 18:38:11 tsi Exp $ */

#ifndef	_ACECAD_H_
#define _ACECAD_H_

/******************************************************************************
 *		Definitions
 *		structs, typedefs, #defines, enums
 *****************************************************************************/
#define ACECAD_PACKET_SIZE		7

#define ACECAD_CONFIG		"a"		/* Send configuration (max coords) */

#define ACECAD_ABSOLUTE		'F'		/* Absolute mode */
#define ACECAD_RELATIVE		'E'		/* Relative mode */

#define ACECAD_UPPER_ORIGIN	"b"		/* Origin upper left */

#define ACECAD_PROMPT_MODE	"B"		/* Prompt mode */
#define ACECAD_STREAM_MODE	"@"		/* Stream mode */
#define ACECAD_INCREMENT	'I'		/* Set increment */
#define ACECAD_BINARY_FMT	"zb"	/* Binary reporting */

#define ACECAD_PROMPT		"P"		/* Prompt for current position */

#define PHASING_BIT		0x80
#define PROXIMITY_BIT	0x40
#define TABID_BIT		0x20
#define XSIGN_BIT		0x10
#define YSIGN_BIT		0x08
#define BUTTON_BITS		0x07
#define COORD_BITS		0x7f

#define ABSOLUTE_FLAG		1

#define milisleep(ms) xf86usleep (ms * 1000)

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

static const char * acecad_initstr = ACECAD_BINARY_FMT ACECAD_STREAM_MODE;

typedef struct 
{
	XISBuffer *buffer;
    int		acecadInc;		/* increment between transmits */
    int		acecadOldX;		/* previous X position */
    int		acecadOldY;		/* previous Y position */
    int		acecadOldZ;		/* previous Z position */
    int		acecadOldProximity;	/* previous proximity */
    int		acecadOldButtons;	/* previous buttons state */
    int		acecadMaxX;		/* max X value */
    int		acecadMaxY;		/* max Y value */
    int		acecadMaxZ;		/* max Y value */
    char	acecadReportSpeed;		/* report speed */
    int		acecadUSB;		/*USB flag*/
    int		flags;			/* various flags */
    int		packeti;		/* number of bytes read */
    int		PacketSize;		/* number of bytes read */
    unsigned char packet[ACECAD_PACKET_SIZE];	/* data read on the device */
} AceCadPrivateRec, *AceCadPrivatePtr;


/******************************************************************************
 *		Declarations
 *****************************************************************************/
#ifdef XFree86LOADER
static MODULESETUPPROTO( SetupProc );
static void TearDownProc (void *);
#endif
static Bool DeviceControl (DeviceIntPtr, int);
static Bool DeviceOn (DeviceIntPtr);
static Bool DeviceOff (DeviceIntPtr);
static Bool DeviceClose (DeviceIntPtr);
static Bool DeviceInit (DeviceIntPtr);
static void ReadInput (LocalDevicePtr);
static void CloseProc (LocalDevicePtr);
static Bool ConvertProc (LocalDevicePtr, int, int, int, int, int, int, int, int, int *, int *);
static Bool ReverseConvertProc(LocalDevicePtr , int , int , int*);
static Bool QueryHardware (AceCadPrivatePtr);
static void NewPacket (AceCadPrivatePtr priv);
static Bool AceCadGetPacket (AceCadPrivatePtr);
static InputInfoPtr AceCadPreInit(InputDriverPtr, IDevPtr , int);
#ifdef LINUX_INPUT
static void USBReadInput (LocalDevicePtr);
static Bool USBQueryHardware (LocalDevicePtr);
static int IsUSBLine(int);
#endif


#endif
