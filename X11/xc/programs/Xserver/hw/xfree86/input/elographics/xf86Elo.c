/* $XConsortium: xf86Elo.c /main/13 1996/10/25 14:11:31 kaleb $ */
/*
 * Copyright 1995, 1999 by Patrick Lecoanet, France. <lecoanet@cena.dgac.fr>
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

/* $XFree86: xc/programs/Xserver/hw/xfree86/input/elographics/xf86Elo.c,v 1.14 2001/08/17 13:27:56 dawes Exp $ */

/*
 *******************************************************************************
 *******************************************************************************
 *
 * This driver is able to deal with Elographics SmartSet serial controllers.
 * It uses only a subset of the functions provided through the protocol.
 *
 * SUPPORT FOR E281-2310 and compatible controllers added with help of:
 *   1996/01/17 Juergen P. Meier (jpm@mailserv.rz.fh-muenchen.de) and
 *   1998/03/25 G.Felkel@edelmann.de
 *  
 *   The E281-2310 is a somewhat lobotomized 2210.
 *   It does not support the c,g,h,k,l,p,q,s and t commands.
 *   Especially the P command, which is used to check the baud rate.
 *   The E281-2310 however semms to use always 9600bps, 8bit, 1stop
 *   no parity, Hardwarehandshake (RTS-CTS) (which are the drivers
 *   default values)
 *
 *******************************************************************************
 *******************************************************************************
 */

#include "xf86Version.h"
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4
#endif

#ifdef XFREE86_V4

#ifndef XFree86LOADER
#include <unistd.h>
#include <errno.h>
#endif

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

#if defined(sun) && !defined(i386)
#include <errno.h>
#include <termio.h>
#include <fcntl.h>
#include <ctype.h>

#include "extio.h"
#else /* defined(sun) && !defined(i386) */
#include "compiler.h"

#include "xf86.h"
#include "xf86Procs.h"
#include "xf86_OSlib.h"
#include "xf86_Config.h"
#include "xf86Xinput.h"
#endif /* defined(sun) && !defined(i386) */

#if !defined(sun) || defined(i386)
#include "os.h"
#include "osdep.h"
#include "exevents.h"

#include "extnsionst.h"
#include "extinit.h"
#endif /* !defined(sun) || defined(i386) */

#endif /* XFREE86_V4 */


#ifndef XFREE86_V4
#if !defined(sun) || defined(i386)
/*
 ***************************************************************************
 *
 * Configuration descriptor.
 *
 ***************************************************************************
 */

#define PORT		1
#define ELO_DEVICE_NAME	2
#define SCREEN_NO	3
#define UNTOUCH_DELAY	4
#define REPORT_DELAY	5
#define MAXX		6
#define MAXY		7
#define MINX		8
#define MINY		9
#define DEBUG_LEVEL     10
#define HISTORY_SIZE	11
#define LINK_SPEED	12
#define ALWAYS_CORE	13
#define SWAP_AXES	14
#define PORTRAIT_MODE	15

static SymTabRec EloTab[] = {
  { ENDSUBSECTION,     "endsubsection" },
  { PORT,              "port" },
  { ELO_DEVICE_NAME,   "devicename" },
  { SCREEN_NO,	       "screenno" },
  { UNTOUCH_DELAY,     "untouchdelay" },
  { REPORT_DELAY,      "reportdelay"},
  { MAXX,              "maximumxposition" },
  { MAXY,              "maximumyposition" },
  { MINX,              "minimumxposition" },
  { MINY,              "minimumyposition" },
  { DEBUG_LEVEL,       "debuglevel" },
  { HISTORY_SIZE,      "historysize" },
  { LINK_SPEED,        "linkspeed" },
  { ALWAYS_CORE,       "alwayscore" },
  { SWAP_AXES,	       "swapxy" },
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
#endif /* !defined(sun) || defined(i386) */

/*
 * This struct connects a line speed with
 * a compatible motion packet delay. The
 * driver will attempt to enforce a correct
 * delay (according to this table) in order to
 * avoid losing data in the touchscreen controller.
 * LinkSpeedValues should be kept in sync with
 * LinkSpeedTab.
 */
typedef struct {
  int	speed;
  int	delay;
} LinkParameterStruct;

static LinkParameterStruct	LinkSpeedValues[] = {
  { B300, 32 },
  { B1200, 8 },
  { B2400, 4 },
  { B9600, 1 },
  { B19200, 0 }
};
#endif /* XFREE86_V4 */


/*
 ***************************************************************************
 *
 * Default constants.
 *
 ***************************************************************************
 */
#define ELO_MAX_TRIALS	3		/* Number of timeouts waiting for a	*/
					/* pending reply.			*/
#define ELO_MAX_WAIT		100000	/* Max wait time for a reply (microsec)	*/
#define ELO_UNTOUCH_DELAY	5	/* 100 ms				*/
#define ELO_REPORT_DELAY	1	/* 40 ms or 25 motion reports/s		*/
#define ELO_LINK_SPEED		B9600	/* 9600 Bauds				*/
#define ELO_PORT		"/dev/ttyS1"

#define DEFAULT_MAX_X		3000
#define DEFAULT_MIN_X		600
#define DEFAULT_MAX_Y		3000
#define DEFAULT_MIN_Y		600


/*
 ***************************************************************************
 *
 * Protocol constants.
 *
 ***************************************************************************
 */
#define ELO_PACKET_SIZE		10

#define ELO_SYNC_BYTE		'U'	/* Sync byte. First of a packet.	*/
#define ELO_TOUCH		'T'	/* Report of touchs and motions. Not	*
					 * used by 2310.			*/
#define ELO_OWNER		'O'	/* Report vendor name.			*/
#define ELO_ID			'I'	/* Report of type and features.		*/
#define ELO_MODE		'M'	/* Set current operating mode.		*/
#define ELO_PARAMETER		'P'	/* Set the serial parameters.		*/
#define ELO_REPORT		'B'	/* Set touch reports timings.		*/
#define ELO_ACK			'A'	/* Acknowledge packet			*/

#define ELO_INIT_CHECKSUM	0xAA	/* Initial value of checksum.		*/

#define	ELO_PRESS		0x01	/* Flags in ELO_TOUCH status byte	*/
#define	ELO_STREAM		0x02
#define ELO_RELEASE		0x04

#define ELO_TOUCH_MODE		0x01	/* Flags in ELO_MODE command		*/
#define ELO_STREAM_MODE		0x02
#define ELO_UNTOUCH_MODE	0x04
#define ELO_RANGE_CHECK_MODE	0x40
#define ELO_TRIM_MODE		0x02
#define ELO_CALIB_MODE		0x04
#define ELO_SCALING_MODE	0x08
#define ELO_TRACKING_MODE	0x40

#define ELO_SERIAL_SPEED	0x06	/* Flags for high speed serial (19200)	*/
#define ELO_SERIAL_MASK		0xF8

#define ELO_SERIAL_IO		'0'	/* Indicator byte for PARAMETER command */


/*
 ***************************************************************************
 *
 * Usefull macros.
 *
 ***************************************************************************
 */
#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 8) | (byte1))
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
typedef struct _EloPrivateRec {
  char		*input_dev;		/* The touchscreen input tty			*/
  int		min_x;			/* Minimum x reported by calibration		*/
  int		max_x;			/* Maximum x					*/
  int		min_y;			/* Minimum y reported by calibration		*/
  int		max_y;			/* Maximum y					*/
  int		untouch_delay;		/* Delay before reporting an untouch (in ms)    */
  int		report_delay;		/* Delay between touch report packets		*/
#ifndef XFREE86_V4
  int		link_speed;		/* Speed of the RS232 link connecting the ts.	*/
#endif
  int		screen_no;		/* Screen associated with the device		*/
  int		screen_width;		/* Width of the associated X screen		*/
  int		screen_height;		/* Height of the screen				*/
  Bool		inited;			/* The controller has already been configured ?	*/
  Bool		is_a_2310;		/* Set if the smartset is a 2310.		*/
  int		checksum;		/* Current checksum of data in assembly buffer	*/
  int		packet_buf_p;		/* Assembly buffer pointer			*/
  int		swap_axes;		/* Swap X an Y axes if != 0 */
  unsigned char	packet_buf[ELO_PACKET_SIZE]; /* Assembly buffer				*/
} EloPrivateRec, *EloPrivatePtr;


#ifndef XFREE86_V4
#if !defined(sun) || defined(i386)
/*
 ***************************************************************************
 *
 * xf86EloConfig --
 *	Configure the driver from the configuration data.
 *
 ***************************************************************************
 */
static Bool
xf86EloConfig(LocalDevicePtr    *array,
              int               inx,
              int               max,
	      LexPtr            val)
{
  LocalDevicePtr        local = array[inx];
  EloPrivatePtr         priv = (EloPrivatePtr)(local->private);
  int                   token;
  int			portrait=0;
  
  while ((token = xf86GetToken(EloTab)) != ENDSUBSECTION) {
    switch(token) {
      
    case PORT:
      if (xf86GetToken(NULL) != STRING) {
	xf86ConfigError("Elographics input port expected");
      }
      priv->input_dev = strdup(val->str);	
      if (xf86Verbose) {
	ErrorF("%s Elographics input port: %s\n",
	       XCONFIG_GIVEN, priv->input_dev);
      }
      break;

    case ELO_DEVICE_NAME:
      if (xf86GetToken(NULL) != STRING) {
	xf86ConfigError("Elographics device name expected");
      }
      local->name = strdup(val->str);
      if (xf86Verbose) {
	ErrorF("%s Elographics X device name: %s\n",
	       XCONFIG_GIVEN, local->name);
      }
      break;

    case SCREEN_NO:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("Elographics screen number expected");
      }
      priv->screen_no = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics associated screen: %d\n",
	       XCONFIG_GIVEN, priv->screen_no);
      }
      break;

    case UNTOUCH_DELAY:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("Elographics untouch delay expected");
      }
      priv->untouch_delay = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics untouch delay: %d ms\n",
	       XCONFIG_GIVEN, priv->untouch_delay*10);
      }
      break;

    case REPORT_DELAY:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("Elographics report delay expected");
      }
      priv->report_delay = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics report delay: %d ms\n",
	       XCONFIG_GIVEN, priv->report_delay*10);
      }
      break;
      
    case LINK_SPEED:
      {
	int	ltoken = xf86GetToken(LinkSpeedTab);
	if (ltoken == EOF ||
	    ltoken == STRING ||
	    ltoken == NUMBER) {
	  xf86ConfigError("Elographics link speed expected");
	}
	priv->link_speed = LinkSpeedValues[ltoken-1].speed;
	if (xf86Verbose) {
	  ErrorF("%s Elographics link speed: %s bps\n",
		 XCONFIG_GIVEN, (LinkSpeedTab[ltoken-1].name)+1);
	}
      }
      break;
      
    case MAXX:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("Elographics maximum x position expected");
      }
      priv->max_x = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics maximum x position: %d\n",
	       XCONFIG_GIVEN, priv->max_x);
      }
     break;
      
    case MAXY:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("Elographics maximum y position expected");
      }
      priv->max_y = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics maximum y position: %d\n",
	       XCONFIG_GIVEN, priv->max_y);
      }
     break;
      
    case MINX:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("Elographics minimum x position expected");
      }
      priv->min_x = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics minimum x position: %d\n",
	       XCONFIG_GIVEN, priv->min_x);
      }
     break;
      
    case MINY:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("Elographics minimum y position expected");
      }
      priv->min_y = val->num;
      if (xf86Verbose) {
	ErrorF("%s Elographics minimum y position: %d\n",
	       XCONFIG_GIVEN, priv->min_y);
      }
     break;
      
    case DEBUG_LEVEL:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("Elographics driver debug level expected");
      }
      debug_level = val->num;
      if (xf86Verbose) {
#if DEBUG
	ErrorF("%s Elographics debug level sets to %d\n", XCONFIG_GIVEN,
	       debug_level);      
#else
	ErrorF("%s Elographics debug not available\n",
	       XCONFIG_GIVEN, debug_level);      
#endif
      }
      break;

    case HISTORY_SIZE:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("Elographics motion history size expected");
      }
      local->history_size = val->num;
      if (xf86Verbose) {
	ErrorF("%s EloGraphics motion history size is %d\n", XCONFIG_GIVEN,
	       local->history_size);
      }
      break;
	    
    case ALWAYS_CORE:
      xf86AlwaysCore(local, TRUE);
      if (xf86Verbose) {
	ErrorF("%s Elographics device will always stays core pointer\n",
	       XCONFIG_GIVEN);
      }
      break;

    case SWAP_AXES:
      priv->swap_axes = 1;
      if (xf86Verbose) {
	ErrorF("%s Elographics device will work with X and Y axes swapped\n",
	       XCONFIG_GIVEN);
      }      
      break;

    case PORTRAIT_MODE:
      if (xf86GetToken(NULL) != STRING) {
      portrait_mode_err:
	xf86ConfigError("Elographics portrait mode should be: Portrait, Landscape or PortraitCCW");
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
	ErrorF("%s Elographics device will work in %s mode\n",
	       XCONFIG_GIVEN, val->str);
      }      
      break;

    case EOF:
      FatalError("Unexpected EOF (missing EndSubSection)");
      break;

    default:
      xf86ConfigError("Elographics subsection keyword expected");
      break;
    }
  }

  if (priv->max_x - priv->min_x <= 0) {
    ErrorF("%s Elographics: reverse x mode (minimum x position >= maximum x position)\n",
	   XCONFIG_GIVEN);
  }  
  if (priv->max_y - priv->min_y <= 0) {
    ErrorF("%s Elographics: reverse y mode (minimum y position >= maximum y position)\n",
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
    
  DBG(2, ErrorF("xf86EloConfig port name=%s\n", priv->input_dev))

  return Success;
}
#endif
#endif


/*
 ***************************************************************************
 *
 * xf86EloGetPacket --
 *	Read a packet from the port. Try to synchronize with start of
 *	packet and compute checksum.
 *      The packet structure read by this function is as follow:
 *		Byte 0 : ELO_SYNC_BYTE
 *		Byte 1
 *		...
 *		Byte 8 : packet data
 *		Byte 9 : checksum of bytes 0 to 8
 *
 *	This function returns if a valid packet has been assembled in
 *	buffer or if no more data is available.
 *
 *	Returns Success if a packet is successfully assembled including
 *	testing checksum. If a packet checksum is incorrect, it is discarded.
 *	Bytes preceding the ELO_SYNC_BYTE are also discarded.
 *	Returns !Success if out of data while reading. The start of the
 *	partially assembled packet is left in buffer, buffer_p and
 *	checksum reflect the current state of assembly.
 *
 ***************************************************************************
 */
static Bool
xf86EloGetPacket(unsigned char	*buffer,
		 int		*buffer_p,
		 int		*checksum,
		 int		fd)
{
  int	num_bytes;
  Bool	ok;

  DBG(4, ErrorF("Entering xf86EloGetPacket with checksum == %d and buffer_p == %d\n",
		*checksum, *buffer_p));
  
  /*
   * Try to read enough bytes to fill up the packet buffer.
   */
  DBG(4, ErrorF("buffer_p is %d, Trying to read %d bytes from link\n",
		*buffer_p, ELO_PACKET_SIZE - *buffer_p));
  SYSCALL(num_bytes = read(fd,
			   (char *) (buffer + *buffer_p),
			   ELO_PACKET_SIZE - *buffer_p));

  /*
   * Okay, give up.
   */
  if (num_bytes < 0) {
    Error("System error while reading from Elographics touchscreen.");
    return !Success;
  }
  DBG(4, ErrorF("Read %d bytes\n", num_bytes));
    
  while (num_bytes) {
    /*
     * Sync with the start of a packet.
     */
    if ((*buffer_p == 0) && (buffer[0] != ELO_SYNC_BYTE)) {
      /*
       * No match, shift data one byte toward the start of the buffer.
       */
      ErrorF("Elographics: Dropping one byte in an attempt to synchronize: '%c' 0x%X\n",
	     buffer[0], buffer[0]);
      memcpy(&buffer[0], &buffer[1], num_bytes-1);
    }
    else {
      /*
       * Compute checksum in assembly buffer.
       */
      if (*buffer_p < ELO_PACKET_SIZE-1) {
	*checksum = *checksum + buffer[*buffer_p];
	*checksum = *checksum % 256;
	DBG(4, ErrorF(" 0x%X-->0x%X ", buffer[*buffer_p], *checksum));
      }
      (*buffer_p)++;
    }
    num_bytes--;
  }

  if (*buffer_p == ELO_PACKET_SIZE) {
    /*
     * Got a packet, validate checksum and reset state.
     */
    ok = (*checksum == buffer[ELO_PACKET_SIZE-1]);
    DBG(3, ErrorF("Expecting checksum %d, got %d\n", *checksum, buffer[ELO_PACKET_SIZE-1]));
    *checksum = ELO_INIT_CHECKSUM;
    *buffer_p = 0;

    if (!ok) {
      ErrorF("Checksum error on Elographics touchscreen link\n");
      return !Success;
    }
    
    /*
     * Valid packet received report it.
     */
    return Success;
  }
  else {
    return !Success;
  }
}

/*
 ***************************************************************************
 *
 * xf86EloConvert --
 *	Convert extended valuators to x and y suitable for core motion
 *	events. Return True if ok and False if the requested conversion
 *	can't be done for the specified valuators.
 *
 ***************************************************************************
 */
static Bool
xf86EloConvert(LocalDevicePtr	local,
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
  EloPrivatePtr	priv = (EloPrivatePtr) local->private;
  int		width = priv->max_x - priv->min_x;
  int		height = priv->max_y - priv->min_y;
  int		input_x, input_y;
  
  if (first != 0 || num != 2) {
    return FALSE;
  }

  DBG(3, ErrorF("EloConvert: v0(%d), v1(%d)\n",	v0, v1));

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
  
  DBG(3, ErrorF("EloConvert: x(%d), y(%d)\n",	*x, *y));

  return TRUE;
}


/*
 ***************************************************************************
 *
 * xf86EloReadInput --
 *	Read all pending report packets from the touchscreen and enqueue
 *	them.
 *	If a packet is not fully received it is deferred until the next
 *	call to the function.
 *	Packets recognized by this function comply with the format:
 *
 *		Byte 1 :  ELO_TOUCH
 *		Byte 2 :  Packet type
 *		  Bit 2 : Pen Up   (Release)
 *		  Bit 1 : Position (Stream)
 *		  Bit 0 : Pen Down (Press)
 *		Byte 3 :  X coordinate (lower bits)
 *		Byte 4 :  X coordinate (upper bits)
 *		Byte 5 :  Y coordinate (lower bits)
 *		Byte 6 :  Y coordinate (upper bits)
 *		Byte 7 :  Z coordinate (lower bits)
 *		Byte 8 :  Z coordinates (upper bits)
 *
 *
 ***************************************************************************
 */
static void
xf86EloReadInput(LocalDevicePtr	local)
{
  EloPrivatePtr			priv = (EloPrivatePtr)(local->private);
  int				cur_x, cur_y;
  int				state;

  DBG(4, ErrorF("Entering ReadInput\n"));
  /*
   * Try to get a packet.
   */
  if (xf86EloGetPacket(priv->packet_buf,
		       &priv->packet_buf_p,
		       &priv->checksum,
		       local->fd) != Success) {
    return;
  }

  /*
   * Process only ELO_TOUCHs here.
   */
  if (priv->packet_buf[1] == ELO_TOUCH) {
    /*
     * First stick together the various pieces.
     */
    cur_x = WORD_ASSEMBLY(priv->packet_buf[3], priv->packet_buf[4]);
    cur_y = WORD_ASSEMBLY(priv->packet_buf[5], priv->packet_buf[6]);
    state = priv->packet_buf[2] & 0x07;

    /*
     * Send events.
     *
     * We *must* generate a motion before a button change if pointer
     * location has changed as DIX assumes this. This is why we always
     * emit a motion, regardless of the kind of packet processed.
     */
    xf86PostMotionEvent(local->dev, TRUE, 0, 2, cur_x, cur_y);
    
    /*
     * Emit a button press or release.
     */
    if (state == ELO_PRESS || state == ELO_RELEASE) {
      xf86PostButtonEvent(local->dev, TRUE, 1, state == ELO_PRESS, 0, 2, cur_x, cur_y);
    }
    
    DBG(3, ErrorF("TouchScreen: x(%d), y(%d), %s\n",
		  cur_x, cur_y,
		  (state == ELO_PRESS) ? "Press" : ((state == ELO_RELEASE) ? "Release" : "Stream")));
  }
}


/*
 ***************************************************************************
 *
 * xf86EloSendPacket --
 *	Emit an height bytes packet to the controller.
 *	The function expects a valid buffer containing the
 *	command to be sent to the controller. It fills it with the
 *	leading sync character an the trailing checksum byte.
 *
 ***************************************************************************
 */
static Bool
xf86EloSendPacket(unsigned char	*packet,
		  int		fd)
{
  int	i, result;
  int	sum = ELO_INIT_CHECKSUM;

  packet[0] = ELO_SYNC_BYTE;
  for (i = 0; i < ELO_PACKET_SIZE-1; i++) {
    sum += packet[i];
    sum &= 0xFF;
  }
  packet[ELO_PACKET_SIZE-1] = sum;

  DBG(4, ErrorF("Sending packet : 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X \n",
		packet[0], packet[1], packet[2], packet[3], packet[4],
		packet[5], packet[6], packet[7], packet[8], packet[9]));
  SYSCALL(result = write(fd, packet, ELO_PACKET_SIZE));
  if (result != ELO_PACKET_SIZE) {
    DBG(5, ErrorF("System error while sending to Elographics touchscreen.\n"));
    return !Success;
  }
  else {
    return Success;
  }
}


/*
 ***************************************************************************
 *
 * xf86EloWaitReply --
 *	It is assumed that the reply will be in the few next bytes
 *	read and will be available very soon after the query post. if
 *	these two asumptions are not met, there are chances that the server
 *	will be stuck for a while.
 *	The reply type need to match parameter 'type'.
 *	The reply is left in reply. The function returns Success if the
 *	reply is valid and !Success otherwise.
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
xf86EloWaitReply(unsigned char	type,
		 unsigned char	*reply,
		 int		fd)
{
  Bool			ok;
  int			i, result;
  int			reply_p = 0;
  int			sum = ELO_INIT_CHECKSUM;

  DBG(4, ErrorF("Waiting a '%c' reply\n", type));
  i = ELO_MAX_TRIALS;
  do {
    ok = !Success;
    
    /*
     * Wait half a second for the reply. The fuse counts down each
     * timeout and each wrong packet.
     */
    DBG(4, ErrorF("Waiting %d ms for data from port\n", ELO_MAX_WAIT / 1000));
    result = xf86WaitForInput(fd, ELO_MAX_WAIT);
    if (result > 0) {
      ok = xf86EloGetPacket(reply, &reply_p, &sum, fd);
      /*
       * Do not report an error on a 'P' query as the controller
       * might be a 2310.
       */
      if (ok == Success && reply[1] != type && type != ELO_PARAMETER) {
	DBG(3, ErrorF("Wrong reply received\n"));
	ok = !Success;
      }
    }
    else {
      DBG(3, ErrorF("No answer from link : %d\n", result));
    }
    
    if (result == 0) {
      i--;
    }
  } while(ok != Success && i);

  return ok;
}


/*
 ***************************************************************************
 *
 * xf86EloWaitAck --
 *	Wait for an acknowledge from the controller. Returns Success if
 *	acknowledge received and reported no errors.
 *
  ***************************************************************************
 */
static Bool
xf86EloWaitAck(int	fd)
{
  unsigned char	packet[ELO_PACKET_SIZE];
  int		i, nb_errors;

  if (xf86EloWaitReply(ELO_ACK, packet, fd) == Success) {
    for (i = 0, nb_errors = 0; i < 4; i++) {
      if (packet[2 + i] != '0') {
	nb_errors++;
      }
    }
    if (nb_errors != 0) {
      DBG(2, ErrorF("Elographics acknowledge packet reports %d errors\n",
		    nb_errors));
    }
    return Success;
    /*    return (nb_errors < 4) ? Success : !Success;*/
  }
  else {
    return !Success;
  }
}


/*
 ***************************************************************************
 *
 * xf86EloSendQuery --
 *	Emit a query to the controller and blocks until the reply and
 *	the acknowledge are read.
 *
 *	The reply is left in reply. The function returns Success if the
 *	reply is valid and !Success otherwise.
 *
 ***************************************************************************
 */
static Bool
xf86EloSendQuery(unsigned char	*request,
		 unsigned char	*reply,
		 int		fd)
{
  Bool			ok;
  
  if (xf86EloSendPacket(request, fd) == Success) {
    ok = xf86EloWaitReply(toupper(request[1]), reply, fd);
    if (ok == Success) {
      ok = xf86EloWaitAck(fd);
    }
    return ok;
  }
  else {
    return !Success;
  }
}


/*
 ***************************************************************************
 *
 * xf86EloSendControl --
 *	Emit a control command to the controller and wait for acknowledge.
 *
 *	Returns Success if acknowledge received and reported no error.
 *
 ***************************************************************************
 */
static Bool
xf86EloSendControl(unsigned char	*control,
		   int			fd)
{
  if (xf86EloSendPacket(control, fd) == Success) {
    return xf86EloWaitAck(fd);
  }
  else {
    return !Success;
  }
}


/*
 ***************************************************************************
 *
 * xf86EloPrintIdent --
 *	Print type of touchscreen and features on controller board.
 *
 ***************************************************************************
 */
static void
xf86EloPrintIdent(unsigned char	*packet,
		  EloPrivatePtr	priv)
{
#ifdef XFREE86_V4
  xf86Msg(X_PROBED, "Elographics touchscreen is a ");
  switch(packet[2]) {
  case '0':
    xf86Msg(X_NONE, "AccuTouch");
    break;
  case '1':
    xf86Msg(X_NONE, "DuraTouch");
    break;
  case '2':
    xf86Msg(X_NONE, "Intellitouch");
    break;
  }
  xf86Msg(X_NONE, ", connected through a ");
  switch(packet[3]) {
  case '0':
    xf86Msg(X_NONE, "serial link.\n");
    break;
  case '1':
    xf86Msg(X_NONE, "PC-Bus port.\n");
    break;
  case '2':
    xf86Msg(X_NONE, "Micro Channel port.\n");
    break;
  }
  xf86Msg(X_PROBED, "The controller is a model ");
  if (packet[8] & 1) {
    if (priv->is_a_2310) {
      xf86Msg(X_NONE, "E281-2310");
    }
    else {
      xf86Msg(X_NONE, "E271-2210");
    }
  }
  else {
    xf86Msg(X_NONE, "E271-2200");
  }
  xf86Msg(X_NONE, ", firmware revision %d.%d.\n", packet[6], packet[5]);
  
  if (packet[4]) {
    xf86Msg(X_PROBED, " Additional features:\n");
    if (packet[4] & 0x10) {
      xf86Msg(X_PROBED, "	External A/D converter\n");
    }
    if (packet[4] & 0x20) {
      xf86Msg(X_PROBED, "	32Ko RAM\n");
    }
    if (packet[4] & 0x40) {
      xf86Msg(X_PROBED, "	RAM onboard\n");
    }
    if (packet[4] & 0x80) {
      xf86Msg(X_PROBED, "	Z axis active\n");
    }
    xf86Msg(X_NONE, "\n");
  }
#else
  ErrorF("%s Elographics touchscreen is a ", XCONFIG_PROBED);
  switch(packet[2]) {
  case '0':
    ErrorF("AccuTouch");
    break;
  case '1':
    ErrorF("DuraTouch");
    break;
  case '2':
    ErrorF("Intellitouch");
    break;
  }
  ErrorF(", connected through a ");
  switch(packet[3]) {
  case '0':
    ErrorF("serial link.\n");
    break;
  case '1':
    ErrorF("PC-Bus port.\n");
    break;
  case '2':
    ErrorF("Micro Channel port.\n");
    break;
  }
  ErrorF("%s The controller is a model ", XCONFIG_PROBED);
  if (packet[8] & 1) {
    if (priv->is_a_2310) {
      ErrorF("E281-2310");
    }
    else {
      ErrorF("E271-2210");
    }
  }
  else {
    ErrorF("E271-2200");
  }
  ErrorF(", firmware revision %d.%d.\n", packet[6], packet[5]);
  
  if (packet[4]) {
    ErrorF("%s Additional features:\n", XCONFIG_PROBED);
    if (packet[4] & 0x10) {
      ErrorF("%s	External A/D converter\n", XCONFIG_PROBED);
    }
    if (packet[4] & 0x20) {
      ErrorF("%s	32Ko RAM\n", XCONFIG_PROBED);
    }
    if (packet[4] & 0x40) {
      ErrorF("%s	RAM onboard\n", XCONFIG_PROBED);
    }
    if (packet[4] & 0x80) {
      ErrorF("%s	Z axis active\n", XCONFIG_PROBED);
    }
    ErrorF("\n");
  }
#endif
}


/*
 ***************************************************************************
 *
 * xf86EloPtrControl --
 *
 ***************************************************************************
 */
#if 0
static void
xf86EloPtrControl(DeviceIntPtr	dev,
		  PtrCtrl	*ctrl)
{
}
#endif


/*
 ***************************************************************************
 *
 * xf86EloControl --
 *
 ***************************************************************************
 */
static Bool
xf86EloControl(DeviceIntPtr	dev,
	       int		mode)
{
  LocalDevicePtr	local = (LocalDevicePtr) dev->public.devicePrivate;
  EloPrivatePtr		priv = (EloPrivatePtr)(local->private);
  unsigned char		map[] = { 0, 1 };
  unsigned char		req[ELO_PACKET_SIZE];
  unsigned char		reply[ELO_PACKET_SIZE];

  switch(mode) {

  case DEVICE_INIT:
    {
#if defined(sun) && !defined(i386) && !defined(XFREE86_V4)
      char      *name = (char *) getenv("ELO_DEV");
      char      *calib = (char *) getenv("ELO_CALIB");
      char	*speed = (char *) getenv("ELO_SPEED");
      char	*delays = (char *) getenv("ELO_DELAYS");
#endif
      
      DBG(2, ErrorF("Elographics touchscreen init...\n"));

#if defined(sun) && !defined(i386) && !defined(XFREE86_V4)
      if (name) {
	priv->input_dev = strdup(name);
	ErrorF("Elographics touchscreen port changed to '%s'\n",
	       priv->input_dev);
      }
      if (calib) {
	if (sscanf(calib, "%d %d %d %d",
		   &priv->min_x, &priv->max_x,
		   &priv->min_y, &priv->max_y) != 4) {
	  ErrorF("Incorrect syntax in ELO_CALIB\n");
	  return !Success;
	}
	else if (priv->max_x <= priv->min_x ||
		 priv->max_y <= priv->min_y) {
	  ErrorF("Bogus calibration data in ELO_CALIB\n");
	  return !Success;
	}
	else {
	  ErrorF("Calibration will be done with:\n");
	  ErrorF("x_min=%d, x_max=%d, y_min=%d, y_max=%d\n",
		 priv->min_x, priv->max_x, priv->min_y, priv->max_y);
	}
      }
      if (speed) {
	/* These tests should be kept in sync with the LinkSpeedValues
	 * array. */
	if (strcmp(speed, "B9600") == 0) {
	  priv->link_speed = B9600;
	}
	else if (strcmp(speed, "B19200") == 0) {
	  priv->link_speed = B19200;
	}
	else if (strcmp(speed, "B2400") == 0) {
	  priv->link_speed = B2400;
	}
	else if (strcmp(speed, "B1200") == 0) {
	  priv->link_speed = B1200;
	}
	else if (strcmp(speed, "B300") == 0) {
	  priv->link_speed = B300;
	}
	else {
	  ErrorF("Bogus speed value in ELO_SPEED\n");
	  return !Success;
	}
      }
      if (delays) {
	if (sscanf(delays, "%d %d",
		   &priv->untouch_delay,
		   &priv->report_delay) != 2) {
	  ErrorF("Bogus delays data in ELO_DELAYS\n");
	}
	else {
	  ErrorF("Untouch delay will be: %d\n", priv->untouch_delay);
	  ErrorF("Report delay will be: %d\n", priv->report_delay);
	}
      }
#endif
      
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
	ErrorF("Unable to allocate Elographics touchscreen ButtonClassDeviceStruct\n");
	return !Success;
      }
      
      if (InitFocusClassDeviceStruct(dev) == FALSE) {
	ErrorF("Unable to allocate Elographics touchscreen FocusClassDeviceStruct\n");
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
	ErrorF("Unable to allocate Elographics touchscreen ValuatorClassDeviceStruct\n");
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
	ErrorF("Unable to allocate Elographics touchscreen FocusClassDeviceStruct\n");
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
    DBG(2, ErrorF("Elographics touchscreen on...\n"));

    if (local->fd < 0) {
#ifndef XFREE86_V4
      struct termios termios_tty;
      int	     i, result;
#endif
      
      DBG(2, ErrorF("Elographics touchscreen opening : %s\n", priv->input_dev));
#ifdef XFREE86_V4
      local->fd = xf86OpenSerial(local->options);
      if (local->fd < 0) {
	Error("Unable to open Elographics touchscreen device");
	return !Success;
      }
#else
      SYSCALL(local->fd = open(priv->input_dev, O_RDWR|O_NDELAY, 0));
      if (local->fd < 0) {
	Error("Unable to open Elographics touchscreen device");
	return !Success;
      }

      DBG(3, ErrorF("Try to see if the link is at the specified rate\n"));
      memset(&termios_tty, 0, sizeof(termios_tty));
      termios_tty.c_cflag = priv->link_speed | CS8 | CREAD | CLOCAL;
      termios_tty.c_cc[VMIN] = 1;
      SYSCALL(result = tcsetattr(local->fd, TCSANOW, &termios_tty));
      if (result < 0) {
	Error("Unable to configure Elographics touchscreen port");
	goto not_success;
      }
#endif
      /*
       * Try to see if the link is at the specified rate and
       * ask the controller to report various infos.
       */
      memset(req, 0, ELO_PACKET_SIZE);
      req[1] = tolower(ELO_PARAMETER);
      if (xf86EloSendQuery(req, reply, local->fd) != Success) {
	priv->is_a_2310 = 1;
	ErrorF("Not at the specified rate or model 2310, will continue\n");
      }

      /*
       * Ask the controller to report various infos.
       */
      memset(req, 0, ELO_PACKET_SIZE);
      req[1] = tolower(ELO_ID);
      if (xf86EloSendQuery(req, reply, local->fd) == Success) {
	xf86EloPrintIdent(reply, priv);
      }
      else {
	ErrorF("Unable to ask Elographics touchscreen identification\n");
	goto not_success;
      }
      
      /*
       * Set the operating mode: Stream, no scaling, no calibration,
       * no range checking, no trim, tracking enabled.
       */
      memset(req, 0, ELO_PACKET_SIZE);
      req[1] = ELO_MODE;
      req[3] = ELO_TOUCH_MODE | ELO_STREAM_MODE | ELO_UNTOUCH_MODE;
      req[4] = ELO_TRACKING_MODE;
      if (xf86EloSendControl(req, local->fd) != Success) {
	ErrorF("Unable to change Elographics touchscreen operating mode\n");
	goto not_success;
      }

#ifndef XFREE86_V4
      /*
       * Check if the report delay is compatible with the selected
       * link speed and reset it otherwise.
       */
      for (i = 0; i < sizeof(LinkSpeedValues)/sizeof(LinkParameterStruct); i++) {
	if (LinkSpeedValues[i].speed == priv->link_speed) {
	  if (LinkSpeedValues[i].delay > priv->report_delay) {
	    ErrorF("Changing report delay from %d ms to %d ms to comply with link speed\n",
		   priv->report_delay*10, LinkSpeedValues[i].delay*10);
	    priv->report_delay = LinkSpeedValues[i].delay;
	  }
	}
      }
#endif
      /*
       * Set the touch reports timings from configuration data.
       */
      memset(req, 0, ELO_PACKET_SIZE);
      req[1] = ELO_REPORT;
      req[2] = priv->untouch_delay;
      req[3] = priv->report_delay;
      if (xf86EloSendControl(req, local->fd) != Success) {
	ErrorF("Unable to change Elographics touchscreen reports timings\n");

      not_success:
	SYSCALL(close(local->fd));
	local->fd = -1;
	return !Success;
      }
#ifdef XFREE86_V4
      xf86AddEnabledDevice(local);
#else
      AddEnabledDevice(local->fd);
#endif
      dev->public.on = TRUE;  
    }
    
    DBG(2, ErrorF("Done\n"));
    return Success;
    
    /*
     * Deactivate the device. After this, the device will not emit
     * events until a subsequent DEVICE_ON. Thus, we can momentarily
     * close the port.
     */
  case DEVICE_OFF:
    DBG(2, ErrorF("Elographics touchscreen off...\n"));
    dev->public.on = FALSE;
    if (local->fd >= 0) {
#ifdef XFREE86_V4
      xf86RemoveEnabledDevice(local);
#else
      RemoveEnabledDevice(local->fd);
#endif
    }
    SYSCALL(close(local->fd));
    local->fd = -1;
    DBG(2, ErrorF("Done\n"));
    return Success;
    
    /*
     * Final close before server exit. This is used during server shutdown.
     * Close the port and free all the resources.
     */
  case DEVICE_CLOSE:
    DBG(2, ErrorF("Elographics touchscreen close...\n"));
    dev->public.on = FALSE;
    if (local->fd >= 0) {
      RemoveEnabledDevice(local->fd);
    }
    SYSCALL(close(local->fd));
    local->fd = -1;
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
 * xf86EloAllocate --
 *
 ***************************************************************************
 */
static LocalDevicePtr
#ifndef XFREE86_V4
xf86EloAllocate(void)
#else
xf86EloAllocate(InputDriverPtr	drv)     
#endif
{
  LocalDevicePtr	local;
  EloPrivatePtr		priv;

  priv = xalloc(sizeof(EloPrivateRec));
  if (!priv)
    return NULL;

#ifndef XFREE86_V4
  local = (LocalDevicePtr) xalloc(sizeof(LocalDeviceRec));
#else
  local = xf86AllocateInput(drv, 0);
#endif

  if (!local) {
    xfree(priv);
    return NULL;
  }
  
#ifdef XFREE86_V4
  priv->input_dev = strdup(ELO_PORT);
#else
  priv->input_dev = ELO_PORT;
  priv->link_speed = ELO_LINK_SPEED;
#endif
  priv->min_x = 0;
  priv->max_x = 3000;
  priv->min_y = 0;
  priv->max_y = 3000;
  priv->untouch_delay = ELO_UNTOUCH_DELAY;
  priv->report_delay = ELO_REPORT_DELAY;
  priv->screen_no = 0;
  priv->screen_width = -1;
  priv->screen_height = -1;
  priv->inited = 0;
  priv->is_a_2310 = 0;
  priv->checksum = ELO_INIT_CHECKSUM;
  priv->packet_buf_p = 0;
  priv->swap_axes = 0;

  local->name = XI_TOUCHSCREEN;
  local->flags = 0 /* XI86_NO_OPEN_ON_INIT */;
#ifndef XFREE86_V4
#if !defined(sun) || defined(i386)
  local->device_config = xf86EloConfig;
#endif
#endif
  local->device_control = xf86EloControl;
  local->read_input   = xf86EloReadInput;
  local->control_proc = NULL;
  local->close_proc   = NULL;
  local->switch_mode  = NULL;
  local->conversion_proc = xf86EloConvert;
  local->reverse_conversion_proc = NULL;
  local->fd	      = -1;
  local->atom	      = 0;
  local->dev	      = NULL;
  local->private      = priv;
  local->type_name    = "Elographics TouchScreen";
  local->history_size = 0;
  
  return local;
}

#ifndef XFREE86_V4
/*
 ***************************************************************************
 *
 * Elographics device association --
 *
 ***************************************************************************
 */
DeviceAssocRec elographics_assoc =
{
  "elographics",                /* config_section_name */
  xf86EloAllocate               /* device_allocate */
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
init_xf86Elo(unsigned long      server_version)
#endif
{
    xf86AddDeviceAssoc(&elographics_assoc);

    if (server_version != XF86_VERSION_CURRENT) {
      ErrorF("Warning: Elographics module compiled for version%s\n", XF86_VERSION);
      return 0;
    }
    else {
      return 1;
    }
}
#endif

#else /* XFREE86_V4 */

static void
xf86EloUninit(InputDriverPtr	drv,
	      LocalDevicePtr	local,
	      int flags)
{
  EloPrivatePtr		priv = (EloPrivatePtr) local->private;
  
  xf86EloControl(local->dev, DEVICE_OFF);

  xfree(priv->input_dev);
  xfree(priv);
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
xf86EloInit(InputDriverPtr	drv,
	    IDevPtr		dev,
	    int			flags)
{
  LocalDevicePtr	local=NULL;
  EloPrivatePtr		priv=NULL;
  char			*str;
  int			portrait = 0;
  
  local = xf86EloAllocate(drv);
  if (!local) {
    return NULL;
  }
  priv = local->private;
  local->conf_idev = dev;
  
  xf86CollectInputOptions(local, default_options, NULL);
  /* Process the common options. */
  xf86ProcessCommonOptions(local, local->options);

  str = xf86FindOptionValue(local->options, "Device");
  if (!str) {
    xf86Msg(X_ERROR, "%s: No Device specified in Elographics module config.\n",
	    dev->identifier);
    if (priv) {
      if (priv->input_dev) {
	xfree(priv->input_dev);
      }
      xfree(priv);
    }
    return local;
  }
  priv->input_dev = strdup(str);

  local->name = xf86SetStrOption(local->options, "DeviceName", XI_TOUCHSCREEN);
  xf86Msg(X_CONFIG, "Elographics X device name: %s\n", local->name);  
  priv->screen_no = xf86SetIntOption(local->options, "ScreenNo", 0);
  xf86Msg(X_CONFIG, "Elographics associated screen: %d\n", priv->screen_no);  
  priv->untouch_delay = xf86SetIntOption(local->options, "UntouchDelay", ELO_UNTOUCH_DELAY);
  xf86Msg(X_CONFIG, "Elographics untouch delay: %d ms\n", priv->untouch_delay*10);
  priv->report_delay = xf86SetIntOption(local->options, "ReportDelay", ELO_REPORT_DELAY);
  xf86Msg(X_CONFIG, "Elographics report delay: %d ms\n", priv->report_delay*10);
  priv->max_x = xf86SetIntOption(local->options, "MaxX", 3000);
  xf86Msg(X_CONFIG, "Elographics maximum x position: %d\n", priv->max_x);
  priv->min_x = xf86SetIntOption(local->options, "MinX", 0);
  xf86Msg(X_CONFIG, "Elographics minimum x position: %d\n", priv->min_x);
  priv->max_y = xf86SetIntOption(local->options, "MaxY", 3000);
  xf86Msg(X_CONFIG, "Elographics maximum y position: %d\n", priv->max_y);
  priv->min_y = xf86SetIntOption(local->options, "MinY", 0);
  xf86Msg(X_CONFIG, "Elographics minimum y position: %d\n", priv->min_y);
  priv->swap_axes = xf86SetBoolOption(local->options, "SwapXY", 0);
  if (priv->swap_axes) {
    xf86Msg(X_CONFIG, "Elographics device will work with X and Y axes swapped\n");
  }
  debug_level = xf86SetIntOption(local->options, "DebugLevel", 0);
  if (debug_level) {
#if DEBUG
    xf86Msg(X_CONFIG, "Elographics debug level sets to %d\n", debug_level);      
#else
    xf86Msg(X_INFO, "Elographics debug not available\n");      
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
    xf86Msg(X_ERROR, "Elographics portrait mode should be: Portrait, Landscape or PortraitCCW");
    str = "Landscape";
  }
  xf86Msg(X_CONFIG, "Elographics device will work in %s mode\n", str);      
  
  if (priv->max_x - priv->min_x <= 0) {
    xf86Msg(X_INFO, "Elographics: reverse x mode (minimum x position >= maximum x position)\n");
  }  
  if (priv->max_y - priv->min_y <= 0) {
    xf86Msg(X_INFO, "Elographics: reverse y mode (minimum y position >= maximum y position)\n");
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
}

#ifdef XFree86LOADER
static
#endif
InputDriverRec ELOGRAPHICS = {
    1,				/* driver version */
    "elographics",		/* driver name */
    NULL,			/* identify */
    xf86EloInit,		/* pre-init */
    xf86EloUninit,		/* un-init */
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
  xf86AddInputDriver(&ELOGRAPHICS, module, 0);

  return module;
}

static void
Unplug(pointer	p)
{
  DBG(1, ErrorF("EloUnplug\n"));
}

static XF86ModuleVersionInfo version_rec = {
  "elographics",
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
XF86ModuleData elographicsModuleData = { &version_rec, Plug, Unplug };

#endif
#endif /* XFREE86_V4 */
