/*
 * $XFree86: xc/programs/Xserver/hw/xfree86/input/magictouch/xf86MagicTouch.c,v 1.3 2003/01/12 03:55:50 tsi Exp $
 */

#include <xf86Version.h>
#if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(3,9,0,0,0)
#define XFREE86_V4
#endif

#ifdef XFREE86_V4

#ifndef XFree86LOADER
#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif

#include <misc.h>
#include <xf86.h>
#if !defined(DGUX)
#include <xf86_ansic.h>
#endif
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>

#ifdef XFree86LOADER
#include <xf86Module.h>
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
#include "xf86Version.h"
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
#define MAGIC_DEVICE_NAME	2
#define SCREEN_NO	3
#define MAXX		4
#define MAXY		5
#define MINX		6
#define MINY		7
#define DEBUG_LEVEL     8
#define HISTORY_SIZE	9
#define LINK_SPEED	10
#define ALWAYS_CORE	11
#define SWAP_AXES	12
#define N_SAMPLE_X	13
#define N_SAMPLE_Y	14

static SymTabRec MagicTab[] = {
  { ENDSUBSECTION,     "endsubsection" },
  { PORT,              "port" },
  { MAGIC_DEVICE_NAME,   "devicename" },
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
  { N_SAMPLE_X,	       "numsamplex" },
  { N_SAMPLE_Y,	       "numsampley" },
  { -1,                "" },
};


#define LS1200		1
#define LS2400		2
#define LS4800		3
#define LS9600		4

static SymTabRec LinkSpeedTab[] = {
  { LS1200,	"b1200" },
  { LS2400,	"b2400" },
  { LS4800,	"b4800" },
  { LS9600,	"b9600" }
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
  { B1200, 8 },
  { B2400, 4 },
  { B4800, 2 },
  { B9600, 1 }
};
#endif /* XFREE86_V4 */


/*
 ***************************************************************************
 *
 * Default constants.
 *
 ***************************************************************************
 */
#define MAGIC_PACKET_SIZE	5
#define MAGIC_PORT		"/dev/magictouch"
#define MAGIC_LINK_SPEED	B9600

/* First byte of the packet */
#define MGCT_TOUCH	      0x01
#define MGCT_RKEY		      0x02
#define MGCT_LKEY		      0x04
#define MGCT_MKEY		      0x08
#define MGCT_CLICK_STATUS	0x10

#define MEDIE_X		20
#define MEDIE_Y		20


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
#define DBG(lvl, f) {if ((lvl) == debug_level) { f; } }
#else
#define DBG(lvl, f)
#endif


#ifdef XFREE86_V4
#undef SYSCALL
#undef read
#undef write
#undef close
#undef strdup
#define SYSCALL(call) call
#define read(fd, ptr, num) xf86ReadSerial(fd, ptr, num)
#define write(fd, ptr, num) xf86WriteSerial(fd, ptr, num)
#define close(fd) xf86CloseSerial(fd)
#define strdup(str) xf86strdup(str)
#endif


					



/*
 ***************************************************************************
 *
 * Device private records.
 *
 ***************************************************************************
 */
typedef struct _MagicPrivateRec {
  char	*input_dev;				/* The touchscreen input tty			*/
  int		min_x;				/* Minimum x reported by calibration		*/
  int		max_x;				/* Maximum x					*/
  int		min_y;				/* Minimum y reported by calibration		*/
  int		max_y;				/* Maximum y					*/
#ifndef XFREE86_V4
  int		link_speed;			/* Speed of the RS232 link connecting the ts.	*/
#endif
  int		screen_no;			/* Screen associated with the device		*/
  int		screen_width;			/* Width of the associated X screen		*/
  int		screen_height;			/* Height of the screen				*/
  int		swap_axes;			/* Swap X an Y axes if != 0 */
  unsigned char	packet_buf[MAGIC_PACKET_SIZE]; 	/* Assembly buffer				*/
  int		packet_pos;
  int		buf_x[MEDIE_X], i_x, num_medie_x;
  int		buf_y[MEDIE_Y], i_y, num_medie_y;
  Bool		first_x, first_y;
  Bool		first_entry;
  Bool		e_presente;
  Bool		click_on;
} MagicPrivateRec, *MagicPrivatePtr;


#ifndef XFREE86_V4
#if !defined(sun) || defined(i386)
/*
 ****************************************************************************
 * xf86MagicConfig --
 * 	Configure the device driver from configuration data
 ****************************************************************************
 */
static Bool
xf86MagicConfig(LocalDevicePtr    *array,
              int               inx,
              int               max,
	      LexPtr            val)
{
  LocalDevicePtr        local = array[inx];
  MagicPrivatePtr       priv = (MagicPrivatePtr)(local->private);
  int                   token;
  
  while ((token = xf86GetToken(MagicTab)) != ENDSUBSECTION) {
    switch(token) {
      
    case PORT:
      if (xf86GetToken(NULL) != STRING) {
	xf86ConfigError("MagicTouch input port expected");
      }
      priv->input_dev = strdup(val->str);	
      if (xf86Verbose) {
	ErrorF("%s MagicTouch input port: %s\n",
	       XCONFIG_GIVEN, priv->input_dev);
      }
      break;

    case MAGIC_DEVICE_NAME:
      if (xf86GetToken(NULL) != STRING) {
	xf86ConfigError("Magictouch device name expected");
      }
      local->name = strdup(val->str);
      if (xf86Verbose) {
	ErrorF("%s MagicTouch X device name: %s\n",
	       XCONFIG_GIVEN, local->name);
      }
      break;

    case SCREEN_NO:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("MagicTouch screen number expected");
      }
      priv->screen_no = val->num;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch associated screen: %d\n",
	       XCONFIG_GIVEN, priv->screen_no);
      }
      break;

    case LINK_SPEED:
      {
	int	ltoken = xf86GetToken(LinkSpeedTab);
	if (ltoken == EOF ||
	    ltoken == STRING ||
	    ltoken == NUMBER) {
	  xf86ConfigError("MagicTouch link speed expected");
	}
	priv->link_speed = LinkSpeedValues[ltoken-1].speed;
	if (xf86Verbose) {
	  ErrorF("%s MagicTouch link speed: %s bps\n",
		 XCONFIG_GIVEN, (LinkSpeedTab[ltoken-1].name)+1);
	}
      }
      break;
      
    case MAXX:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("MagicTouch maximum x position expected");
      }
      priv->max_x = val->num;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch maximum x position: %d\n",
	       XCONFIG_GIVEN, priv->max_x);
      }
     break;
      
    case MAXY:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("MagicTouch maximum y position expected");
      }
      priv->max_y = val->num;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch maximum y position: %d\n",
	       XCONFIG_GIVEN, priv->max_y);
      }
     break;
      
    case MINX:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("MagicTouch minimum x position expected");
      }
      priv->min_x = val->num;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch minimum x position: %d\n",
	       XCONFIG_GIVEN, priv->min_x);
      }
     break;
      
    case MINY:
      if (xf86GetToken(NULL) != NUMBER) {
        xf86ConfigError("MagicTouch minimum y position expected");
      }
      priv->min_y = val->num;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch minimum y position: %d\n",
	       XCONFIG_GIVEN, priv->min_y);
      }
     break;
      
    case DEBUG_LEVEL:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("MagicTouch driver debug level expected");
      }
      debug_level = val->num;
      if (xf86Verbose) {
#if DEBUG
	ErrorF("%s MagicTouch debug level sets to %d\n", XCONFIG_GIVEN,
	       debug_level);      
#else
	ErrorF("%s MagicTouch debug not available\n",
		XCONFIG_GIVEN, debug_level);      
#endif
      }
      break;

    case HISTORY_SIZE:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("MagicTouch motion history size expected");
      }
      local->history_size = val->num;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch motion history size is %d\n", XCONFIG_GIVEN,
	       local->history_size);
      }
      break;
	    
    case ALWAYS_CORE:
      xf86AlwaysCore(local, TRUE);
      if (xf86Verbose) {
	ErrorF("%s MagicTouch device will always stays core pointer\n",
	       XCONFIG_GIVEN);
      }
      break;

    case SWAP_AXES:
      priv->swap_axes = 1;
      if (xf86Verbose) {
	ErrorF("%s MagicTouch device will work with X and Y axes swapped\n",
	       XCONFIG_GIVEN);
      }      
      break;

    case N_SAMPLE_X:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("MagicTouch driver NumSamplesX expected");
      }
      priv->num_medie_x = val->num;
      
      
      if (priv->num_medie_x>MEDIE_X) 
      	  priv->num_medie_x=MEDIE_X;
      
      DBG(4,
      	ErrorF("NumSampleX = %d\n", priv->num_medie_x)
      	);
      
      break;
      
    case N_SAMPLE_Y:
      if (xf86GetToken(NULL) != NUMBER) {
	xf86ConfigError("MagicTouch driver NumSamplesY expected");
      }
      priv->num_medie_y = val->num;
      
      if (priv->num_medie_y>MEDIE_Y) 
      	  priv->num_medie_y=MEDIE_Y;
      	  
      DBG(4,
      	ErrorF("NumSampleY = %d\n", priv->num_medie_y)
      	);
      break;

    case EOF:
      FatalError("Unexpected EOF (missing EndSubSection)");
      break;

    default:
      xf86ConfigError("MagicTouch subsection keyword expected");
      break;
    }
  }

  if (priv->max_x - priv->min_x <= 0) {
    ErrorF("%s MagicTouch: reverse x mode (minimum x position >= maximum x position)\n",
	   XCONFIG_GIVEN);
  }  
  if (priv->max_y - priv->min_y <= 0) {
    ErrorF("%s MagicTouch: reverse y mode (minimum y position >= maximum y position)\n",
	   XCONFIG_GIVEN);
  }
    
  DBG(2, ErrorF("xf86MagicConfig port name=%s\n", priv->input_dev))

  return Success;
}

#endif /* !defined(sun) || defined(i386) */
#endif /* XFREE86_V4 */

/****************************************************************************
 *
 * xf86MagicQueryOK --
 *	Testa la presenza del touch controller. 
 * 	Si osserva che al primo accesso al touch dopo l'accensione e' 
 *	presente nel buffer di ricezione il codice 0xF che identifica la
 *	vera presenza del touch controller.
 *	Dal secondo accesso in poi bisogna interrogare il touch controller
 * 	per verificarne l'esistenza. 
 ****************************************************************************
 */
static Bool
xf86MagicQueryOK(int fd)
{
	Bool	ok;
	int	result;
	char 	buf;

	ok = Success;

	/* Provo a leggere un byte dal buffer di ricezione */
	SYSCALL( result = read(fd, &buf, 1) );
	
	DBG(4, ErrorF("<<%s[%d]>> QueryOK: read --> %d\n", __FILE__, __LINE__, result) );
	
	/* Se result e' -1 vuol dire che non c'e' nessun carattere nel
	 buffer. Allora X/Window e' stato avviato almeno una volta */
	if (result<0) {
		DBG(4,
			ErrorF("Avvio n-esimo di X/Windows\n");
			ErrorF("Controllo presenza Touch Controller\n")
		);
		
		/* Cerco il touch controller. Invio il carattere 0x00. */
		buf = 0;
		SYSCALL( result = write(fd, &buf, 1) );
		
		/* Attendo 20 ms per dare il tempo al touch controller di
		capire il comando */
		usleep(20000);
		
		/* Leggo la risposta */
		SYSCALL( result = read(fd, &buf, 1) );

		DBG(4,
			ErrorF("QueryOK: buf==%X,  result==%d\n", buf, result)
		);
	}
		
	/* Se result<0 allora il touch controller non e' presente sul 
	disposito. Non posso proseguire */
	if (result<0) {
		DBG(4,
			ErrorF("<<%s[%d]>> result<0\n", __FILE__, __LINE__)
		);
		ok = !Success;
	}
	/* Se il touch controller ha risposto allora controllo cosa ha 
	risposto */
	else {
		ok = (buf==0xF ? Success : !Success);
		DBG(4,
			ErrorF("<<%s[%d]>> QueryOK buf==%x\n", __FILE__, __LINE__, buf)
		);
	}

	return ok;
}

/*
 ***********************************************************************
 *
 * xf86MagicControl
 *
 ***********************************************************************
 */
static Bool
xf86MagicControl(DeviceIntPtr dev,
		int mode)
{
	LocalDevicePtr	local = (LocalDevicePtr) dev->public.devicePrivate;
	MagicPrivatePtr	priv = (MagicPrivatePtr)(local->private);
	unsigned char	map[] = { 0, 1 };
	unsigned char	req[MAGIC_PACKET_SIZE], replay[MAGIC_PACKET_SIZE];
	int            status_line;
	
	switch (mode) {
		case DEVICE_INIT:
			DBG(2, ErrorF("MagicTouch init...\n") );
			
			/* Controlla il numero di schermo selezionato */
			if (priv->screen_no >= screenInfo.numScreens || priv->screen_no<0)
				priv->screen_no = 0;
			/* Legge le dimensioni dello schermo */
			priv->screen_width = screenInfo.screens[priv->screen_no]->width;
			priv->screen_height = screenInfo.screens[priv->screen_no]->height;
			
			if (InitButtonClassDeviceStruct(dev, 1, map)==FALSE) {
				ErrorF("Impossibile allocare ButtonClassDeviceStruct per MagicTouch\n");
				return !Success;
			}
			
			if (InitFocusClassDeviceStruct(dev)==FALSE) {
				ErrorF("Impossibile allocare FocusClassDeviceStruct per MagicTouch\n");
				return !Success;
			}
			
			/*
			 * Il movimento viene eseguito su due assi in coordinate assolute.
			 */
			if (InitValuatorClassDeviceStruct(dev, 2, xf86GetMotionEvents, local->history_size, Absolute) == FALSE ) 
			{
				ErrorF("MagicTouch ValuatorClassDeviceStruct: ERRORE\n");
				return !Success;
			}
			else {
				InitValuatorAxisStruct(dev, 0, priv->min_x, priv->max_x, 
							9500, 
							0, 	/* min res */
							9500	/* max res */);
							
				InitValuatorAxisStruct(dev, 1, priv->min_y, priv->max_y,
							10500,
							0,
							10500);
			}
			
			if (InitFocusClassDeviceStruct(dev)==FALSE) {
				ErrorF("Impossibile allocare FocusClassDeviceStruct per MagicTouch\n");
			}
			
			/*
			 * Alloca il buffer degli eventi spostamento
			 */
			xf86MotionHistoryAllocate(local);
			
#ifndef XFREE86_V4
			AssignTypeAndName(dev, local->atom, local->name);
#endif /* XFREE86_V4 */
			
			DBG(2, ErrorF("MagicTouch INIT OK\n") );
			
			break; /* DEVICE_INIT*/
			
		case DEVICE_ON:
			DBG(2, ErrorF("MagicTouch ON\n") );
			if (local->fd<0) {
#ifndef XFREE86_V4
			struct termios termios_tty;
			int 		i,result;
#endif
			
			DBG(2, ErrorF("Opening device...\n") );
			
#ifdef XFREE86_V4
			local->fd = xf86OpenSerial(local->options);
			if (local->fd<0) {
				ErrorF("Impossibile aprire MagicTouch\n");
				return !Success;
			}
#else
			SYSCALL( local->fd = open(priv->input_dev, O_RDWR | O_NDELAY, 0) );
			if (local->fd<0) {
				Error("Impossibile aprire MagicTouch\n");
				return !Success;
			}
			
			DBG(3, ErrorF("Provo a configurare il MagicTouch\n") );
			
			
			memset(&termios_tty, 0, sizeof(termios_tty) );
			termios_tty.c_iflag = 0;
			termios_tty.c_cflag = priv->link_speed | CS8 | CREAD | CLOCAL;
			termios_tty.c_oflag = 0;
			termios_tty.c_lflag = 0;
			termios_tty.c_cc[VTIME]=0;
			termios_tty.c_cc[VMIN]=1;
			
			/*
			 * Attivo l'RTS per abilitare il touch controller
			 */
#if 0
         SYSCALL( result = ioctl(local->fd, TIOCMGET, &status_line) );
			if (result<0) 
			{
				Error("Impossibile leggere stato linee seriale\n");
				close(local->fd);
				return !Success;
			}
			status_line |= TIOCM_RTS;
			SYSCALL( result = ioctl(local->fd, TIOCMSET, &status_line) );
			if (result<0) 
			{
				Error("Impossibile settare stato linee seriale\n");
				close(local->fd);
				return !Success;
			}
#endif
			
			SYSCALL( result = tcsetattr(local->fd, TCSANOW, &termios_tty) );
			if (result<0) 
			{
				Error("Impossibile configurare MagicTouch\n");
				close(local->fd);
				return !Success;
			}
#endif
			
								
			/* Controlla se e' presente il touch controller.*/
			req[0] = 0x00;
			if (xf86MagicQueryOK(local->fd)!=Success) {
				ErrorF("MagicTouch not present\n");
				close(local->fd);
				return !Success;
			}

			priv->e_presente = TRUE;			
			
			AddEnabledDevice(local->fd);
			dev->public.on = TRUE;			
			} /* if (local->fd<0) */
			break; /* DEVICE_ON */
			
		case DEVICE_CLOSE:
		case DEVICE_OFF:
			DBG(2, ErrorF("MagicTouch OFF\n") );
			dev->public.on = FALSE;
			if (local->fd>=0)
			   emoveEnabledDevice(local->fd);
				
			SYSCALL( close(local->fd) );
			local->fd = -1;
			DBG(2, ErrorF("OK\n") );
			break; /* DEVICE_OFF*/
			
		default:
			ErrorF("unsupported mode %d\n", mode);
			return !Success;
	} /* switch (mode) */
	
	return Success;
}



/*
 ***************************************************************************
 *
 * GetPacket --
 *
 ***************************************************************************
 */
static Bool
GetPacket(LocalDevicePtr local,  unsigned char *buffer, int *n_rx, int fd)
{
 	int	num_bytes;
 	int  	i;
	Bool	ok;
	MagicPrivatePtr  priv=(MagicPrivatePtr) local->private;

	DBG(6, ErrorF("Entering GetPacket with packet_pos == %d\n", *n_rx) );
	
	SYSCALL( 
		num_bytes=read(fd, buffer+*n_rx, MAGIC_PACKET_SIZE-*n_rx) 
	);
	
	/* Se e' il primo ingresso nella procedura e ho letto un solo byte,
	   allora e' arrivato lo 0x0F di risposta all-inizializzazione del
	   touch controlloer */
	/* Sto gia' leggendo un pacchetto normale */		
	*n_rx += num_bytes;
	
	DBG(8,
		for (i=0; i<*n_rx; i++)
			ErrorF("%3X", buffer[i]);
		ErrorF("\n")
	);

	ok = (*n_rx==MAGIC_PACKET_SIZE ? Success : !Success );
	
	if (ok==Success)
		*n_rx = 0;
	
	DBG(6, 
		if(ok==Success) 
			ErrorF("GetPacket OK\n");
		else
			ErrorF("GetPacket FAIL\n")
	);
	  
  	return ok;
}

/*
 ************************************************************************
 *
 * xf86MagicReadInput
 *
 ************************************************************************
 */
static 
int medie_x(LocalDevicePtr local, int x)
{
	int i,res;
	float medie;
	MagicPrivatePtr priv = (MagicPrivatePtr)(local->private);
	
	DBG(6, 
		ErrorF("Medie in X = %d\n", priv->num_medie_x)
	);
	
	if (priv->first_x) {
		priv->first_x = FALSE;
		for (i=0; i<priv->num_medie_x; i++)
			priv->buf_x[i] = x;
			
		res = x;
	}
	else {
		priv->buf_x[priv->i_x] = x;
		priv->i_x++;
		if (priv->i_x>=priv->num_medie_x)
			priv->i_x = 0;
			
		medie = 0.0;	
		for (i=0; i<priv->num_medie_x; i++)
			medie += priv->buf_x[i];
		
		res = (int)(medie/priv->num_medie_x);
	}
	
	return res;
}

static
int medie_y(LocalDevicePtr local, int y)
{
	int i,res;
	float medie;
	MagicPrivatePtr priv = (MagicPrivatePtr)(local->private);
	
	DBG(6, 
		ErrorF("Medie in Y = %d\n", priv->num_medie_y)
	);
	
	if (priv->first_y) {
		priv->first_y = FALSE;
		for (i=0; i<priv->num_medie_y; i++)
			priv->buf_y[i] = y;
			
		res = y;
	}
	else {
		priv->buf_y[priv->i_y] = y;
		priv->i_y++;
		if (priv->i_y>=priv->num_medie_y)
			priv->i_y = 0;
			
		medie = 0.0;	
		for (i=0; i<priv->num_medie_y; i++)
			medie += priv->buf_y[i];
		
		res = (int)(medie/priv->num_medie_y);
	}
	
	return res;
}

/*
static
int MAX(int x, int y)
{
  return (x>=y ? x : y);
}
*/

#define MAX(x,y) (x>=y ? x : y)
 
static void
xf86MagicReadInput(LocalDevicePtr	local)
{
	MagicPrivatePtr	priv = (MagicPrivatePtr)(local->private);
	int		cur_x, cur_y;
	Bool		touch_now;
	static int	n_coms = 0;

	if (!priv->e_presente) {
		DBG(4,
			ErrorF("<<%s[%d]>> ReadInput: Touch Controller non inizializzato\n")
		);
		return;
	}
		
	DBG(4, ErrorF("Entering ReadInput\n"));
  	/*
  	 * Try to get a packet.
  	 */
  	if (GetPacket(local, priv->packet_buf, &priv->packet_pos, local->fd)==Success) 
  	{
  		/* Calculate the (x,y) coord of pointer */
  		cur_x = priv->packet_buf[1];
  		cur_x <<= 6;
  		cur_x |= priv->packet_buf[2];
  		
  		cur_y = priv->packet_buf[3];
  		cur_y <<= 6;
  		cur_y |= priv->packet_buf[4];
  		
		touch_now = priv->packet_buf[0] & MGCT_TOUCH == MGCT_TOUCH;
		
		/* Se c'e' pressione sul touch inizio a calcolare la posizione
		   e a spostare il cursore grafico */
		if (touch_now) {
			DBG(6, 
				ErrorF("Touch premuto: medio i valori di posizione\n")
			);
			cur_x = medie_x(local, cur_x);
			cur_y = medie_y(local, cur_y);
		}
		else {
			DBG(6, 
				ErrorF("Touch rilasciato:\n"
					"\tazzeramento buffer memoria\n"
					"\tposizionamento immediato\n")
			);
			
			/* Se non ho pressione allora comando lo spostamento
			del cursore senza mediare. Svuoto il buffer delle medie */
			priv->first_x = TRUE;
			priv->first_y = TRUE;
		}
		/* Comando lo spostamento */
		xf86PostMotionEvent(local->dev, TRUE, 0, 2, cur_x, cur_y);		
    		/* comanda la pressione del tasto */
    		
    		DBG(9,
    			ErrorF("touch_now==%s\n", (touch_now==TRUE ? "TRUE" : "FALSE") )
    		);
    		if (touch_now!=priv->click_on) {
    			DBG(9,
    				ErrorF("Bottone == %s\n", (touch_now==TRUE ? "PREMUTO" : "RILASCAITO") )
    			);
    			priv->click_on = touch_now;
			xf86PostButtonEvent(local->dev, TRUE, 1, touch_now, 0, 2, cur_x, cur_y);
		}
  	} /* GetPacket */
}


/*
 ************************************************************************
 *
 * xf86MagicConvert
 *
 ************************************************************************
 */
static Bool
xf86MagicConvert(LocalDevicePtr	local,
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
  MagicPrivatePtr	priv = (MagicPrivatePtr) local->private;
  int		width = priv->max_x - priv->min_x;
  int		height = priv->max_y - priv->min_y;
  int		input_x, input_y;
  
  if (first != 0 || num != 2) {
    return FALSE;
  }

  DBG(3, ErrorF("MagicConvert: v0(%d), v1(%d)\n",	v0, v1));

  if (priv->swap_axes) {
    input_x = v1;
    input_y = v0;
  }
  else {
    input_x = v0;
    input_y = v1;
  }
  *x = (priv->screen_width * (input_x - priv->min_x)) / width;
  *y = (priv->screen_height - (priv->screen_height * (input_y - priv->min_y)) / height);
  
#ifdef XFREE86_V4
  /*
   * Need to check if still on the correct screen.
   * This call is here so that this work can be done after
   * calib and before posting the event.
   */
  xf86XInputSetScreen(local, priv->screen_no, *x, *y);
#endif
  
  DBG(3, ErrorF("MagicConvert: x(%d), y(%d)\n",	*x, *y));

  return TRUE;
}



/*
 ************************************************************************
 *
 * xf86MagicAllocate
 * 
 ************************************************************************
 */
static LocalDevicePtr
#ifndef XFREE86_V4
xf86MagicAllocate(void)
#else
xf86MagicAllocate(InputDriverPtr drv)
#endif
{
#ifndef XFREE86_V4
	LocalDevicePtr	local = (LocalDevicePtr) xalloc( sizeof(LocalDeviceRec) );
#else
	LocalDevicePtr	local = xf86AllocateInput(drv, 0);
#endif

	MagicPrivatePtr	priv = (MagicPrivatePtr) xalloc( sizeof(MagicPrivateRec) );

	/* Controlla la corretta allocazione di buffers. Se uno dei buffers non
		e' stato allocato correttamente termina l'inizializzazione
	*/
	if (!local) {
		if (priv)
			xfree(priv);
		return NULL;
	}
	
	if (!priv) {
		if (local)
			xfree(local);
		return NULL;
	}
	
	/* I buffers sono allocati correttamente */
#ifdef XFREE86_V4
	priv->input_dev = strdup(MAGIC_PORT);
#else
	priv->input_dev = MAGIC_PORT;
	priv->link_speed = MAGIC_LINK_SPEED;
#endif
	
	priv->min_x = 60;
	priv->max_x = 960;
	priv->min_y = 60;
	priv->max_y = 960;
	priv->screen_no = 0;
	priv->screen_width = -1;
	priv->screen_height = -1;
	priv->swap_axes = 0;
	priv->first_x = 
	priv->first_y = TRUE;
	priv->first_entry = TRUE;
	priv->e_presente = FALSE;
	priv->click_on = FALSE;
	priv->i_x = 
	priv->i_y = 0;
	priv->packet_pos = 0;
	bzero(priv->buf_x, MEDIE_X);
	bzero(priv->buf_y, MEDIE_Y);
	priv->num_medie_x = MEDIE_X;
	priv->num_medie_y = MEDIE_Y;
	
	local->name = XI_TOUCHSCREEN;
	local->flags = 0;
	
#ifndef XFREE86_V4
#if !defined(sun) || defined(i386)
	local->device_config = xf86MagicConfig;	
#endif /* !defined(sun) || defined(i386) */	
#endif /* XFREE86_V4*/
	
	local->device_control = xf86MagicControl;
	local->read_input = xf86MagicReadInput;
	local->control_proc = NULL;
	local->close_proc = NULL;
	local->switch_mode = NULL;
	local->conversion_proc = xf86MagicConvert;
	local->reverse_conversion_proc = NULL;
	local->fd = -1;
	local->atom = 0;
	local->dev = NULL;
	local->private = priv;
	local->type_name = "MagicTouch";
	local->history_size = 0;
	
	return local;
	
} /* xf86MagicAllocae */



#ifndef XFREE86_V4

/*
 * Sezione relativa a X < 4.0.0
 */

DeviceAssocRec magictouch_assoc = {
	"magictouch",
	xf86MagicAllocate
};

#ifdef DYNAMIC_MODULE

#ifndef DLSYM_BUG
int init_module(unsigned long server_version)
#else
int init_xf86Magic(unsigned long server_version)
#endif
{
	/* Aggiunge l'assiocazione per il touchscreen */
	xf86AddDeviceAssoc(&magictouch_assoc);
	
	/* Controlla la versione */ 
	if (server_version != XF86_VERSION_CURRENT) {
		ErrorF("Warining: MagicTouch module compiled for version %s\n", XF86_VERSION);
		return 0;
	}
	return 1;
}
#endif /* DYNAMIC_MODULE */

#else
/*
 * Sezione relativa a X >= 4.0.0
 */

#endif /* XFREE86_V4 */
