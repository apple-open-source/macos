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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/elo2300/elo.h,v 1.2 1999/02/01 12:12:59 dawes Exp $ */

#ifndef	_elo_H_
#define _elo_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define ELO_PACKET_SIZE		10

															  /* Note: some of them are not supported by the E281-2310 *//* JPM */

#define ELO_SYNC_BYTE       'U'	/* Sync byte. First of a packet.    */
#define ELO_TOUCH           'T'	/* Report of touchs and motions.    */
#define ELO_OWNER           'O'	/* Report vendor name.          */
#define ELO_ID              'I'	/* Report of type and features.     */
#define ELO_MODE            'M'	/* Set current operating mode.      */
#define ELO_PARAMETER       'P'	/* Set the serial parameters.       */
#define ELO_REPORT          'B'	/* Set touch reports timings.       */
#define ELO_CALIBRATION     'C'	/* Calibration command.         */
#define ELO_ACK             'A'	/* Acknowledge packet           */
#define ELO_SERIAL_IO       '0'	/* Indicator byte for PARAMETER command */

#define ELO_INIT_CHECKSUM       0xAA	/* Initial value of checksum.       */

#define ELO_PRESS               0x01	/* Flags in ELO_TOUCH status byte   */
#define ELO_STREAM              0x02
#define ELO_RELEASE             0x04

#define ELO_TOUCH_MODE          0x01	/* Flags in ELO_MODE command        */
#define ELO_STREAM_MODE         0x02
#define ELO_UNTOUCH_MODE        0x04
#define ELO_RANGE_CHECK_MODE    0x40
#define ELO_ENABLE_Z_MODE       0x80
#define ELO_TRIM_MODE           0x02
#define ELO_CALIB_MODE          0x04
#define ELO_SCALING_MODE        0x08
#define ELO_TRACKING_MODE       0x40

#define ELO_SERIAL_SPEED        0x06	/* Flags for high speed serial
										 * (19200) */
#define ELO_SERIAL_MASK         0xF8

#define ELO_MAX_WRONG_PACKETS   200		/* Number of wrong packets to accept
										 * before giving up when looking for
										 * a specific packet type */
#define ELO_MAX_EMPTY_PACKETS   3	/* Number of empty reads to accept before 
									 * giving up when looking for a reply */
#define ELO_MAX_WAIT        100000	/* Max wait time for a reply (microsec) */

#define ELO_UNTOUCH_DELAY   10	/* 100 ms               */
#define ELO_REPORT_DELAY    4	/* 40 ms or 25 motion reports/s     */

typedef enum
{
	elo_normal, elo_body, elo_checksum
}
ELOState;

#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 8) | (byte1))

typedef struct _EloPrivateRec
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
	unsigned char packet[ELO_PACKET_SIZE];	/* packet being/just read */
	int packeti;				/* index into packet */
	unsigned char checksum;		/* Current checksum of data in assembly *
								 * buffer   */
	ELOState lex_mode;
}
EloPrivateRec, *EloPrivatePtr;

/******************************************************************************
 *		Declarations
 *								variables:	use elo_LOC in front
 *											of globals.
 *											put locals in the .c file.
 *****************************************************************************/
static MODULESETUPPROTO( SetupProc );
static void TearDownProc (pointer p);
static Bool DeviceControl (DeviceIntPtr dev, int mode);
static Bool DeviceOn (DeviceIntPtr dev);
static Bool DeviceOff (DeviceIntPtr dev);
static Bool DeviceClose (DeviceIntPtr dev);
static Bool DeviceInit (DeviceIntPtr dev);
static void ReadInput (LocalDevicePtr local);
static int ControlProc (LocalDevicePtr local, xDeviceCtl * control);
static void CloseProc (LocalDevicePtr local);
static int SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode);
static Bool ConvertProc (LocalDevicePtr local, int first, int num, int v0, int v1, int v2, int v3, int v4, int v5, int *x, int *y);
static Bool QueryHardware (EloPrivatePtr priv, int *errmaj, int *errmin);
static Bool EloSendControl (unsigned char *control, EloPrivatePtr priv);
static Bool EloSendQuery (unsigned char *request, EloPrivatePtr priv);
static Bool EloSendPacket (unsigned char *packet, XISBuffer * b);
static Bool EloWaitReply (unsigned char type, EloPrivatePtr priv);
static void EloNewPacket (EloPrivatePtr priv);
static Bool EloGetPacket (EloPrivatePtr priv);
static Bool EloWaitAck (EloPrivatePtr priv);
static void EloPrintIdent (unsigned char *packet);

/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
