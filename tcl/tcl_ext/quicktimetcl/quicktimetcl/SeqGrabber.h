/*
 * SeqGrabber.h --
 *
 * Copyright (c) 2000-2005  Mats Bengtsson
 *
 * $Id: SeqGrabber.h,v 1.8 2005/04/16 13:23:26 matben Exp $
 */

#ifndef INCLUDED_SEQGRABBER_H
#define INCLUDED_SEQGRABBER_H

#ifdef _WIN32
#   include "QuickTimeTclWin.h"
#endif  

#include "QuickTimeTcl.h"
 

/*
 * A data structure of the following type is kept for each
 * sequence grabber that currently exists for this process:
 */

typedef struct SeqGrabber {
	Tk_Window 		tkwin; 				/* Window for Sequence Grabber. */
	Display 		*display;		 	/* Display containing widget. */
	Tcl_Interp 		*interp;			/* Interpreter for widget. */
	Tcl_Command 	widgetCmd;			/* Sequence grabber instance command. */
	Tk_OptionTable 	optionTable;
	
	/*
	 * Sequence Grabber specific attributes.
	 */
	 
	SeqGrabComponent	seqGrab;		/* Sequence grabber component. */
	SGChannel		sgChanVideo;		/* Video and audio sequence grabber channels. */
	SGChannel		sgChanAudio;
	char 			*filename;			/* File name to save movie to. */
	Boolean		    willRecord;			/* Recording things we grab? */
	int 			sgWidth;			/* Sequence Grabber width and height without padding. */
	int 			sgHeight;
	short 			srcWidth;			/* The video source width and height which */
	short 			srcHeight;			/* is identical to the max size. */
	int 			videoWidth;			/* The video width and height describes the zoomed */
	int 			videoHeight;		/* in rectangle of the source rectangle above. */
	int 			width;				/* Width of widget; only if option set. */
	int 			height;				/* Height of widget; only if option set. */
	double			zoom;				/* Zoom factor, >= 1.0 */
	int				indSize;			/* Index to either "full", "half", or "quarter". */
	int				indQuality;			/* Playback quality. */
	int             audio;          	/* Boolean; shall we have audio. */
	int             audioOnly;          /* Boolean; wants audio only. */
	int             playDuringRecord;   /* Boolean; preview while recording. */
	int             frameCount;
	int             showFPS;
	UInt32          startTick;
	UInt32          latestTick;
	int 			padx;				/* X padding */
	int		 		pady;				/* Y padding */
	XColor 			*background;		/* background color */
	int				videoBottle;		/* Do we use a videobottleneck? */
	CGrafPtr		videoBottlePortPtr; /* Temporary port for overlaying graphics in video. */
	char			*overlayimage;		/* Name of tk image photo to overlay the video. */
	PicHandle		overlayPictHand;	/* The overlay tk image as a Picture. */
	PicHandle       updatePictHand;     /* When stopped or paused, have this Pict for update events. */
    Tcl_Obj			*imageAsyncProcObj; /* These are all used for taking images async. */
    Tcl_Obj			*imageNameObj;
    void 			*asyncImageHandlerPtr;
	int 			flags;				/* Various status flags; fields defined below. */
    char			*videoCompressor;   /* Video compressor quality. */
    double          volume;             /* Channel volume: -1.0 - +1.0. */
    double			frameRate;
#ifdef _WIN32
	LONG	    	winEventProc;       /* Original event procedure (Windows). */
#endif
} SeqGrabber;


/*
 * Flag bits for grabber used in 'flags' in SeqGrabber struct:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *						has already been queued to redraw
 *						this window.
 * NEWGWORLD:			Non-zero means this widget has a new GWorld, and that we need
 *						to associate the sequence grabber with this GWorld. Need to
 *						know when displaying.
 * UPDATEGRABBER:		This flag bit is set when received an Expose (update) event in the
 *						tcl event procedure.
 * ISRUNNING:			Set if the grabber is not paused or stopped.
 */

#define REDRAW_PENDING 			(1L << 0)
#define NEWGWORLD 				(1L << 1)
#define UPDATEGRABBER 			(1L << 2)
#define ISRUNNING	 			(1L << 3)


#endif	// INCLUDED_SEQGRABBER_H
