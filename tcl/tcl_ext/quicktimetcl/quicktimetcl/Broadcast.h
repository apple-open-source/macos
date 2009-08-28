/* 
 * Broadcast.h --
 *
 *      This module implements a "broadcast" widget that is object
 *      based. It is part of the QuickTimeTcl package.
 *      It provides an interface to the (ill docoumented) QuickTime broadcaster
 *      API's. This includes the Presentation and Sourcer classes of APIs.
 *
 * Copyright (c) 2003 Mats Bengtsson
 *
 * $Id: Broadcast.h,v 1.2 2003/09/28 06:14:51 matben Exp $
 */

#ifndef INCLUDED_BROADCAST_H
#define INCLUDED_BROADCAST_H

#include "QuickTimeTcl.h"
#if TARGET_OS_MAC
#	if TARGET_API_MAC_CARBON
#	else
#		include <QuickTimeStreaming.h>
#		include <QTStreamingComponents.h>
#	endif
#endif


#define kDefaultPresTimeScale		600

#define kReturnChar                 '\r'
#define kNewlineChar                '\n'

/* QuickTime Streaming Errors */
/*
enum {
    qtsBadSelectorErr           = -5400,
    qtsBadStateErr              = -5401,
    qtsBadDataErr               = -5402,
    qtsUnsupportedDataTypeErr   = -5403,
    qtsUnsupportedRateErr       = -5404,
    qtsUnsupportedFeatureErr    = -5405,
    qtsTooMuchDataErr           = -5406,
    qtsUnknownValueErr          = -5407,
    qtsTimeoutErr               = -5408,
    qtsConnectionFailedErr      = -5420,
    qtsAddressBusyErr           = -5421
};
*/

/*
 * Constants for the 'state' and 'targetState' members of MovieBroadcast.
 */

typedef enum PresentationState {
	kPresentationStateIdle          = 1,    /* Initial state. */
	kPresentationStateStartingPreview,
	kPresentationStatePreviewing,
	kPresentationStateStartingPreroll,
	kPresentationStateReadyToPlay,	        /* Sucessfully prerolled */
	kPresentationStateStarting,
	kPresentationStatePlaying
} PresentationState;

/*
 * Keep record for each sourcer.
 * Each broadcaster may have a number of sourcers associated with it.
 */
 
typedef struct BcastSourcerInfo {
    struct BcastSourcerInfo    *next;
    QTSPresentation     presentation;
    QTSStream           stream;
    Component           component;
    ComponentInstance   sourcer;
    OSType              trackType;
	Track				track;
	Boolean				done;
} BcastSourcerInfo;

/*
 * A data structure of the following type is kept for each
 * widget managed by this file:
 */

typedef struct {
    Tk_Window           tkwin;		/* Window that embodies the widget.  NULL
				                     * means window has been deleted but
				                     * widget record hasn't been cleaned up yet. */
    Display             *display;   /* X's token for the window's display. */
    Tcl_Interp          *interp;    /* Interpreter associated with widget. */
    Tcl_Command         widgetCmd;	/* Token for square's widget command. */
    Tk_OptionTable      optionTable;/* Token representing the configuration
				                     * specifications. */
    /*
     * Widget specific members.
     */
     			 
    int                 height;
    int                 width;
    char                *command;
    long                flags;
    QTSPresentation     presentation;
    Tcl_Obj             *sdpListPtr;
    double              targetFrameRate;
    UInt32              targetDataRate;
    int                 haveAudioStream;
    int                 haveVideoStream;
    int                 haveTextStream;
    long                state;
    long                targetState;
	GWorldPtr           grafPtr;    /* We save the actual graf port so we don't 
									 * need to call QTSPresSetGWorld. */
    BcastSourcerInfo   	*sourcerInfoListPtr;
    
    int                 indSize;
    Tcl_Obj             *sizeObjPtr;
    int                 srcWidth;
    int                 srcHeight;	
} MovieBroadcast;

/*
 * We need a (single) linked list for our broadcasters.
 */

typedef struct MovieBroadcastList {
    struct MovieBroadcastList    *next;
    MovieBroadcast           *bcastPtr;
} MovieBroadcastList;


/*
 * Flag bits for grabber used in 'flags' in MovieBroadcast struct:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *						has already been queued to redraw
 *						this window.
 * NEWGWORLD:			Non-zero means this widget has a new GWorld, and that we need
 *						to associate the sequence grabber with this GWorld. Need to
 *						know when displaying.
 */

#define REDRAW_PENDING 	0x0001
#define NEWGWORLD 		0x0002


#endif INCLUDED_BROADCAST_H
