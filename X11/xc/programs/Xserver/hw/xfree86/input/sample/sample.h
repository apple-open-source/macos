/*
 * THIS SAMPLE INPUT DRIVER IS OUT OF DATE.  DO NOT USE IT AS A TEMPLATE
 * WHEN WRITING A NEW INPUT DRIVER.
 */

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/sample/sample.h,v 1.4 2002/01/15 15:32:45 dawes Exp $ */

#ifndef	_SAMPLE_H_
#define _SAMPLE_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define SAMPLE_PACKET_SIZE		10
#define SAMPLE_SYNC_BYTE		'T'
#define SAMPLE_INIT_CHECKSUM	0
#define SAMPLE_BODY_LEN			9

typedef enum
{
	SAMPLE_normal, SAMPLE_type, SAMPLE_body, SAMPLE_checksum
}
SAMPLEState;

#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 8) | (byte1))

typedef struct _SAMPLEPrivateRec
{
	int min_x;					/* Minimum x reported by calibration        */
	int max_x;					/* Maximum x                    */
	int min_y;					/* Minimum y reported by calibration        */
	int max_y;					/* Maximum y                    */
	int button_threshold;		/* Z > button threshold = button click */
	int axes;
	Bool button_down;			/* is the "button" currently down */
	int button_number;			/* which button to report */
	int reporting_mode;			/* TS_Raw or TS_Scaled */

	int untouch_delay;			/* Delay before reporting an untouch (in ms) */
	int report_delay;			/* Delay between touch report packets       */

	int screen_num;				/* Screen associated with the device        */
	int screen_width;			/* Width of the associated X screen     */
	int screen_height;			/* Height of the screen             */
	XISBuffer *buffer;
	unsigned char packet[SAMPLE_PACKET_SIZE];	/* packet being/just read */
	int packeti;				/* index into packet */
	unsigned char checksum;		/* Current checksum of data in assembly *
								 * buffer   */
	SAMPLEState lex_mode;
}
SAMPLEPrivateRec, *SAMPLEPrivatePtr;

/******************************************************************************
 *		Declarations
 *****************************************************************************/
static MODULESETUPPROTO( SetupProc );
static void TearDownProc (pointer p);
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
static Bool QueryHardware (SAMPLEPrivatePtr, int *, int *);
static Bool SAMPLEGetPacket (SAMPLEPrivatePtr priv);
/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
