/* $Id: citron.h,v 1.5 2003/04/14 08:42:27 pk Exp $
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

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/citron/citron.h,v 1.5 2003/11/03 05:11:47 tsi Exp $ */

/*
 * Based, in part, on code with the following copyright notice:
 *
 * Copyright 1999-2001 by Thomas Thanner, Citron GmbH, Germany. <support@citron.de>
 * Copyright 1999-2003 by Peter Kunzmann, Citron GmbH, Germany. <kunzmann@citron.de> *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and that
 * the name of Thomas Thanner and Citron GmbH not be used in advertising or
 * publicity pertaining to distribution of the software without specific, written
 * prior permission. Thomas Thanner and Citron GmbH makes no representations about
 * the suitability of this software for any purpose. It is provided "as is"
 * without express or implied warranty.
 *
 * THOMAS THANNER AND CITRON GMBH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL THOMAS THANNER OR CITRON GMBH BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS  ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef	_citron_H_
#define _citron_H_

/******************************************************************************
 *		Definitions
 *									structs, typedefs, #defines, enums
 *****************************************************************************/

/* CTS (Citron Touch Software) protocol constants */
#define	CTS_CTRLMIN			0x10	/* Lower end of the control character range */
#define	CTS_XON				0x11	/* Start serial transmission character */
#define	CTS_STX				0x12	/* Start of message delimiter */
#define	CTS_XOFF			0x13	/* Stop serial transmission character */
#define	CTS_ETX				0x14	/* End of message delimiter */
#define	CTS_NAK				0x15	/* Not Acknowledge, send by the IRT just before it resets itself */
#define	CTS_ESC				0x16	/* Escape character to encode non control characters with a value inside the control character range */
#define	CTS_CTRLMAX			0x16	/* Upper end of the control character range */

#define	CTS_ENCODE			0x40	/* Use this constant to encode non control character with a value inside the control character range */
#define	CTS_DECODE			(~0x40)	/* Use this constant to decode previously encoded characters. These characters are marked by a leading CTS_ESC. */

#define	CTS_MSG_MIN			0x18	/* First usable character for message and report identifiers */
#define	CTS_MSG_MAX			0x7F	/* Last usable character for message and report identifiers */

#define	CTS_DESIGNATOR_LEN	32		/* Length of the designator part in the HardwareRevision report */
#define	CTS_ASSY_LEN		16		/* Length of the ASSY part in the HardwareRevision report */
#define	CTS_OEMSTRING_LEN	256		/* Length of the OEM string */
#define CTS_FPGA_LEN		28		/* Length of the FPGA version string */
#define CTS_SENSORCOUNT_LEN	1		/* Length of sensorcount report */
#define	CTS_USERNAME_LEN	14		/* User name length without trailing zero (GetUserString) */
#define	CTS_USERSTRING_LEN	127		/* Length of the Userstring without trailing zero */

#define	CTS_MAX_HWASSY		32		/* Maximum number of hardware codeable assy numbers */

#define	CTS_MAX_POLYEDGE	64		/* Maximum number of polygonal area edges */

#define	CMD_REP_CONV		0x7f	/* Use this bit mask to convert a command into a report identifier */

#define CTS_PACKET_SIZE		(1+CTS_OEMSTRING_LEN)

/* Area operating modes */
#define	AOM_OFF				0x00	/* No coordinate messages will be generated by this area */
#define	AOM_ENTER			0x01	/* Only the area entry point will be reported in a coordinate message */
#define	AOM_TRACK			0x02	/* Every movement inside the area will be reported in a coordinate message */
#define	AOM_CONT			0x03	/* If the area is touched, coordinate messages will be created in fixed time intervals */

/* Modification flags for the area operating mode */
#define	AOF_ADDEXIT			0x01	/* Exit messages will be generated for this area */
#define	AOF_ADDCOORD		0x02	/* The coordinate of the touch point will be reported in addition to the area number */
#define	AOF_ADDPRESS		0x04	/* Pressure messages will be generated for this area */
#define	AOF_PRESSALWAYS		0x08	/* This area requires a permanent pressure to generate coordinate messages */
#define	AOF_PRESSENTER		0x10	/* This area requires only pressure to generate the first coordinate message. */
#define	AOF_PRESSLOCAL		0x20	/* This area has a locally defined pressure sensitivity, If this flag is not set, the pressure sensivity of area0 is used. */
#define	AOF_EXTENDED		0x40	/* This area must be leaved, before any other area will generate coordinate messages */
#define	AOF_ACTIVE			0x80	/* This area is active. Only active areas will generate messages. */

/* group ClearArea command parameter values */
#define	CA_ALL				0x00	/* Clear all areas on all pages */
#define	CA_PAGE				0x01	/* Clear all areas of a certain page */
#define	CA_AREA				0x02	/* Clear a single area, however area0 cannot be cleared. area0 will only be reset to its power up default state. */

/* SetTransmission command parameter values */
#define	TM_TRANSMIT		0x01		/* Enable the transmission of messages (report will be transmitted always) */
/*	TM_NONE			0x00 */		/* Disable transmission of messages and disable the XON/XOFF protocol */
#define	TM_RXDFLOW		0x10		/* Enable the XON/XOFF protocol for the transmitter (IRT will send XON/XOFF to the host) */
#define	TM_TXDFLOW		0x20		/* Enable the XON/XOFF protocol for the receiver (host will sned XON/XOFF to the IRT) */

/* SetScanning command parameter values */
#define	SC_DISABLE		0x00		/* Disable scanning */
#define	SC_ENABLE		0x01		/* Enable scanning */

/* Sleep- and Doze-Mode command parameters */
#define	TS_QUIET		0x00		/* Disable the generation of TouchSaver messages */
#define	TS_ACTIMSG		0x01		/* Enable the generation of messages on sleep- or doze-mode activation */
#define	TS_PASIMSG		0x02		/* Enable the generation of messages on sleep- or doze-mode deactivation */
#define	TS_SETOUT		0x10		/* The /GP_OUT output of the IRT will reflect the sleep- or doze-mode state, if this flag is set. */
#define	TS_ACTIVE		0x80		/* This is a read only flag to decode the current sleep- or doze-mode state in SleepModeState and DozeModeState reports. */

/* SetDualTouching command parameters */
#define	DT_IGNORE		0x00		/* Multiple touches are ignored, no DualTouchError messages will be generated */
#define	DT_ERROR		0x01		/* Multiple touches will be reported by a DualTouchError message */
#define	DT_COORD		0x02		/* The coordinate of the second touch point will be reported in a separate coordinate message. More than 2 touch points will be reported by DualTouchError messages. */

/* SetOrigin command parameters */
#define OR_TOPLEFT		0x00		/* The coordinate origin is in the top left corner of the touch */
#define OR_TOPRIGHT		0x01		/* The coordinate origin is in the top right corner of the touch */
#define OR_BOTTOMRIGHT	0x02		/* The coordinate origin is in the bottom right corner of the touch */
#define OR_BOTTOMLEFT	0x03		/* The coordinate origin is in the bottom left corner of the touch */

/* GetSignalValues command parameters */
#define	GS_NOREPORT		0x00		/* Don't report the signal values */
#define	GS_SIGNAL		0x01		/* Report the beam values as used for coordinate generation */
#define	GS_REFERENCE	0x02		/* Report the reference beam values */
#define	GS_BROKEN		0x03		/* Report the results of the broken/not broken beam detection */
#define	GS_RESCAN		0x80		/* Add this flag to rescan the touch before generating the SignalValues report */

/* GetPressureValues command parameters */
#define	GP_NOREPORT		0x00		/* Don't report the pressure values */
#define	GP_SIGNAL		0x01		/* Report the signals of the active pressure sensors */
#define	GP_REFERENCE	0x02		/* Report the signals of the calibration sensors */
#define	GP_INTERNAL		0x04		/* Report the internal state of the pressure sensitive unit */

/* SetPort/GetPort command parameters */
#define	GP_OCOUT0		0x01		/* Get/Set the /OC_OUT0 port of the IRT */
#define	GP_BIJMP		0x02		/* Get the state of the BurnIn jumper on the IRT */
#define	GP_OCSSAVER		0x04		/* Get/Set the /OC_SSAVER port of the IRT */
#define	GP_OCIN0		0x08		/* Get the state of the /OC_IN0 port of the IRT */

/* GetRevisions command parameters */
#define	GR_SYSMGR		0x01		/* Get the version number of the System Manager module */
#define	GR_HARDWARE		0x02		/* Get the version number of the Hardware module */
#define GR_PROCESS		0x04		/* Get the version number of the Process module */
#define	GR_PROTOCOL		0x08		/* Get the version number of the Protocol module */
#define	GR_HWPARAM		0x10		/* Get the version number of the Hardware Parameters module */
#define	GR_DESIGNATOR	0x20		/* Get the IRT designator and ASSY number */
#define	GR_BURNIN		0x40		/* Get the version number of the Burn-In module */
#define	GR_FPGA			0x80		/* Get the version number of the FPGA module */

/* GetErrors command parameters */
#define	GE_INITIAL			0x01	/* Report the errors detected during IRT startup */
#define	GE_DEFECTBEAMS		0x02	/* Report the beams that are marked defect and are therefore excluded from the coordinate calculations */
#define	GE_COMMUNICATION	0x04	/* Report communication errors on the serial link */
#define	GE_COMMAND			0x08	/* Report command errors (invalid parameters, unknown commands, ...) */
#define	GE_CLEAR			0x80	/* Add this flag to clear the errors after reporting */

/* GetHardware command parameters */
#define	GH_BEAMCOUNT		0x01	/* Report the number of x and y beams */
#define	GH_SENSORCOUNT		0x02	/* Report the number of pressure sensors */
#define	GH_PERIPHERALS		0x04	/* Report a bit vector that identifies all assembled peripherals on the IRT */

/* GetHWVersion command parameters */
#define	HV_SSNO			0x01	/* Report the silicon serial number */
#define	HV_ASSY			0x02	/* Report the hard wired assembly number */
#define	HV_FPGA			0x04	/* Report the FPGA version string */

/* InitialError decoding bit masks */
#define	IE_SMCHKSUM		0x00000001UL	/* The system manager module has a checksum error */
#define	IE_SMINIT		0x00000002UL	/* The system manager module reported an error during initialisation */
#define	IE_HWCHKSUM		0x00000004UL	/* The hardware module has a checksum error */
#define	IE_HWINIT		0x00000008UL	/* The hardware module reported an error during initialisation */
#define	IE_PCCHKSUM		0x00000010UL	/* The process module has a checksum error */
#define	IE_PCINIT		0x00000020UL	/* The process module reported an error during initialisation */
#define	IE_PTCHKSUM		0x00000040UL	/* The protocol module has a checksum error */
#define	IE_PTINIT		0x00000080UL	/* The protocol module reported an error during initialisation */
#define	IE_HW_BEAMS		0x00000100UL	/* There were broken beams during hardware initialisation */
#define	IE_HW_PSU		0x00000200UL	/* There pressure sensitive unit could not be initialised */
#define	IE_HW_CPU		0x00000400UL	/* There was an error in the CPU core detected during startup */
#define	IE_HW_IRAM		0x00000800UL	/* There was an error in the initial internal ram check */
#define	IE_HW_XRAM		0x00001000UL	/* There was an error in the initial external ram check */
#define	IE_BICHK		0x00002000UL	/* The burnin module has a checksum error */
#define	IE_BIINIT		0x00004000UL	/* The burnin module reported an error during initialisation */
#define	IE_FPGACHK		0x00008000UL	/* The fpga module has a checksum error */
#define	IE_HWPCHK		0x00010000UL	/* The hardware parameter module has a checksum error */

/* CommunicationError decoding bit masks */
#define	CE_DC2GTDC4		0x00000001UL	/* There were more CTS_STX received than CTS_ETX */
#define	CE_DC4GTDC2		0x00000002UL	/* There were more CTS_ETX received than CTS_STX */
#define	CE_UNXNONCTRL	0x00000004UL	/* Non control character received outside a CTS_STX/CTS_ETX sequence */
#define	CE_UNXCONTROL	0x00000008UL	/* Unexpected control character received */
#define	CE_OVERFLOW		0x00000010UL	/* The hardware receiver buffer had an overflow */
#define	CE_FRAMING		0x00000020UL	/* There were characters with framing errors received */
#define	CE_PARITY		0x00000040UL	/* There were characters with invalid parity received */
#define	CE_XOFFTO		0x00000080UL	/* No XON was received within the defined timeout after a XOFF */
#define	CE_CMDOVER		0x00000100UL	/* The command buffer had an overflow */
#define	CE_RCVROVER		0x00000200UL	/* The receiver ring buffer had an overflow */

/* CommandError decoding bit masks */
#define	CE_UNKNOWN		0x00000001UL	/* Unknown command received */
#define	CE_PARAMCNT		0x00000002UL	/* Too much or too less parameters received */
#define	CE_RANGE		0x00000004UL	/* One or more parameters were out of range */

/* Peripheral indentification bit masks */
#define	PERI_OCOUT0		0x00000001UL	/* The /OC_OUT0 port is available */
#define	PERI_BURNIN		0x00000002UL	/* The BurnIn jumper is available */
#define	PERI_GP_OUT		0x00000004UL	/* The /GP_OUT port is available */
#define	PERI_OCPWM		0x00000008UL	/* The /OC_PWM port is available */
#define	PERI_SPEAKER	0x00000010UL	/* The speaker port is available */
#define	PERI_GP_IN		0x00000020UL	/* The /GP_IN port is available */
#define	PERI_RUNLED		0x00000040UL	/* The red blinking indication LED is available */

/* SaveSetup/ReadSetup command parameters */
#define	SUP_SERIAL		0x01			/* Save/Read the serial port setup */
#define	SUP_MACRO		0x02			/* Save/Read the macro definitions */
#define	SUP_AREAS		0x04			/* Save/Read the area definitions */
#define	SUP_PERI		0x08			/* Save/Read the peripheral settings */
#define	SUP_COORD		0x10			/* Save/Read the coordinate settings */

/* IRT initialisation modes for <f cts_Connect> */

#define	MODE_A				0x7b		/* Initialise the IRT to AFE-Mode A emulation */
#define	MODE_B				0x3c		/* Initialise the IRT to Carroll emulation */
#define	MODE_C				0x6f		/* Another entry point for Mode-D (for backwards compatibility) */
#define	MODE_D				0x81		/* Initialise the IRT to the CTS protocol */

/* Command is for the driver */
#define DRIVCOMM			0x00		/* Command for driver */

/* Command Identifiers for the driver */
#define D_SETCLICKMODE		0x00
#define D_BEEP				0x01
#define D_SETBEEP			0x02
#define D_DEBUG				0x03
#define D_ENTERCOUNT		0x04
#define D_ZENTERCOUNT		0x05
#define D_PWMADJ			0x06


/* Message identifiers */
#define	R_DUALTOUCHERROR	0x18		/* Invalid multiple touches are detected */
#define	R_COORD				0x19		/* Regular coordinate report */
#define	R_EXIT				0x1a		/* An area was leaved */
#define	R_PRESSURE			0x1b		/* An area was pressed or released */
#define PRESS_BELOW			0x00		/* Pressure below a certain threshold */
#define PRESS_EXCEED		0x01		/* Pressure higher than a certain threshold */

#define	R_SLEEPMODE			0x1c		/* The sleep-mode was activated or deactivated */
#define	R_DOZEMODE			0x1d		/* The doze-mode was activated or deactivated */

/* Special report identifiers */
#define	R_POLYAREADEF		0x2a
#define R_IDLE				0x34
#define	R_SCANTIMING		0x56
#define R_LOCKZ				0x52		/* LockZ timings report */
#define R_USERSTRING		0x66		/* Userstring report */

/* Command identifiers */
#define	C_SOFTRESET			0x80
#define	C_RESETCTS			0x81
#define	C_SAVESETUP			0x83
#define	C_DESTROYSETUP		0x84
#define	C_SETSCANTIMING		0x85
#define	C_GETSCANTIMING		0x86

#define	C_CLEARAREA			0xa0
#define	C_DEFINEAREA		0xa1
#define	C_GETAREADEF		0xa2
#define	C_GETAREAPAGE		0xa3
#define	C_GETFREEAREASPACE	0xa4
#define	C_SELECTAREAPAGE	0xa5
#define	C_SETAREASTATE		0xa6
#define	C_SETAREAMODE		0xa7
#define	C_SETAREAFLAGS		0xa8
#define	C_SETAREAPRESSURE	0xa9
#define	C_DEFINEPOLYAREA	0xaa

#define	C_GETERRORS			0xb0
#define	C_GETHARDWARE		0xb1
#define	C_GETREVISIONS		0xb2
#define	C_GETSETUP			0xb3
#define	C_GETSINGLEMESSAGE	0xb4
#define	C_GETSINGLESCAN		0xb5
#define	C_GETSIGNALVALUES	0xb6
#define	C_GETPRESSUREVALUES	0xb7
#define	C_GETOEMSTRING		0xb8
#define	C_GETHWVERSIONS		0xb9
#define	C_BIGETFIRSTSESSION	0xba
#define	C_BIGETNEXTSESSION	0xbb
#define	C_BIGETRECORD		0xbc
#define	C_BIERASEDATA		0xbd
#define	C_BIGETTICKUNIT		0xbe

#define	C_GETBEAMMINMAX		0xc0
#define	C_GETBEAMTIMEOUT	0xc1
#define	C_GETCONTTIME		0xc2
#define	C_GETDUALTOUCHING	0xc3
#define	C_GETORIGIN			0xc4
#define	C_GETRESOLUTION		0xc5
#define	C_GETSCANNING		0xc6
#define	C_GETTRANSMISSION	0xc7
#define	C_SETBEAMMINMAX		0xc8
#define	C_SETBEAMTIMEOUT	0xc9
#define	C_SETCONTTIME		0xca
#define	C_SETDUALTOUCHING	0xcb
#define	C_SETORIGIN			0xcc
#define	C_SETRESOLUTION		0xcd
#define	C_SETSCANNING		0xce
#define	C_SETTRANSMISSION	0xcf

#define	C_GETTOUCHTIME		0xd0
#define	C_SETTOUCHTIME		0xd1
#define C_GETLOCKZ			0xd2
#define C_SETLOCKZ			0xd3

#define	C_CLEARMACRO		0xe0
#define	C_ENDMACRORECORD	0xe1
#define	C_EXECMACRO			0xe2
#define	C_GETFREEMACROSPACE	0xe3
#define	C_STARTMACRORECORD	0xe5
#define	C_GETUSERSTRING		0xe6
#define C_SETUSERSTRING		0xe7

#define	C_GETPORT			0xf0
#define	C_GETPWM			0xf1
#define	C_GETSOUND			0xf2
#define	C_GETSLEEPMODE		0xf3
#define	C_SETPORT			0xf4
#define	C_SETPWM			0xf5
#define	C_SETSOUND			0xf6
#define	C_SETSLEEPMODE		0xf7
#define C_GETDOZEMODE		0xf8
#define	C_SETDOZEMODE		0xf9
#define C_SETPWMFREQ		0xfa
#define C_GETPWMFREQ		0xfb

/* touch states */
#define	CIT_TOUCHED			0x01
#define	CIT_PRESSED			0x02
#define	CIT_BUTTON			0x04

/* click modes */
#define	CM_ENTER			1
#define	CM_DUAL				2
#define	CM_DUALEXIT			3
#define	CM_ZPRESS			4
#define CM_ZPRESSEXIT		5

#define	MAX_DUAL_TOUCH_COUNT	2

#define NO_CLICK_MODE		255		/* no click mode set in xf86Config */

/* command structure for Feedback Functions */
typedef struct {
	unsigned char par[3];	/* byte parameter */
	char packet;			/* packet number 00 - 7F */	
} COMMAND;


/* Data exchange with driver (Driver Data Structure) */
#define MAX_BYTES_TO_TRANSFER	0x20
#define LAST_PACKET				0x7f

typedef struct {
		short curbyte;			/* current byte number */
		short numbytes;			/* number of bytes to transmit */
		short packet;			/* packet number */
		unsigned char data[MAX_BYTES_TO_TRANSFER];	/* pointer to data area */
} CitronDDS;





/*****************************************************************************
 *	X-Header
 ****************************************************************************/

#define X_CITOUCH	 " CiTouch: "
const char *CI_PROBED	= {"(--)" X_CITOUCH}; /* Value was probed */
const char *CI_CONFIG	= {"(**)" X_CITOUCH}; /* Value was given in the config file */
const char *CI_DEFAULT	= {"(==)" X_CITOUCH}; /* Value is a default */
const char *CI_CMDLINE	= {"(++)" X_CITOUCH}; /* Value was given on the command line */
const char *CI_NOTICE	= {"(!!)" X_CITOUCH}; /* Notice */
const char *CI_INFO		= {"(II)" X_CITOUCH}; /* Informational message */
const char *CI_WARNING	= {"(WW)" X_CITOUCH}; /* Warning message */
const char *CI_ERROR	= {"(EE)" X_CITOUCH}; /* Error message */
const char *CI_UNKNOWN	= {"(?\?)" X_CITOUCH}; /* Unknown message */



/*****************************************************************************
 *	macros
 ****************************************************************************/
#define millisleep(ms) xf86usleep((ms) * 1000)

#define	HIBYTE(x)	( (unsigned char) ( (x) >> 8 ) )
#define	LOBYTE(x)	( (unsigned char) ( (x) & 0xff ) )


/*****************************************************************************
 *	typedefs
 ****************************************************************************/
typedef enum
{
	cit_idle, cit_getID, cit_collect, cit_escape
}
cit_State;	/* Citron Infrared Touch Driver State */

typedef struct _cit_privateRec
{
	int min_x;					/* Minimum x reported by calibration        */
	int max_x;					/* Maximum x                  				*/
	int min_y;					/* Minimum y reported by calibration        */
	int max_y;					/* Maximum y                    			*/
	int button_threshold;		/* Z > button threshold = button click 		*/
	int axes;
	int	dual_touch_count;		/* counter for dual touch error events 		*/
	int click_mode;				/* one of the CM_ constants 				*/
	int button_number;			/* which button to report 					*/
	int reporting_mode;			/* TS_Raw or TS_Scaled 						*/
	int screen_num;				/* Screen associated with the device        */
	int screen_width;			/* Width of the associated X screen     	*/
	int screen_height;			/* Height of the screen             		*/
	int packeti;					/* index into packet 					*/
	int raw_x;						/* Raw Coordinates */
	int raw_y;
	int	sleep_mode;					/* sleep mode: 0x00=no message, 0x01=m at activation, 0x02=m at deactivation, */
									/*             0x03= message at act. + deact., 0x10= GP_OUT set */
	int	sleep_time_act;				/* time until touchsaver gets activate 	*/
	int sleep_time_scan;			/* time interval between two scans		*/
	int	pwm_sleep;					/* PWM duty cycle during touch saver mode */
	int	pwm_active;					/* PWM duty cycle during regular operation */
	int pwm_freq;					/* PWM base frequency */
	int	pwm_src;					/* Source for PWM adjust BL_TDK or BL_AC */
	int pwm_dst;					/* Destination for PWM adjust BL_TDK or BL_AC */
	int	state;
/* additional parameters */
	int last_x;						/* last cooked data */
	int last_y;
	int	doze_mode;					/* doze mode: 0x00=no message, 0x01=m at activation, 0x02=m at deactivation, */
									/*            0x03= message at act. + deact., 0x10= GP_OUT set */
	int	doze_time_act;				/* time until touchsaver gets activate 	*/
	int doze_time_scan;				/* time interval between scans			*/
	int origin;						/* Coordinates origin 					*/
	int delta_x;					/* Delta x - if coordinate changed less than delta x no motion event */
	int delta_y;
	int beep;						/* 0= no beep, 1=beep enabled 			*/
	int press_vol;					/* volume of beep (press event) 		*/
	int press_pitch;				/* pitch of beep (press event)	 		*/
	int press_dur;					/* length of beep in 10ms (press event)	*/
	int rel_vol;					/* volume of beep (release event) 		*/
	int rel_pitch;					/* pitch of beep (release event) 		*/
	int rel_dur;					/* length of beep in 10ms (release event) */
	int beam_timeout;				/* Beam timeout 0= no beam timeout		*/
	int touch_time;					/* minimum time span for a valid interruption */
	int	enter_touched;				/* button is down due to an enter event */
	int enter_count;				/* number of jumed coord reports before a ButtonPress event is sent */
	int enter_count_no_Z;			/* number of jumped over coords before ButtonPress event in not pressure sensitive mode */
	int enter_count_Z;				/* number of jumped over coords before ButtonPress event in pressure sensitive mode */
	int max_dual_count;				/* number of jumed dualtouch error reports before a ButtonPress event is sent */
	int dual_flg;					/* Flag set if dualtouch error report is received , reset by counter */
	int raw_min_x;					/* min x,y max x,y value accumulated over the whole session */
	int query_state;				/* test if query was already started */
	int raw_max_x;
	int raw_min_y;
	int raw_max_y;
	int	pressure_sensors;			/* number of pressure sensors */
	int lockz_enter_time;			/* Minimum duration of AreaPressEnter state before a PressEnter event is issued. Steps: 10ms */
	int lockz_exit_time;			/* Minimum duration of AreaPressExit state before a PressEnter event is issued. Steps: 10ms */
	int lockz_lock_time;			/* Minimum gap between PressExit and PressEnter event. Steps: 10ms */ 
	

#define MAX_TIMER	2							/* Max. concurrent timers */
#define FAKE_TIMER	0							/* Timer for faked exit message */
#define SV_TIMER	1							/* Supervision timer for command timeout suopervision */
	OsTimerPtr timer_ptr[MAX_TIMER];			/* Timer for general purposes */
	CARD32 timer_val1[MAX_TIMER];				/* Timer 1st delay */
	CARD32 timer_val2[MAX_TIMER];				/* Timer second delay */
	OsTimerCallback timer_callback[MAX_TIMER];	/* timer callback routine	*/
	int fake_exit;					/* tell the ReadInput function there is a exit message (from timer) */
/* end additional parameters */

	LocalDevicePtr local;			/* Pointer to local device */
	Bool button_down;				/* is the "button" currently down 			*/
	Bool proximity;
	cit_State lex_mode;
	XISBuffer *buffer;
	unsigned char packet[CTS_PACKET_SIZE];	/* packet being/just read 		*/	
    CitronDDS dds;					/* Structure for Byte transfer to the driver via LedFeedbackControl */
}
cit_PrivateRec, *cit_PrivatePtr;




#define	BL_TDK	0			/* TDK Inverter */
#define BL_AC	1			/* Applied Concepts inverter */
#define BL_MAX	1			/* Max. Tables */

const unsigned short cit_bright_adjust[2] [256] =
{
	{1,1,2,2,2,3,3,3,4,4,5,5,6,7,8,10,11,13,14,16,18,20,23,25,27,30,33,36,39,42,45,49,52,56,60,64,68,72,76,
	80,85,89,94,99,104,109,114,119,124,130,135,141,147,153,159,165,171,177,183,190,196,203,209,216,223,230,
	237,244,251,258,265,273,280,288,295,303,311,319,326,334,342,350,359,367,375,383,392,400,408,417,426,434,
	443,452,460,469,478,487,496,505,514,523,532,541,550,560,569,578,587,597,606,615,625,634,644,653,663,672,
	682,691,701,710,720,730,739,749,758,768,778,787,797,807,816,826,836,845,855,865,874,884,893,903,913,922,
	932,941,951,960,970,979,989,998,1008,1017,1027,1036,1045,1054,1064,1073,1082,1091,1100,1109,1119,1128,
	1136,1145,1154,1163,1172,1181,1189,1198,1207,1215,1224,1232,1240,1249,1257,1265,1273,1281,1289,1297,1305,
	1313,1321,1329,1336,1344,1351,1359,1366,1373,1380,1387,1395,1401,1408,1415,1422,1428,1435,1441,1448,1454,
	1460,1466,1472,1478,1484,1489,1495,1500,1506,1511,1516,1521,1526,1531,1536,1541,1545,1549,1554,1558,1562,
	1566,1570,1574,1577,1581,1584,1587,1590,1593,1596,1599,1601,1604,1606,1608,1610,1612,1614,1616,1617,1618,
	1620,1621,1622,1622,1623,1623},		/* TDK */

	{113,117,122,126,131,135,140,144,149,154,159,164,168,173,178,183,189,194,199,204,210,215,220,226,231,237,
	243,248,254,260,265,271,277,283,289,295,301,307,313,320,326,332,338,345,351,357,364,370,377,383,390,397,
	403,410,417,423,430,437,444,451,458,465,472,479,486,493,500,507,514,521,529,536,543,550,558,565,572,580,
	587,594,602,609,617,624,632,639,647,655,662,670,677,685,693,700,708,716,723,731,739,747,755,762,770,778,
	786,794,801,809,817,825,833,841,849,856,864,872,880,888,896,904,912,920,928,935,943,951,959,967,975,983,
	991,999,1007,1015,1023,1030,1038,1046,1054,1062,1070,1078,1086,1093,1101,1109,1117,1125,1133,1140,1148,
	1156,1164,1171,1179,1187,1194,1202,1210,1217,1225,1233,1240,1248,1256,1263,1271,1278,1286,1293,1301,1308,
	1315,1323,1330,1337,1345,1352,1359,1367,1374,1381,1388,1395,1402,1409,1417,1424,1431,1438,1444,1451,1458,
	1465,1472,1479,1485,1492,1499,1506,1512,1519,1525,1532,1538,1545,1551,1557,1564,1570,1576,1582,1589,1595,
	1601,1607,1613,1619,1625,1631,1636,1642,1648,1654,1659,1665,1670,1676,1681,1687,1692,1697,1703,1708,1713,
	1718,1723,1728,1733,1738,1743,1748,1752,1757,1762,1766,1771,1775,1779,1784,1788,1792,1796,1801,1805,1809}, /* AC */
};



/******************************************************************************
 *		Declarations
 *****************************************************************************/
/*extern void ModuleInit (pointer *, INT32 *);*/
#ifdef XFree86LOADER
static MODULESETUPPROTO (SetupProc);
static void TearDownProc (pointer p);
/*static void *SetupProc (XF86OptionPtr, int *, int *);*/
#endif
static Bool DeviceControl (DeviceIntPtr def, int mode);
static Bool DeviceOn (DeviceIntPtr);
static Bool DeviceOff (DeviceIntPtr);
static Bool DeviceClose (DeviceIntPtr);
static Bool DeviceInit (DeviceIntPtr);
static void ReadInput (LocalDevicePtr);
static int ControlProc (LocalDevicePtr, xDeviceCtl *);
static void CloseProc (LocalDevicePtr);
static int SwitchMode (ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc (LocalDevicePtr, int, int, int, int, int, int, int, int, int *, int *);
static Bool QueryHardware (LocalDevicePtr, int *, int *);
static Bool cit_GetPacket (cit_PrivatePtr);
static void cit_Flush(cit_PrivatePtr);
static void cit_SendCommand(XISBuffer *, unsigned char, int, ...);
static Bool cit_GetInitialErrors(cit_PrivatePtr);
static Bool cit_GetDefectiveBeams(cit_PrivatePtr);
static Bool cit_GetDesignator(cit_PrivatePtr);
static Bool cit_GetPressureSensors(cit_PrivatePtr);
static Bool cit_GetRevision(cit_PrivatePtr, int);
static void cit_ProcessPacket(cit_PrivatePtr);
static void cit_Beep(cit_PrivatePtr priv, int press);
static void cit_SetBlockDuration (cit_PrivatePtr priv, int block_duration);
static void cit_ReinitSerial(cit_PrivatePtr priv);
static int cit_ZPress(cit_PrivatePtr priv);
static void cit_SetEnterCount(cit_PrivatePtr priv);
static void cit_SendPWM(cit_PrivatePtr priv);
static void cit_SendPWMFreq(cit_PrivatePtr priv);
static int cit_AdjustBright(cit_PrivatePtr priv, int val);

#ifdef CIT_TIM
static void cit_StartTimer(cit_PrivatePtr priv, int nr);
static void cit_CloseTimer(cit_PrivatePtr priv, int nr);
static CARD32 cit_SuperVisionTimer(OsTimerPtr timer, CARD32 now, pointer arg);
static CARD32 cit_DualTouchTimer(OsTimerPtr timer, CARD32 now, pointer arg);
#endif



/*
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif
