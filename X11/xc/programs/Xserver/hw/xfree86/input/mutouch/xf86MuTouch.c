/*
 * Copyright 1996, 1999 by Patrick Lecoanet, France. <lecoanet@cena.dgac.fr>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Patrick  Lecoanet not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Patrick Lecoanet   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * PATRICK LECOANET DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT  SHALL PATRICK LECOANET BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/mutouch/xf86MuTouch.c,v 1.14 2001/08/17 13:27:56 dawes Exp $ */

/*
 *******************************************************************************
 *******************************************************************************
 *
 * This driver is able to deal with MicroTouch serial controllers using
 * firmware set 2. This includes (but may not be limited to) Serial/SMT3
 * and TouchPen controllers. The only data format supported is Mode Tablet
 * as it is the only available with these controllers. Anyway this is not a big
 * lost as it is the most efficient (by far) and is supported by all controllers.
 *
 * The code has been lifted from the Elographics driver in xf86Elo.c.
 *
 * ThruGlass specific addition 1999 by Andreas Micklei, Germany.
 * <micklei@fokus.gmd.de>
 *
 *******************************************************************************
 *******************************************************************************
 */

#include "xf86Version.h"
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4
#endif

#ifdef XFREE86_V4
#include "misc.h"
#include "xf86.h"
#if !defined(DGUX)
#include "xf86_ansic.h"
#endif
#include "xf86_OSproc.h"
#include "xf86Xinput.h"
#include "exevents.h"

#ifdef XFree86LOADER
#include "xf86Module.h"
#endif

#else /* XFREE86_V4 */

#include "Xos.h"
#include <signal.h>
#include <stdio.h>

#define	 NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "XI.h"
#include "XIproto.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Procs.h"
#include "xf86_OSlib.h"
#include "xf86_Config.h"
#include "xf86Xinput.h"

#include "os.h"
#include "osdep.h"
#include "exevents.h"

#include "extnsionst.h"
#include "extinit.h"

#endif /* XFREE86_V4 */


#ifndef XFREE86_V4
/*
 ***************************************************************************
 *
 * Configuration descriptor.
 *
 ***************************************************************************
 */
#define FINGER_SECTION_NAME	"microtouchfinger"
#define STYLUS_SECTION_NAME	"microtouchstylus"

#define PORT		1
#define DEVICENAME	2
#define SCREEN_NO	3
#define MAXX		5
#define MAXY		6
#define MINX		7
#define MINY		8
#define DEBUG_LEVEL	9
#define HISTORY_SIZE	10
#define LINK_SPEED	11
#define ALWAYS_CORE	12
#define SWAP_AXES	13
#define FREQUENCY	14
#define PORTRAIT_MODE	15

static SymTabRec MuTTab[] = {
  { ENDSUBSECTION,     "endsubsection" },
  { PORT,              "port" },
  { DEVICENAME,        "devicename" },
  { SCREEN_NO,	       "screenno" },
  { MAXX,              "maximumxposition" },
  { MAXY,              "maximumyposition" },
  { MINX,              "minimumxposition" },
  { MINY,              "minimumyposition" },
  { DEBUG_LEVEL,       "debuglevel" },
  { HISTORY_SIZE,      "historysize" },
  { LINK_SPEED,        "linkspeed" },
  { ALWAYS_CORE,       "alwayscore" },
  { SWAP_AXES,	       "swapxy" },
  { FREQUENCY,         "frequency" },
  { PORTRAIT_MODE,     "portraitmode" },
  { -1,                "" },
};

#define LS300		1
#define LS1200		2
#define LS2400		3
#define LS9600		4
#define LS19200		5

static SymTabRec LinkSpeedTab[] = {
  { LS300,	"b300" },
  { LS1200,	"b1200" },
  { LS2400,	"b2400" },
  { LS9600,	"b9600" },
  { LS19200,	"b19200" }
};


typedef struct {
  int	speed;
} LinkParameterStruct;
  
static LinkParameterStruct	LinkSpeedValues[] = {
  { B300 },
  { B1200 },
  { B2400 },
  { B9600 },
  { B19200 }
};
#endif /* XFREE86_V4 */


/*
 ***************************************************************************
 *
 * Default constants.
 *
 ***************************************************************************
 */
#define MuT_MAX_TRIALS		5	/* Number of timeouts waiting for a	*/
					/* pending reply.			*/
#define MuT_MAX_WAIT		300000	/* Max wait time for a reply (microsec) */
#define MuT_LINK_SPEED		B9600	/* 9600 Bauds				*/
#define MuT_PORT		"/dev/ttyS1"

#define DEFAULT_MAX_X		3000
#define DEFAULT_MIN_X		600
#define DEFAULT_MAX_Y		3000
#define DEFAULT_MIN_Y		600

#define XI_FINGER		"FINGER"	/* X device name for the finger device	*/
#define XI_STYLUS		"STYLUS"	/* X device name for the stylus device	*/


/*
 ***************************************************************************
 *
 * Protocol constants.
 *
 ***************************************************************************
 */
#define MuT_REPORT_SIZE		5	/* Size of a report packet.			*/
#define MuT_BUFFER_SIZE		256	/* Size of input buffer.			*/
#define MuT_PACKET_SIZE		10	/* Maximum size of a command/reply *including*	*/
					/* the leading and trailing bytes.		*/

#define MuT_LEAD_BYTE		0x01	/* First byte of a command/reply packet.	*/
#define MuT_TRAIL_BYTE		0x0D	/* Last byte of a command/reply packet.		*/

/*
 * Commands.
 */
#define MuT_RESET		"R"	/* Reset the controller.			*/
#define MuT_RESTORE_DEFAULTS	"RD"	/* Restore factory settings.			*/
#define MuT_FORMAT_TABLET	"FT"	/* Report events using tablet format.		*/
#define MuT_FORMAT_RAW		"FR"	/* Report events in raw mode (no corrections).	*/
#define MuT_CALIBRATE_RAW	"CR"	/* Calibration in raw mode.			*/ 
#define MuT_CALIBRATE_EXT	"CX"	/* Calibration in extended mode (cooked).	*/
#define MuT_OUTPUT_IDENT	"OI"	/* Ask some infos about the firmware.		*/
#define MuT_UNIT_TYPE		"UT"	/* Ask some more infos about the firmware.	*/
#define MuT_FINGER_ONLY		"FO"	/* Send reports only if a finger is touching.	*/
#define MuT_PEN_ONLY		"PO"	/* Send reports only if a pen is touching.	*/
#define MuT_PEN_FINGER		"PF"	/* Always send reports.				*/
#define MuT_MODE_STREAM		"MS"	/* Receive reports in stream mode (continuous).	*/

/*
 * Additional ThruGlass-Specific Commands
 */
#define MuT_MODE_NOISE		"MN"	/* Stream noise data packets.			*/
#define MuT_MODE_EXTENDED	"MX"	/* Send firmware algorithm data on press.	*/
#define MuT_SET_CREEP		"SC"	/* Set/show base update rates.			*/
#define MuT_SET_SENSITIVITY	"SS"	/* Set/show touch algorithm parameters.		*/
#define MuT_SET_FREQUENCY	"SF"	/* Set/show frequency.				*/
#define MuT_SET_PHASE		"SP"	/* Set/show phase.				*/
#define MuT_SET_TYPE		"ST"	/* Set/show controller & screen orientation.	*/
#define MuT_SET_CORRECTION_X	"SCX"	/* Set/show X depth correction parameters.	*/
#define MuT_SET_CORRECTION_Y	"SCY"	/* Set/show Y depth correction parameters.	*/
#define MuT_FORMAT_RAW_ASCII	"FRA"	/* Show 16 sensor channel values.		*/
#define MuT_FORMAT_BASE_ASCII	"FBA"	/* Show 16 base values.				*/
#define MuT_FORMAT_DEPTH_ASCII	"FZA"	/* Show 16 press depth values.			*/
#define MuT_NOISE_ASCII		"NOA"	/* Show noise data.				*/

/*
 * Command reply values.
 */
#define MuT_OK			'0'	/* Report success.				*/
#define MuT_ERROR		'1'	/* Report error.				*/

/*
 * Offsets in status byte of touch and motion reports.
 */
#define MuT_SW1			0x01	/* State of switch 1 (TouchPen only).		*/
#define MuT_SW2			0x02	/* State of switch 2 (TouchPen only).		*/
#define MuT_WHICH_DEVICE	0x20	/* If report is from pen or from finger.	*/
#define MuT_CONTACT		0x40	/* Report touch/untouch with touchscreen.	*/

/*
 * Identity and friends.
 */
#define MuT_TOUCH_PEN_IDENT	"P5"
#define MuT_SMT3_IDENT		"Q1"
#define MuT_THRU_GLASS_IDENT	"T1"


/*
 ***************************************************************************
 *
 * Usefull macros.
 *
 ***************************************************************************
 */
#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 7) | (byte1))
#define SYSCALL(call)			while(((call) == -1) && (errno == EINTR))

/* This one is handy, thanx Fred ! */
#ifdef DBG
#undef DBG
#endif
#ifdef DEBUG
#undef DEBUG
#endif

static int      debug_level = 0;
#define DEBUG 1
#if DEBUG
#define DBG(lvl, f) {if ((lvl) <= debug_level) f;}
#else
#define DBG(lvl, f)
#endif

#ifdef XFREE86_V4
#undef SYSCALL
#undef read
#undef write
#undef close
#define SYSCALL(call) call
#define read(fd, ptr, num) xf86ReadSerial(fd, ptr, num)
#define write(fd, ptr, num) xf86WriteSerial(fd, ptr, num)
#define close(fd) xf86CloseSerial(fd)
#endif


/*
 ***************************************************************************
 *
 * Device private records.
 *
 ***************************************************************************
 */
#define FINGER_ID		1
#define STYLUS_ID		2
#define DEVICE_ID(flags)	((flags) & 0x03)
  
typedef struct _MuTPrivateRec {
  char			*input_dev;	/* The touchscreen input tty			*/
  int			min_x;		/* Minimum x reported by calibration		*/
  int			max_x;		/* Maximum x					*/
  int			min_y;		/* Minimum y reported by calibration		*/
  int			max_y;		/* Maximum y					*/
#ifndef XFREE86_V4
  int			link_speed;	/* Speed of the RS232 link connecting the ts.	*/
#endif
  int			frequency;	/* Frequency for ThruGlass			*/
  int			screen_no;	/* Screen associated with the device		*/
  int			screen_width;	/* Width of the associated X screen		*/
  int			screen_height;	/* Height of the screen				*/
  Bool			inited;		/* The controller has already been configured ?	*/
  char			state;		/* Current state of report flags.		*/
  int			num_old_bytes;	/* Number of bytes left in receive buffer.	*/
  LocalDevicePtr	finger;		/* Finger device ptr associated with the hw.	*/
  LocalDevicePtr	stylus;		/* Stylus device ptr associated with the hw.	*/
  int			swap_axes;	/* Swap X an Y axes if != 0 */
  unsigned char		rec_buf[MuT_BUFFER_SIZE]; /* Receive buffer.			*/
} MuTPrivateRec, *MuTPrivatePtr;


#ifndef XFREE86_V4
/*
 ***************************************************************************
 *
 * xf86MuTConfig --
 *	Configure the driver from the configuration data.
 *
 ***************************************************************************
 */
static Bool
xf86MuTConfig(LocalDevicePtr    *array,
	      int               inx,
	      int               max,
	      LexPtr            val)
{
  LocalDevicePtr        local = array[inx];
  MuTPrivatePtr         priv = (MuTPrivatePtr)(local->private);
  int                   token;
  int			portrait=0;
  
  while ((token = xf86GetToken(MuTTab)) != ENDSUBSECTION) {
    switch(token) {
      
      case PORT:
	if (xf86GetToken(NULL) != STRING)
	  xf86ConfigError("MicroTouch input port expected");
	else {
	  /*
	   * See if another X device share the same physical
	   * device and set up the links so that they share
	   * the same private structure (the one that controls
	   * the physical device).
	   */
	  int	i;
	  for (i = 0; i < max; i++) {
	    if (i == inx)
	      continue;
	    if (array[i]->device_config == xf86MuTConfig &&
		(strcmp(((MuTPrivatePtr) array[i]->private)->input_dev,
			val->str) == 0)) {
	      ErrorF("%s MicroTouch config detected a device share between %s and %s\n",
		     XCONFIG_GIVEN, local->name, array[i]->name);
	      xfree(priv);
	      priv = local->private = array[i]->private;
	      switch (DEVICE_ID(local->private_flags)) {
	      case FINGER_ID:
		priv->finger = local;
		break;
	      case STYLUS_ID:
		priv->stylus = local;
		break;
	      }
	      break;
	    }
	  }
	  if (i == max) {
	    priv->input_dev = strdup(val->str);	
	    if (xf86Verbose)
	      ErrorF("%s MicroTouch %s input port: %s\n",
		     XCONFIG_GIVEN, local->name, priv->input_dev);
	  }
	}
	break;
	
    case DEVICENAME:
      if (xf86GetToken(NULL) != STRING)
	xf86ConfigError("MicroTouch device name expected");
      local->name = strdup(val->str);
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s X device name: %s\n",
	       XCONFIG_GIVEN, local->name, local->name);
      break;
      
    case SCREEN_NO:
      if (xf86GetToken(NULL) != NUMBER)
	xf86ConfigError("MicroTouch screen number expected");
      priv->screen_no = val->num;
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s associated screen: %d\n",
	       XCONFIG_GIVEN, local->name, priv->screen_no);      
      break;
      
    case LINK_SPEED:
      {
	int	ltoken = xf86GetToken(LinkSpeedTab);
	if (ltoken == EOF ||
	    ltoken == STRING ||
	    ltoken == NUMBER)
	  xf86ConfigError("MicroTouch link speed expected");
	priv->link_speed = LinkSpeedValues[ltoken-1].speed;
	if (xf86Verbose)
	  ErrorF("%s MicroTouch %s link speed: %s bps\n",
		 XCONFIG_GIVEN, local->name, (LinkSpeedTab[ltoken-1].name)+1);
      }
      break;
    
    case MAXX:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("MicroTouch maximum x position expected");
      priv->max_x = val->num;
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s maximum x position: %d\n",
	       XCONFIG_GIVEN, local->name, priv->max_x);      
     break;
      
    case MAXY:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("MicroTouch maximum y position expected");
      priv->max_y = val->num;
      if (xf86Verbose)
	ErrorF("%s Microtouch %s maximum y position: %d\n",
	       XCONFIG_GIVEN, local->name, priv->max_y);      
     break;
      
    case MINX:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("MicroTouch minimum x position expected");
      priv->min_x = val->num;
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s minimum x position: %d\n",
	       XCONFIG_GIVEN, local->name, priv->min_x);      
     break;
      
    case MINY:
      if (xf86GetToken(NULL) != NUMBER)
        xf86ConfigError("MicroTouch minimum y position expected");
      priv->min_y = val->num;
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s minimum y position: %d\n",
	       XCONFIG_GIVEN, local->name, priv->min_y);      
     break;
      
    case DEBUG_LEVEL:
	if (xf86GetToken(NULL) != NUMBER)
	    xf86ConfigError("MicroTouch driver debug expected");
	debug_level = val->num;
	if (xf86Verbose) {
#if DEBUG
	    ErrorF("%s MicroTouch %s debug level sets to %d\n", XCONFIG_GIVEN,
		   local->name, debug_level);      
#else
	    ErrorF("%s MicroTouch %s debug not available\n",
		   XCONFIG_GIVEN, local->name, debug_level);      
#endif
	}
        break;

    case HISTORY_SIZE:
      if (xf86GetToken(NULL) != NUMBER)
	xf86ConfigError("MicroTouch motion history size expected");
      local->history_size = val->num;
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s motion history size is %d\n", XCONFIG_GIVEN,
	       local->name, local->history_size);      
      break;
	    
    case FREQUENCY:
      if (xf86GetToken(NULL) != NUMBER)
	xf86ConfigError("MicroTouch ThruGlass frequency expected");
      priv->frequency = val->num;
      if (xf86Verbose)
	ErrorF("%s MicroTouch %s frequency is %d\n", XCONFIG_GIVEN,
	       local->name, priv->frequency);      
      break;

    case ALWAYS_CORE:
	xf86AlwaysCore(local, TRUE);
	if (xf86Verbose)
	    ErrorF("%s MicroTouch %s device will always stays core pointer\n",
		   XCONFIG_GIVEN, local->name);
	break;

    case SWAP_AXES:
      priv->swap_axes = 1;
      if (xf86Verbose) {
	ErrorF("%s Microtouch %s device will work with X and Y axes swapped\n",
	       XCONFIG_GIVEN, local->name);
      }      
      break;

    case PORTRAIT_MODE:
      if (xf86GetToken(NULL) != STRING) {
      portrait_mode_err:
	xf86ConfigError("Microtouch portrait mode should be: Portrait, Landscape or PortraitCCW");
      }
      if (strcmp(val->str, "portrait") == 0) {
	portrait = 1;
      }
      else if (strcmp(val->str, "portraitccw") == 0) {
	portrait = -1;
      }
      else if (strcmp(val->str, "landscape") != 0) {
	goto portrait_mode_err;
      }
      if (xf86Verbose) {
	ErrorF("%s Microtouch %s device will work in %s mode\n",
	       XCONFIG_GIVEN, local->name, val->str);
      }      
      break;

    case EOF:
      FatalError("Unexpected EOF (missing EndSubSection)");
      break;

    default:
      xf86ConfigError("MicroTouch subsection keyword expected");
      break;
    }
  }

  if (priv->max_x - priv->min_x <=0) {
   ErrorF("%s MicroTouch: reverse x mode (minimum x position >= maximum x position)\n",
	   XCONFIG_GIVEN);
  }  
  if (priv->max_y - priv->min_y <=0) {
    ErrorF("%s MicroTouch: reverse y mode (minimum y position >= maximum y position)\n",
	   XCONFIG_GIVEN);    
  }
  
  /*
   * The portrait adjustments need to be done after axis reversing
   * and axes swap. This way the driver can cope with deffective
   * hardware and still do the correct processing depending on the
   * actual display orientation.
   */
  if (portrait == 1) {
    /*
     * Portrait Clockwise: reverse Y axis and exchange X and Y.
     */
    int tmp;
    tmp = priv->min_y;
    priv->min_y = priv->max_y;
    priv->max_y = tmp;
    priv->swap_axes = (priv->swap_axes==0) ? 1 : 0;
  }
  else if (portrait == -1) {
    /*
     * Portrait Counter Clockwise: reverse X axis and exchange X and Y.
     */
    int tmp;
    tmp = priv->min_x;
    priv->min_x = priv->max_x;
    priv->max_x = tmp;
    priv->swap_axes = (priv->swap_axes==0) ? 1 : 0;
  }

  DBG(2, ErrorF("xf86MuTConfig port name=%s\n", priv->input_dev))

  return Success;
}
#endif


/*
 ***************************************************************************
 *
 * xf86MuTConvert --
 *	Convert extended valuators to x and y suitable for core motion
 *	events. Return True if ok and False if the requested conversion
 *	can't be done for the specified valuators.
 *
 ***************************************************************************
 */
static Bool
xf86MuTConvert(LocalDevicePtr	local,
	       int		first,
	       int		num,
	       int		v0,
	       int		v1,
	       int		v2,
	       int		v3,
	       int		v4,
	       int		v5,
	       int		*x,
	       int		*y)
{
  MuTPrivatePtr	priv = (MuTPrivatePtr) local->private;
  int		width = priv->max_x - priv->min_x;
  int		height = priv->max_y - priv->min_y;
  int		input_x, input_y;

  if (first != 0 || num != 2)
    return FALSE;

  if (priv->swap_axes) {
    input_x = v1;
    input_y = v0;
  }
  else {
    input_x = v0;
    input_y = v1;
  }
  *x = (priv->screen_width * (input_x - priv->min_x)) / width;
  *y = (priv->screen_height -
	(priv->screen_height * (input_y - priv->min_y)) / height);
  
#ifdef XFREE86_V4
  /*
   * Need to check if still on the correct screen.
   * This call is here so that this work can be done after
   * calib and before posting the event.
   */
  xf86XInputSetScreen(local, priv->screen_no, *x, *y);
#endif

  return TRUE;
}


/*
 ***************************************************************************
 *
 * xf86MuTReadInput --
 *	Read a buffer full of input from the touchscreen and enqueue
 *	all report packets found in it.
 *	If a packet is not fully received it is deferred until the next
 *	call to the function.
 *	Packet recognized by this function comply with the format :
 *
 *		Byte 1 :  Status flags with MSB set to 1
 *		Byte 2 :  X coordinate (lower bits)
 *		Byte 3 :  X coordinate (upper bits)
 *		Byte 4 :  Y coordinate (lower bits)
 *		Byte 5 :  Y coordinate (upper bits)
 *
 *	The routine can work with any of the two X device structs associated
 *	with the touchscreen. It is always possible to find the relevant
 *	informations and to emit the events for both devices if provided
 *	with one of the two structs. This point is relevant only if the
 *	two devices are actives at the same time.
 *

 ***************************************************************************
 */
static void
xf86MuTReadInput(LocalDevicePtr	local)
{
  MuTPrivatePtr		priv = (MuTPrivatePtr)(local->private);
  int			cur_x, cur_y;
  int			state;
  int			num_bytes;
  int			bytes_in_packet;
  unsigned char		*ptr, *start_ptr;
  
  DBG(4, ErrorF("Entering ReadInput\n"));
  
  /*
   * Try to get a buffer full of report packets.
   */
  DBG(4, ErrorF("num_old_bytes is %d, Trying to read %d bytes from port\n",
		priv->num_old_bytes, MuT_BUFFER_SIZE - priv->num_old_bytes));
  SYSCALL(num_bytes = read(local->fd,
			   (char *) (priv->rec_buf + priv->num_old_bytes),
			   MuT_BUFFER_SIZE - priv->num_old_bytes));
  if (num_bytes < 0) {
    Error("System error while reading from MicroTouch touchscreen.");
    return;
  }

  DBG(4, ErrorF("Read %d bytes of reports\n", num_bytes));
  num_bytes += priv->num_old_bytes;
  ptr = priv->rec_buf;
  bytes_in_packet = 0;
  start_ptr = ptr;

  while (num_bytes >= (MuT_REPORT_SIZE-bytes_in_packet)) {
    /*
     * Skip bytes until a status byte (MSB set to 1).
     */
    if (bytes_in_packet == 0) {
      if ((ptr[0] & 0x80) == 0) {
	DBG(3, ErrorF("Dropping a byte in an attempt to synchronize a report packet: 0x%X\n",
		      ptr[0]));
	start_ptr++;
      }
      else {
	bytes_in_packet++;
      }
      num_bytes--;
      ptr++;
    }
    else if (bytes_in_packet != 5) {
      if ((ptr[0] & 0x80) == 0) {
	bytes_in_packet++;
      }
      else {
	/*
	 * Reset the start of packet, we have most certainly
	 * lost some data.
	 */
	DBG(3, ErrorF("Reseting start of report packet data has been lost\n"));
	bytes_in_packet = 1;
	start_ptr = ptr;
      }
      ptr++;
      num_bytes--;
    }

    if (bytes_in_packet == 5) {
      LocalDevicePtr	local_to_use;
      
      /*
       * First stick together the various pieces.
       */
      state = start_ptr[0] & 0x7F;
      cur_x = WORD_ASSEMBLY(start_ptr[1], start_ptr[2]);
      cur_y = WORD_ASSEMBLY(start_ptr[3], start_ptr[4]);
      
      DBG(3, ErrorF("Packet: 0x%X 0x%X 0x%X 0x%X 0x%X\n",
		    start_ptr[0], start_ptr[1], start_ptr[2], start_ptr[3], start_ptr[4]));
      start_ptr = ptr;
      bytes_in_packet = 0;
      
      /*
       * Send events.
       *
       * We *must* generate a motion before a button change if pointer
       * location has changed as DIX assumes this. This is why we always
       * emit a motion, regardless of the kind of packet processed.
       *
       * If local_to_use is NULL we have received a packet from a device
       * (stylus or finger) which is not configured. Discard it. The first
       * time a warning is emitted in case of misconfiguration. (Patch
       * contributed by David Woodhouse). This probably happens
       * with a touchscreen that reports finger touches only and the
       * configured device is Stylus. On TouchPens the init procedure is
       * smart enough to ask only for packets that match the configuration
       * in XF86Config.
       */
      local_to_use = (state & MuT_WHICH_DEVICE) ? priv->stylus : priv->finger;
      if (!local_to_use) {
	/*
	 * We have received an event for a device which we don't care
	 * about. Drop it, but whinge first, just in case it's a 
	 * misconfiguration.
	 */
	static int whinged = 0;

	if (!whinged) {
	  whinged++;
	  ErrorF("MicroTouch screen sent %s event, but that device is not configured.\n", 
		 (state & MuT_WHICH_DEVICE)?"stylus":"finger");
	  ErrorF("You might want to consider altering your config accordingly.\n");
	}	
      }
      else {
	/*
	 * Emit a motion. If in core pointer mode we need to calibrate
	 * or we will feed X with quite bogus event positions.
	 */
	xf86PostMotionEvent(local_to_use->dev, TRUE, 0, 2, cur_x, cur_y);
	
	/*
	 * Emit a button press or release.
	 */
	if ((state & MuT_CONTACT) != (priv->state & MuT_CONTACT)) {
	  xf86PostButtonEvent(local_to_use->dev, TRUE, 1, state & MuT_CONTACT,
			      0, 2, cur_x, cur_y);
	}
      }
      DBG(3, ErrorF("TouchScreen %s: x(%d), y(%d), %s\n",
		    ((state & MuT_WHICH_DEVICE) ? "Stylus" : "Finger"),
		    cur_x, cur_y,
		    (((state & MuT_CONTACT) != (priv->state & MuT_CONTACT)) ?
		     ((state & MuT_CONTACT) ? "Press" : "Release") : "Stream")));
      priv->state = state;
    }
  }

  /*
   * If some bytes are left in the buffer, pack them at the
   * beginning for the next turn.
   */
  if (num_bytes != 0) {
    memcpy(priv->rec_buf, ptr, num_bytes);
    priv->num_old_bytes = num_bytes;
  }
  else {
    priv->num_old_bytes = 0;
  }
}


/*
 ***************************************************************************
 *
 * xf86MuTSendPacket --
 *	Emit a variable length packet to the controller.
 *	The function expects a valid buffer containing the
 *	command to be sent to the controller.  The command
 *	size is in len
 *	The buffer is filled with the leading and trailing
 *	character before sending.
 *
 ***************************************************************************
 */
static Bool
xf86MuTSendPacket(unsigned char	*packet,
		  int		len,
		  int		fd)
{
  int	result;

  packet[0] = MuT_LEAD_BYTE;
  packet[len+1] = MuT_TRAIL_BYTE;

  DBG(4, ErrorF("Sending packet : 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X\n",
		packet[0], packet[1], packet[2], packet[3], packet[4],
		packet[5], packet[6], packet[7], packet[8], packet[9]));
  SYSCALL(result = write(fd, packet, len+2));
  if (result != len+2) {
    DBG(5, ErrorF("System error while sending to MicroTouch touchscreen.\n"));
    return !Success;
  }
  else {
    return Success;
  }
}


/*
 ***************************************************************************
 *
 * xf86MuTGetReply --
 *	Read a reply packet from the port. Synchronize with start and stop
 *	of packet.
 *      The packet structure read by this function is as follow:
 *		Byte 0 : MuT_LEAD_BYTE
 *		Byte 1
 *		...
 *		Byte n : packet data
 *		Byte n+1 : MuT_TRAIL_BYTE
 *
 *	This function returns if a valid packet has been assembled in
 *	buffer or if no more data is available to do so.
 *
 *	Returns Success if a packet is successfully assembled.
 *	Bytes preceding the MuT_LEAD_BYTE are discarded.
 *	Returns !Success if out of data while reading. The start of the
 *	partially assembled packet is left in buffer, buffer_p reflects
 *	the current state of assembly. Buffer should at least have room
 *	for MuT_BUFFER_SIZE bytes.
 *
 ***************************************************************************
 */
static Bool
xf86MuTGetReply(unsigned char	*buffer,
		int		*buffer_p,
		int		fd)
{
  int	num_bytes;

  DBG(4, ErrorF("Entering xf86MuTGetReply with buffer_p == %d\n", *buffer_p));
  
  /*
   * Try to read enough bytes to fill up the packet buffer.
   */
  DBG(4, ErrorF("buffer_p is %d, Trying to read %d bytes from port\n",
		*buffer_p, MuT_BUFFER_SIZE - *buffer_p));
  SYSCALL(num_bytes = read(fd,
			   (char *) (buffer + *buffer_p),
			   MuT_BUFFER_SIZE - *buffer_p));
  
  /*
   * Okay, give up.
   */
  if (num_bytes < 0) {
    Error("System error while reading from MicroTouch touchscreen.");
    return !Success;
  }
  DBG(4, ErrorF("Read %d bytes of reply\n", num_bytes));
    
  while (num_bytes) {
    /*
     * Sync with the start of a packet.
     */
    if ((*buffer_p == 0) && (buffer[0] != MuT_LEAD_BYTE)) {
      /*
       * No match, shift data one byte toward the start of the buffer.
       */
      DBG(4, ErrorF("Dropping one byte in an attempt to synchronize: '%c' 0x%X\n",
		    buffer[0], buffer[0]));
      memcpy(&buffer[0], &buffer[1], num_bytes-1);
      num_bytes--;
    }
    else if (buffer[*buffer_p] == MuT_TRAIL_BYTE) {
      /*
       * Got a packet, report it.
       */
      *buffer_p = 0;
      return Success;
    }
    else {
      num_bytes--;
      (*buffer_p)++;
    }
  }

  return !Success;
}


/*
 ***************************************************************************
 *
 * xf86MuTWaitReply --
 *	It is assumed that the reply will be in the few next bytes
 *	read and will be available very soon after the command post. if
 *	these two asumptions are not met, there are chances that the server
 *	will be stuck for a while.
 *	The reply is left in reply. The function returns Success if a valid
 *	reply was found and !Success otherwise. Reply should at least
 *	have room for MuT_BUFFER_SIZE bytes.
 *
 ***************************************************************************
 */
#ifndef XFREE86_V4
static int
xf86WaitForInput(int	fd,
		 int	timeout)
{
  fd_set	readfds;
  struct	timeval to;
  int		r;
  
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);
  to.tv_sec = 0;
  to.tv_usec = timeout;
  
  SYSCALL(r = select(FD_SETSIZE, &readfds, NULL, NULL, &to));
  return r;
}
#endif

static Bool
xf86MuTWaitReply(unsigned char	*reply,
		 int		fd)
{
  Bool			ok;
  int			i, result;
  int			reply_p = 0;
  unsigned char		local_reply[3];

  DBG(4, ErrorF("Waiting a reply\n"));
  i = MuT_MAX_TRIALS;
  do {
    ok = !Success;
    
    /*
     * Wait half a second for the reply. The fuse counts down each
     * timeout and each wrong packet.
     */
    DBG(4, ErrorF("Waiting %d ms for data from port\n", MuT_MAX_WAIT / 1000));
    result = xf86WaitForInput(fd, MuT_MAX_WAIT);
    if (result > 0) {
      if (reply) {
	ok = xf86MuTGetReply(reply, &reply_p, fd);
      }
      else {
	ok = xf86MuTGetReply(local_reply, &reply_p, fd);
	if (ok && local_reply[1] != MuT_OK) {
	  DBG(3, ErrorF("Error reported by firmware\n"));
	  ok = !Success;
	}
      }
    }
    else {
      DBG(3, ErrorF("No answer from port : %d\n", result));
    }
    
    if (result == 0)
      i--;
  } while(ok != Success && i);

  return ok;
}


/*
 ***************************************************************************
 *
 * xf86MuTSendCommand --
 *	Emit a command to the controller and blocks until the reply is
 *	read.
 *
 *	The reply is left in reply. The function returns Success if the
 *	reply is valid and !Success otherwise. Reply should at least
 *	have room for MuT_BUFFER_SIZE bytes.
 *
 ***************************************************************************
 */
static Bool
xf86MuTSendCommand(unsigned char	*request,
		   int			len,
		   unsigned char	*reply,
		   int			fd)
{
  Bool			ok;
  
  if (xf86MuTSendPacket(request, len, fd) == Success) {
    ok = xf86MuTWaitReply(reply, fd);
    return ok;
  }
  else {
    return !Success;
  }
}


/*
 ***************************************************************************
 *
 * xf86MuTPrintIdent --
 *	Print type of touchscreen and features on controller board.
 *
 ***************************************************************************
 */
static void
xf86MuTPrintIdent(unsigned char	*packet)
{
  int	vers, rev;
  
#ifdef XFREE86_V4
  xf86Msg(X_PROBED, "MicroTouch touchscreen is a ");
  if (strncmp((char *) &packet[1], MuT_TOUCH_PEN_IDENT, 2) == 0) {
    xf86Msg(X_NONE, "TouchPen");
  }
  else if (strncmp((char *) &packet[1], MuT_SMT3_IDENT, 2) == 0) {
    xf86Msg(X_NONE, "Serial/SMT3");
  }
  else if (strncmp((char *) &packet[1], MuT_THRU_GLASS_IDENT, 2) == 0) {
    xf86Msg(X_NONE, "ThruGlass");
  }
  xf86Msg(X_NONE, ", connected through a serial port.\n");
  sscanf((char *) &packet[3], "%2d%2d", &vers, &rev);
  xf86Msg(X_PROBED, "MicroTouch controller firmware revision is %d.%d.\n", vers, rev);
#else
  ErrorF("%s MicroTouch touchscreen is a ", XCONFIG_PROBED);
  if (strncmp((char *) &packet[1], MuT_TOUCH_PEN_IDENT, 2) == 0) {
    ErrorF("TouchPen");
  }
  else if (strncmp((char *) &packet[1], MuT_SMT3_IDENT, 2) == 0) {
    ErrorF("Serial/SMT3");
  }
  else if (strncmp((char *) &packet[1], MuT_THRU_GLASS_IDENT, 2) == 0) {
    ErrorF("ThruGlass");
  }
  ErrorF(", connected through a serial port.\n");
  sscanf((char *) &packet[3], "%2d%2d", &vers, &rev);
  ErrorF("%s MicroTouch controller firmware revision is %d.%d.\n", XCONFIG_PROBED, vers, rev);
#endif
}


/*
 ***************************************************************************
 *
 * xf86MuTPrintHwStatus --
 *	Print status of hardware. That is if the controller report errors,
 *	decode and display them.
 *
 ***************************************************************************
 */
static void
xf86MuTPrintHwStatus(unsigned char	*packet)
{
#ifdef XFREE86_V4
  xf86Msg(X_PROBED, "MicroTouch status of errors: %c%c.\n", packet[7], packet[8]);
#else
  ErrorF("%s MicroTouch status of errors: %c%c.\n", XCONFIG_PROBED, packet[7], packet[8]);
#endif
}


/*
 ***************************************************************************
 *
 * xf86MuTPtrControl --
 *
 ***************************************************************************
 */
#if 0
static void
xf86MuTPtrControl(DeviceIntPtr	dev,
		  PtrCtrl	*ctrl)
{
}
#endif


/*
 ***************************************************************************
 *
 * xf86MuTControl --
 *
 ***************************************************************************
 */
static Bool
xf86MuTControl(DeviceIntPtr	dev,
	       int		mode)
{
  LocalDevicePtr	local = (LocalDevicePtr) dev->public.devicePrivate;
  MuTPrivatePtr		priv = (MuTPrivatePtr)(local->private);
  unsigned char		map[] = { 0, 1 };
  unsigned char		req[MuT_PACKET_SIZE];
  unsigned char		reply[MuT_BUFFER_SIZE];
  char			*id_string = DEVICE_ID(local->private_flags) == FINGER_ID ? "finger" : "stylus";
  
  switch(mode) {

  case DEVICE_INIT:
    {
      DBG(2, ErrorF("MicroTouch %s init...\n", id_string));

      if (priv->screen_no >= screenInfo.numScreens ||
	  priv->screen_no < 0) {
	priv->screen_no = 0;
      }
      priv->screen_width = screenInfo.screens[priv->screen_no]->width;
      priv->screen_height = screenInfo.screens[priv->screen_no]->height;

      /*
       * Device reports button press for up to 1 button.
       */
      if (InitButtonClassDeviceStruct(dev, 1, map) == FALSE) {
	ErrorF("Unable to allocate ButtonClassDeviceStruct\n");
	return !Success;
      }
      
      /*
       * Device reports motions on 2 axes in absolute coordinates.
       * Axes min and max values are reported in raw coordinates.
       * Resolution is computed roughly by the difference between
       * max and min values scaled from the approximate size of the
       * screen to fit one meter.
       */
      if (InitValuatorClassDeviceStruct(dev, 2, xf86GetMotionEvents,
					local->history_size, Absolute) == FALSE) {
	ErrorF("Unable to allocate ValuatorClassDeviceStruct\n");
	return !Success;
      }
      else {
	InitValuatorAxisStruct(dev, 0, priv->min_x, priv->max_x,
			       9500,
			       0     /* min_res */,
			       9500  /* max_res */);
	InitValuatorAxisStruct(dev, 1, priv->min_y, priv->max_y,
			       10500,
			       0     /* min_res */,
			       10500 /* max_res */);
      }
      
      if (InitFocusClassDeviceStruct(dev) == FALSE) {
	ErrorF("Unable to allocate FocusClassDeviceStruct\n");
      }
      
      /*
       * Allocate the motion events buffer.
       */
      xf86MotionHistoryAllocate(local);
      
      /*
       * This once has caused the server to crash after doing an xalloc & strcpy ??
       */
#ifndef XFREE86_V4
      AssignTypeAndName(dev, local->atom, local->name);
#endif
      
      DBG(2, ErrorF("Done.\n"));
      return Success;
    }
    
  case DEVICE_ON:
    {
      Bool	already_open = FALSE;
      char	*report_what = "";
      
      DBG(2, ErrorF("MicroTouch %s on...\n", id_string));

      /*
       * Try to see if the port has already been opened either
       * for this device or for the other one.
       */
      if (local->fd >= 0) {
	already_open = TRUE;
      }
      else {
	switch (DEVICE_ID(local->private_flags)) {
	case FINGER_ID:
	  if (priv->stylus && priv->stylus->fd >= 0) {
	    already_open = TRUE;
	    local->fd = priv->stylus->fd;
	  }
	  break;
	case STYLUS_ID:
	  if (priv->finger && priv->finger->fd >= 0) {
	    already_open = TRUE;
	    local->fd = priv->finger->fd;
	  }
	  break;
	}
      }
      if (!already_open) {
#ifndef XFREE86_V4
	struct termios termios_tty;
	int	       result;
#endif
	
	DBG(2, ErrorF("MicroTouch touchscreen opening : %s\n", priv->input_dev));
#ifdef XFREE86_V4
	local->fd = xf86OpenSerial(local->options);
	if (local->fd < 0) {
	  Error("Unable to open MicroTouch touchscreen device");
	  return !Success;
	}
#else
	SYSCALL(local->fd = open(priv->input_dev, O_RDWR|O_NDELAY, 0));
	if (local->fd < 0) {
	  Error("Unable to open MicroTouch touchscreen device");
	  return !Success;
	}
	
	/*
	 * Try to see if the link is at the specified rate and
	 * reset the controller. The wait time needed by the
	 * controller after reset should be compensated by the
	 * timeouts in the receive section.
	 */
	DBG(3, ErrorF("Try to see if the link is at the specified rate\n"));
	memset(&termios_tty, 0, sizeof(termios_tty));
	termios_tty.c_cflag = priv->link_speed | CS8 | CREAD | CLOCAL;
#ifdef CRTSCTS
	termios_tty.c_cflag &= ~CRTSCTS;
#endif
	termios_tty.c_cc[VMIN] = 1;
	SYSCALL(result = tcsetattr(local->fd, TCSANOW, &termios_tty));
	if (result < 0) {
	  Error("Unable to configure MicroTouch touchscreen port");
	  goto not_success;
	}
#endif
	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_RESET, strlen(MuT_RESET));
	if (xf86MuTSendCommand(req, strlen(MuT_RESET), NULL, local->fd) != Success) {
	  DBG(3, ErrorF("Not at the specified rate, giving up\n"));
	  goto not_success;
	}
	
	/*
	 * ask the controller to report identity and status.
	 */
	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_OUTPUT_IDENT, strlen(MuT_OUTPUT_IDENT));
	if (xf86MuTSendCommand(req, strlen(MuT_OUTPUT_IDENT),
			       reply, local->fd) != Success) {
	  ErrorF("Unable to ask MicroTouch touchscreen identification\n");
	  goto not_success;
	}
	xf86MuTPrintIdent(reply);
	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_UNIT_TYPE, strlen(MuT_UNIT_TYPE));
	if (xf86MuTSendCommand(req, strlen(MuT_UNIT_TYPE),
			       reply, local->fd) != Success) {
	  ErrorF("Unable to ask MicroTouch touchscreen status\n");
	  goto not_success;
	}
	xf86MuTPrintHwStatus(reply);
	
	/*
	 * Set the operating mode: Format Tablet, Mode stream, Pen.
	 */
	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_FORMAT_TABLET, strlen(MuT_FORMAT_TABLET));
	if (xf86MuTSendCommand(req, strlen(MuT_FORMAT_TABLET),
			       NULL, local->fd) != Success) {
	  ErrorF("Unable to switch MicroTouch touchscreen to Tablet Format\n");
	  goto not_success;
	}
	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_MODE_STREAM, strlen(MuT_MODE_STREAM));
	if (xf86MuTSendCommand(req, strlen(MuT_MODE_STREAM),
			       NULL, local->fd) != Success) {
	  ErrorF("Unable to switch MicroTouch touchscreen to Stream Mode\n");
	  goto not_success;
	}

	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_PEN_ONLY, strlen(MuT_PEN_ONLY));
	if (xf86MuTSendCommand(req, strlen(MuT_PEN_ONLY),
			       NULL, local->fd) != Success) {
	  ErrorF("Unable to change MicroTouch touchscreen to pen mode\n");
	  goto not_success;
	}
	/*	goto not_success;*/
#ifndef XFREE86_V4
	xf86AddEnabledDevice(local);
#else
	AddEnabledDevice(local->fd);
#endif
      }

      /*
       * Select Pen / Finger reports depending on which devices are
       * currently on.
       */
      switch (DEVICE_ID(local->private_flags)) {
      case FINGER_ID:
	if (priv->stylus && priv->stylus->dev->public.on) {
	  report_what = MuT_PEN_FINGER;
	}
	else {
	  report_what = MuT_FINGER_ONLY;
	}
	break;
      case STYLUS_ID:
	if (priv->finger && priv->finger->dev->public.on) {
	  report_what = MuT_PEN_FINGER;
	}
	else {
	  report_what = MuT_PEN_ONLY;
	}
	break;
      }
      memset(req, 0, MuT_PACKET_SIZE);
      strncpy((char *) &req[1], report_what, strlen(report_what));
      if (xf86MuTSendCommand(req, strlen(report_what), NULL, local->fd) != Success) {
	ErrorF("Unable to change MicroTouch touchscreen to %s\n",
	       (strcmp(report_what, MuT_PEN_FINGER) == 0) ? "Pen & Finger" :
	       ((strcmp(report_what, MuT_PEN_ONLY) == 0) ? "Pen Only" : "Finger Only"));
	goto not_success;
      }
      dev->public.on = TRUE;  
      /*
       * Set frequency for ThruGlass
       */
      if (priv->frequency != 0) {
	memset(req, 0, MuT_PACKET_SIZE);
	strncpy((char *) &req[1], MuT_SET_FREQUENCY, strlen(MuT_SET_FREQUENCY));
	req[1+strlen(MuT_SET_FREQUENCY)] = ' ';
	req[2+strlen(MuT_SET_FREQUENCY)] = '0';
	req[3+strlen(MuT_SET_FREQUENCY)] = (priv->frequency<=9?'0':'A'-10)+priv->frequency;
	if (xf86MuTSendCommand(req, strlen((char *) &req[1]), NULL, local->fd) != Success) {
	  ErrorF("Unable to set MicroTouch ThruGlass frquency to %d\n", priv->frequency);
	  goto not_success;
	}
      }

    
      DBG(2, ErrorF("Done\n"));
      return Success;
      
    not_success:
      SYSCALL(close(local->fd));
      local->fd = -1;
      return !Success;
    }
  
  /*
   * Deactivate the device.
   */
  case DEVICE_OFF:
    DBG(2, ErrorF("MicroTouch %s off...\n", id_string));
    dev->public.on = FALSE;
    DBG(2, ErrorF("Done\n"));
    return Success;
    
    /*
     * Final close before server exit. This is used during server shutdown.
     * Close the port and free all the resources.
     */
  case DEVICE_CLOSE:
    DBG(2, ErrorF("MicroTouch %s close...\n", id_string));
    dev->public.on = FALSE;
    if (local->fd >= 0) {
#ifdef XFREE86_V4
      xf86RemoveEnabledDevice(local);
#else
      RemoveEnabledDevice(local->fd);
#endif
      SYSCALL(close(local->fd));
      local->fd = -1;
      /*
       * Need some care to close the port only once.
       */
      switch (DEVICE_ID(local->private_flags)) {
	case FINGER_ID:
	  if (priv->stylus) {
	    priv->stylus->fd = -1;
	  }
	  break;
	case STYLUS_ID:
	  if (priv->finger) {
	    priv->finger->fd = -1;
	  }
      }
    }
    DBG(2, ErrorF("Done\n"));  
    return Success;

  default:
      ErrorF("unsupported mode=%d\n", mode);
      return !Success;
  }
}

/*
 ***************************************************************************
 *
 * xf86MuTAllocate --
 *
 ***************************************************************************
 */
static LocalDevicePtr
#ifndef XFREE86_V4
xf86MuTAllocate(char		*name,
		char		*type_name,
		int		flag)
#else
xf86MuTAllocate(InputDriverPtr	drv,
		char		*name,
		char		*type_name,
		int		flag)
#endif
{
#ifndef XFREE86_V4
  LocalDevicePtr        local = (LocalDevicePtr) xalloc(sizeof(LocalDeviceRec));
#else
  LocalDevicePtr        local = xf86AllocateInput(drv, 0);
#endif
  MuTPrivatePtr         priv = (MuTPrivatePtr) xalloc(sizeof(MuTPrivateRec));
  
  if (!local) {
    if (priv) {
      xfree(priv);
    }
    return NULL;
  }
  if (!priv) {
    if (local) {
      xfree(local);
    }
    return NULL;
  }

  priv->input_dev = strdup(MuT_PORT);
#ifndef XFREE86_V4
  priv->link_speed = MuT_LINK_SPEED;
#endif
  priv->min_x = 0;
  priv->max_x = 0;
  priv->min_y = 0;
  priv->max_y = 0;
  priv->screen_no = 0;
  priv->screen_width = -1;
  priv->screen_height = -1;
  priv->inited = 0;
  priv->state = 0;
  priv->num_old_bytes = 0;
  priv->stylus = NULL;
  priv->finger = NULL;
  priv->swap_axes = 0;
  priv->frequency = 0;
  
  local->name = name;
  local->flags = 0 /* XI86_NO_OPEN_ON_INIT */;
#ifndef XFREE86_V4
  local->device_config = xf86MuTConfig;
#endif
  local->device_control = xf86MuTControl;
  local->read_input = xf86MuTReadInput;
  local->control_proc = NULL;
  local->close_proc = NULL;
  local->switch_mode = NULL;
  local->conversion_proc = xf86MuTConvert;
  local->reverse_conversion_proc = NULL;
  local->fd = -1;
  local->atom = 0;
  local->dev = NULL;
  local->private = priv;
  local->private_flags = flag;
  local->type_name = type_name;
  local->history_size = 0;
  
  return local;
}


/*
 ***************************************************************************
 *
 * xf86MuTAllocateFinger --
 *
 ***************************************************************************
 */
static LocalDevicePtr
#ifndef XFREE86_V4
xf86MuTAllocateFinger(void)
#else
xf86MuTAllocateFinger(InputDriverPtr	drv)     
#endif
{
#ifndef XFREE86_V4
  LocalDevicePtr	local = xf86MuTAllocate(XI_FINGER, "MicroTouch Finger", FINGER_ID);
#else
  LocalDevicePtr	local = xf86MuTAllocate(drv, XI_FINGER, "MicroTouch Finger", FINGER_ID);
#endif

  if (local) {
    ((MuTPrivatePtr) local->private)->finger = local;
  }
  return local;  
}


/*
 ***************************************************************************
 *
 * xf86MuTAllocateStylus --
 *
 ***************************************************************************
 */
static LocalDevicePtr
#ifndef XFREE86_V4
xf86MuTAllocateStylus(void)
#else
xf86MuTAllocateStylus(InputDriverPtr	drv)     
#endif
{
#ifndef XFREE86_V4
  LocalDevicePtr	local = xf86MuTAllocate(XI_STYLUS, "MicroTouch Stylus", STYLUS_ID);
#else
  LocalDevicePtr	local = xf86MuTAllocate(drv, XI_STYLUS, "MicroTouch Stylus", STYLUS_ID);
#endif

  if (local) {
    ((MuTPrivatePtr) local->private)->stylus = local;
  }
  return local;
}


#ifndef XFREE86_V4
/*
 ***************************************************************************
 *
 * MicroTouch finger device association --
 *
 ***************************************************************************
 */
DeviceAssocRec MuT_finger_assoc =
{
  FINGER_SECTION_NAME,		/* config_section_name */
  xf86MuTAllocateFinger		/* device_allocate */
};

/*
 ***************************************************************************
 *
 * MicroTouch stylus device association --
 *
 ***************************************************************************
 */
DeviceAssocRec MuT_stylus_assoc =
{
  STYLUS_SECTION_NAME,		/* config_section_name */
  xf86MuTAllocateStylus		/* device_allocate */
};


#ifdef DYNAMIC_MODULE
/*
 ***************************************************************************
 *
 * entry point of dynamic loading
 *
 ***************************************************************************
 */
int
#ifndef DLSYM_BUG
init_module(unsigned long	server_version)
#else
init_xf86MuTouch(unsigned long      server_version)
#endif
{
    xf86AddDeviceAssoc(&MuT_finger_assoc);
    xf86AddDeviceAssoc(&MuT_stylus_assoc);

    if (server_version != XF86_VERSION_CURRENT) {
      ErrorF("Warning: MicroTouch module compiled for version%s\n", XF86_VERSION);
      return 0;
    }
    else {
      return 1;
    }
}
#endif

#else /* XFREE86_V4 */
static void
xf86MuTUninit(InputDriverPtr	drv,
	      LocalDevicePtr	local,
	      int		flags)
{
  MuTPrivatePtr		priv = (MuTPrivatePtr) local->private;
  
  xf86MuTControl(local->dev, DEVICE_OFF);

  if (priv) {
    priv->stylus->private = NULL;
    priv->finger->private = NULL;
    xfree(priv->input_dev);
    xfree(priv);
  }
  xfree(local->name);
  xfree(local);

  xf86DeleteInput(local, 0);
}

static const char *default_options[] = {
  "BaudRate", "9600",
  "StopBits", "1",
  "DataBits", "8",
  "Parity", "None",
  "Vmin", "10",
  "Vtime", "1",
  "FlowControl", "None",
  NULL
};

static InputInfoPtr
xf86MuTInit(InputDriverPtr	drv,
	    IDevPtr		dev,
	    int			flags)
{
  LocalDevicePtr	local=NULL, fake_local=NULL, current;
  MuTPrivatePtr		priv=NULL;
  char			*str;
  int			portrait=0;

  fake_local = (LocalDevicePtr) xcalloc(1, sizeof(LocalDeviceRec));
  if (!fake_local) {
    goto init_err;
  }
  fake_local->conf_idev = dev;
  
  xf86CollectInputOptions(fake_local, default_options, NULL);

  str = xf86FindOptionValue(fake_local->options, "Type");
  if (str && (xf86NameCmp(str, "finger") == 0)) {
    local = xf86MuTAllocateFinger(drv);
  }
  else if (str && (xf86NameCmp(str, "stylus") == 0)) {
    local = xf86MuTAllocateStylus(drv);
  }
  else {
    xf86Msg(X_ERROR, "%s: Type field missing in Microtouch module config,\n"
	    "Must be stylus or finger\n", dev->identifier);
    goto init_err;
  }
  if (!local) {
    goto init_err;
  }
  priv = local->private;
  local->options = fake_local->options;
  local->conf_idev = fake_local->conf_idev;
  xfree(fake_local);
  fake_local = NULL;
  
  str = xf86FindOptionValue(local->options, "Device");
  if (!str) {
    xf86Msg(X_ERROR, "%s: No Device specified in Microtouch module config.\n",
	    dev->identifier);
    goto init_err;
  }
  priv->input_dev = strdup(str);

  /*
   * See if another X device share the same physical
   * device and set up the links so that they share
   * the same private structure (the one that controls
   * the physical device).
   */
  current = xf86FirstLocalDevice();
  while (current) {
    if ((local != current) &&
	(current->device_control == xf86MuTControl) &&
	(strcmp(((MuTPrivatePtr) (current->private))->input_dev, priv->input_dev) == 0)) {
      xf86Msg(X_CONFIG, "MicroTouch config detected a device share between %s and %s\n",
	      local->name, current->name);
      xfree(priv->input_dev);
      xfree(priv);
      priv = local->private = current->private;
      switch (DEVICE_ID(local->private_flags)) {
      case FINGER_ID:
	priv->finger = local;
	break;
      case STYLUS_ID:
	priv->stylus = local;
	break;
      }
      break;
    }
    current = current->next;
  }
  if (!current) {
    xf86Msg(X_CONFIG, "MicroTouch %s input device: %s\n", local->name, priv->input_dev);
  }

  /* Process the common options. */
  xf86ProcessCommonOptions(local, local->options);
  
  str = xf86FindOptionValue(local->options, "DeviceName");
  if (str) {
    local->name = strdup(str);
  }
  xf86Msg(X_CONFIG, "Microtouch X device name: %s\n", local->name);  
  priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", 0);
  xf86Msg(X_CONFIG, "Microtouch associated screen: %d\n", priv->screen_no);  
  priv->max_x = xf86SetIntOption(local->options, "MaxX", 3000);
  xf86Msg(X_CONFIG, "Microtouch maximum x position: %d\n", priv->max_x);
  priv->min_x = xf86SetIntOption(local->options, "MinX", 0);
  xf86Msg(X_CONFIG, "Microtouch minimum x position: %d\n", priv->min_x);
  priv->max_y = xf86SetIntOption(local->options, "MaxY", 3000);
  xf86Msg(X_CONFIG, "Microtouch maximum y position: %d\n", priv->max_y);
  priv->min_y = xf86SetIntOption(local->options, "MinY", 0);
  xf86Msg(X_CONFIG, "Microtouch minimum y position: %d\n", priv->min_y);
  priv->frequency = xf86SetIntOption(local->options, "Frequency", 0);
  xf86Msg(X_CONFIG, "Microtouch ThruGlass frequency is: %d\n", priv->frequency);
  priv->swap_axes = xf86SetBoolOption(local->options, "SwapXY", 0);
  if (priv->swap_axes) {
    xf86Msg(X_CONFIG, "Microtouch %s device will work with X and Y axes swapped\n",
	    local->name);
  }
  debug_level = xf86SetIntOption(local->options, "DebugLevel", 0);
  if (debug_level) {
#if DEBUG
    xf86Msg(X_CONFIG, "Microtouch debug level sets to %d\n", debug_level);      
#else
    xf86Msg(X_INFO, "Microtouch debug not available\n");      
#endif
  }
  str = xf86SetStrOption(local->options, "PortraitMode", "Landscape");
  if (strcmp(str, "Portrait") == 0) {
    portrait = 1;
  }
  else if (strcmp(str, "PortraitCCW") == 0) {
    portrait = -1;
  }
  else if (strcmp(str, "Landscape") != 0) {
    xf86Msg(X_ERROR, "Microtouch portrait mode should be: Portrait, Landscape or PortraitCCW");
    str = "Landscape";
  }
  xf86Msg(X_CONFIG, "Microtouch device will work in %s mode\n", str);      
  
  if (priv->max_x - priv->min_x <= 0) {
    xf86Msg(X_INFO, "MicroTouch: reverse x mode (minimum x position >= maximum x position)\n");
  }  
  if (priv->max_y - priv->min_y <= 0) {
    xf86Msg(X_INFO, "MicroTouch: reverse y mode (minimum y position >= maximum y position)\n");    
  }

  if (portrait == 1) {
    /*
     * Portrait Clockwise: reverse Y axis and exchange X and Y.
     */
    int tmp;
    tmp = priv->min_y;
    priv->min_y = priv->max_y;
    priv->max_y = tmp;
    priv->swap_axes = (priv->swap_axes==0) ? 1 : 0;
  }
  else if (portrait == -1) {
    /*
     * Portrait Counter Clockwise: reverse X axis and exchange X and Y.
     */
    int tmp;
    tmp = priv->min_x;
    priv->min_x = priv->max_x;
    priv->max_x = tmp;
    priv->swap_axes = (priv->swap_axes==0) ? 1 : 0;
  }

  /* mark the device configured */
  local->flags |= XI86_CONFIGURED;
  return local;

 init_err:
  if (fake_local) {
    xfree(fake_local);
  }
  if (priv) {
    if (priv->input_dev) {
      xfree(priv->input_dev);
    }
    xfree(priv);
  }
  if (local) {
    xfree(local);
  }
  return NULL;
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec MUTOUCH = {
    1,				/* driver version */
    "mutouch",			/* driver name */
    NULL,			/* identify */
    xf86MuTInit,		/* pre-init */
    xf86MuTUninit,		/* un-init */
    NULL,			/* module */
    0				/* ref count */
};

#ifdef XFree86LOADER
static pointer
Plug(pointer	module,
     pointer	options,
     int	*errmaj,
     int	*errmin)
{
  xf86AddInputDriver(&MUTOUCH, module, 0);

  return module;
}

static void
Unplug(pointer	p)
{
  DBG(1, ErrorF("MuTUnplug\n"));
}

static XF86ModuleVersionInfo version_rec = {
  "mutouch",
  MODULEVENDORSTRING,
  MODINFOSTRING1,
  MODINFOSTRING2,
  XF86_VERSION_CURRENT,
  1, 0, 0,
  ABI_CLASS_XINPUT,
  ABI_XINPUT_VERSION,
  MOD_CLASS_XINPUT,
  { 0, 0, 0, 0 }
};

/*
 * This is the entry point in the module. The name
 * is setup after the pattern <module_name>ModuleData.
 * Do not change it.
 */
XF86ModuleData mutouchModuleData = { &version_rec, Plug, Unplug };
#endif

#endif /* XFREE86_V4 */
