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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/microtouch/microtouch.h,v 1.4 2000/03/03 20:36:41 dawes Exp $ */

#ifndef	_microtouch_H_
#define _microtouch_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define MuT_REPORT_SIZE		5	/* Size of a report packet.           */
#define MuT_BUFFER_SIZE		256	/* Size of input buffer.          */
#define MuT_PACKET_SIZE		10	/* Maximum size of a command/reply
								 * *including*    */
#define MuT_MAX_WRONG_PACKETS   20	/* Number of wrong packets to accept
									 * before giving up when looking for a
									 * specific packet type */
#define MuT_MAX_EMPTY_PACKETS   5	/* Number of empty reads to accept before 
									 * giving up when looking for a reply */
#define MuT_RETRIES				3
#define MuT_MAX_WAIT        100000		/* Max wait time for a reply
										 * (microsec) */

					/* the leading and trailing bytes.      */
#define MuT_LEAD_BYTE		0x01	/* First byte of a command/reply packet.
									 * */
#define MuT_TRAIL_BYTE		0x0D	/* Last byte of a command/reply packet.
									 *    */

/* 
 * Commands.
 */
#define MuT_RESET		"R"		/* Reset the controller.          */
#define MuT_OUTPUT_IDENT	"OI"	/* Ask some infos about the firmware. */
#define MuT_UNIT_TYPE		"UT"	/* Ask some more infos about the *
									 * firmware. */
#define MuT_UNIT_VERIFY		"UV"	/* Ask some more infos about the *
									 * firmware. some hardware only supports
									 * * this version */
#define MuT_ABDISABLE		"AD"	/* disable auto baud detection */
#define MuT_SETRATE			"PN812"		/* set com parameters to
										 * 8,1,none,9600 */
#define MuT_FORMAT_TABLET	"FT"	/* Report events using tablet format. */
#define MuT_FINGER_ONLY     "FO"	/* Always send reports. */
#define MuT_MODE_STREAM		"MS"	/* Receive reports in stream mode */
#define MuT_PARAM_LOCK		"PL"	/* write settings to nvram */

#define MuT_OK			"0"		/* Report success.                */
#define MuT_OK7			0xb0	/* 7bit Report success as seen in 8bit mode */
#define MuT_ERROR		"1"		/* Report error.                */
#define MuT_ANY			""		/* Accept any reply               */

#define MuT_TOUCH_PEN_IDENT	"P5"
#define MuT_SMT3_IDENT		"Q1"
#define MuT_GENERAL_IDENT		"A3"

#define MuT_CONTACT     0x40	/* Report touch/untouch with touchscreen. */

#define ACK				0
#define NACK			-1
#define TIMEOUT 		-2
#define WRONG_PACKET	-3

typedef enum
{
	microtouch_normal, microtouch_body, mtouch_binary
}
MuTState;

typedef struct _MuTPrivateRec
{
	int min_x;					/* Minimum x reported by calibration        */
	int max_x;					/* Maximum x                    */
	int min_y;					/* Minimum y reported by calibration        */
	int max_y;					/* Maximum y                    */
	Bool proximity;				/* is the stylus in proximity */
	Bool button_down;			/* is the "button" currently down */
	int button_number;			/* which button to report */
	int reporting_mode;			/* TS_Raw or TS_Scaled */

	int screen_num;				/* Screen associated with the device        */
	int screen_width;			/* Width of the associated X screen     */
	int screen_height;			/* Height of the screen             */
	XISBuffer *buffer;
	unsigned char packet[MuT_BUFFER_SIZE];	/* packet being/just read */
	int packeti;				/* index into packet */
	Bool cs7flag;
	Bool binary_pkt;			/* indicates packet was a binary touch */

	MuTState lex_mode;
	int bin_byte;				/* bytes recieved in binary packet */
}
MuTPrivateRec, *MuTPrivatePtr;

/******************************************************************************
 *		Declarations
 *****************************************************************************/
/*int DumpOpts (XF86OptionPtr opts);	*/
static Bool DeviceControl (DeviceIntPtr dev, int mode);
static Bool DeviceOn (DeviceIntPtr dev);
static Bool DeviceOff (DeviceIntPtr dev);
static Bool DeviceInit (DeviceIntPtr dev);
static void ReadInput (LocalDevicePtr local);
static int ControlProc (LocalDevicePtr local, xDeviceCtl * control);
static void CloseProc (LocalDevicePtr local);
static int SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode);
static Bool ConvertProc (LocalDevicePtr local, int first, int num, int v0, int v1, int v2, int v3, int v4, int v5, int *x, int *y);
static Bool xf86MuTSendCommand (unsigned char *type, MuTPrivatePtr priv);
static Bool QueryHardware (LocalDevicePtr local);
static void MuTNewPacket (MuTPrivatePtr priv);
static Bool xf86MuTSendPacket (unsigned char *type, int len, MuTPrivatePtr priv);
static int xf86MuTWaitReply (unsigned char *type, MuTPrivatePtr priv);
static Bool MuTGetPacket (MuTPrivatePtr priv);
static void xf86MuTPrintIdent (unsigned char *packet);
static Bool xf86MuTPrintHwStatus (unsigned char *packet);
/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
