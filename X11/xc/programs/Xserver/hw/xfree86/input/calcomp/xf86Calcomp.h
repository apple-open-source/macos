/* 
 * Copyright (c) 2000-2001 Martin Kroeker (mk@daveg.com)
 * code derived from sample driver
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/input/calcomp/xf86Calcomp.h,v 1.2 2001/10/28 03:33:55 tsi Exp $ */

#ifndef	_CALCOMP_H_
#define _CALCOMP_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/
#define CALCOMP_PACKET_SIZE		5
#define CALCOMP_INIT_CHECKSUM	0
#define CALCOMP_BODY_LEN			6



/*************************************************************************/
#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define DB_COMM_SETUP   "\033%C1N81\r"  /* Serial communication setup */
#define DB_FIRMID       "\033%__V\r"    /* Request firmware ID string */
#define DB_PRODID       "\033%__p\r"    /* Request product  ID string */
#define DB_CONFIG       "\033%VS\r"     /* Send configuration (max coords) */

#define DB_ABSOLUTE     "\033%IR\r"     /* Absolute mode */
#define DB_RELATIVE     "\033%IT\r"     /* Relative mode */

#define DB_UPPER_ORIGIN "\033%L1\r"     /* Origin upper left */
#define DB_1000LPI      "\033%JR1000,0\r" /* 1000 lines per inch */

#define DB_PROMPT_MODE  "\033%Q?\r"     /* Prompt mode (untested!) */
#define DB_STREAM_MODE  "\033%IR\r"     /* Stream mode */
#define DB_XINCREMENT    "\033%X1\r"
#define DB_YINCREMENT    "\033%Y1\r"      /* Set increment to 1 */
#define DB_BINARY_FMT   "\033%^23\r"  /* Calcomp binary mode */
#define DB_PRESSURE_OFF	"\033%VA0\r" /* Select button or pressure */
#define DB_PRESSURE_ON	"\033%VA1\r" /* reporting */
#define DB_PROMPT       "?"     /* Prompt for current position */

#define PHASING_BIT     0x80
#define PROXIMITY_BIT   0x20
#define BUTTON_BITS     0x7c
#define COORD_BITS      0x7f
#define tcflush(fd, n) xf86FlushInput((fd))
#undef read
#define read(a,b,c) xf86ReadSerial((a),(b),(c))
#undef write
#define write(a,b,c) xf86WriteSerial((a),(char*)(b),(c))
#undef close
#define close(a) xf86CloseSerial((a))

/*************************************************************************/
typedef enum
{
	CALCOMP_normal, /*CALCOMP_type,*/ CALCOMP_body, CALCOMP_checksum
}
CALCOMPState;

#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 8) | (byte1))

typedef struct _CALCOMPPrivateRec
{
	int min_x;					/* Minimum x reported by calibration        */
	int max_x;					/* Maximum x                    */
	int min_y;					/* Minimum y reported by calibration        */
	int max_y;					/* Maximum y                    */
	int min_z;
	int max_z;
	int button_threshold;		/* Z > button threshold = button click */
	int pressure;		/* 0 or 1, do we need pressure info */
	int axes;
	int prox;				/* Proximity */
        int x,y;                                /* x,y values */
	int buttons;				/* button state */
	Bool button_down;			/* is the "button" currently down */
	int button_number;			/* which button to report */
	int reporting_mode;			/* TS_Raw or TS_Scaled */

	int untouch_delay;			/* Delay before reporting an untouch (in ms) */
	int report_delay;			/* Delay between touch report packets       */

	int screen_num;				/* Screen associated with the device        */
	int screen_width;			/* Width of the associated X screen     */
	int screen_height;			/* Height of the screen             */
	XISBuffer *buffer;
	unsigned char packet[CALCOMP_PACKET_SIZE];	/* packet being/just read */
	int packeti;				/* index into packet */
	unsigned char checksum;		/* Current checksum of data in assembly *
								 * buffer   */
	CALCOMPState lex_mode;
}
CALCOMPPrivateRec, *CALCOMPPrivatePtr;

/******************************************************************************
 *		Declarations
 *****************************************************************************/
#ifdef XFree86LOADER
static MODULESETUPPROTO( CalcompSetupProc );
#endif
/*static void TearDownProc (pointer p);*/
static Bool DeviceControl (DeviceIntPtr, int);
static Bool DeviceOn (DeviceIntPtr);
static Bool DeviceOff (DeviceIntPtr);
static Bool DeviceClose (DeviceIntPtr);
static Bool DeviceInit (DeviceIntPtr);
static void ReadInput (LocalDevicePtr);
static void ControlProc (DeviceIntPtr, PtrCtrl *);
static int ChangeControlProc (LocalDevicePtr, xDeviceCtl *);
static void CloseProc (LocalDevicePtr);
static int SwitchMode (ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc (LocalDevicePtr, int, int, int, int, int, int, int, int, int *, int *);
static Bool QueryHardware (int , CALCOMPPrivatePtr);
static Bool CALCOMPGetPacket (CALCOMPPrivatePtr priv);
static char * xf86CalWriteAndRead(int, char *, char *, int , int );
static InputInfoPtr CalcompPreInit(InputDriverPtr, IDevPtr, int) ;                                                
/* 
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
