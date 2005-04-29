/*The Panache Window Manager*/
/*By George Peter Staplin*/
/*Please read the LICENSE file included with the Panache distribution 
 *for usage restrictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#ifndef __STDC__
	#include <malloc.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <tcl.h>  
#include "PanacheWindowList.h"

/*Style 
I use if (returnFromFunc == 1) instead of if (returnFromFunc)
I use if (returnFromFunc == 0) instead of if (!returnFromFunc) 
*/  

/*Automatic focus of new windows yes/no.*/
/*Automatic focus of transient windows yes/no.*/

#define PANACHE_DIRECTORY "Panache"
#define CMD_ARGS (ClientData clientData, Tcl_Interp *interp, \
int objc, Tcl_Obj *CONST objv[])
		
Display *dis;
XEvent report;
Window root;
Tcl_Interp *interp;
int distance_from_edge = 0; 
Window mapped_window = None;
int screen;
Atom _XA_WM_STATE;  
Atom _XA_WM_PROTOCOLS;
Atom _XA_WM_DELETE_WINDOW; 
Window workspace_manager;
struct CList *keepAboveWindowList;
unsigned long eventMask = (ResizeRedirectMask | PropertyChangeMask | \
	EnterWindowMask | LeaveWindowMask | FocusChangeMask | KeyPressMask);
				
#define winIdLength 14
/*#define FORK_ON_START*/

int PanacheGetWMState (Window win);
void PanacheSelectInputForRootParented (Window win);
void PanacheConfigureNormalWindow (Window win, unsigned long value_mask);

char Panache_Init_script[] = {
	"if {[file exists $prefix/$panacheDirectory/Panache.tcl] != 1} {\n" 
	"	puts stderr  {unable to open Panache.tcl  Did you run make install?}\n" 
	"	puts stderr \"I looked in $prefix/$panacheDirectory\"\n"
	"	exit -1\n" 
	"}\n"
	"proc sendToPipe str {\n"
	"	set str [string map {\"\n\" \"\" \"\r\" \"\"} $str]\n"
	"	puts $::pipe $str\n"
	"	flush $::pipe\n"
	"}\n"
	"proc getFromPipe {} {\n"
	"	gets $::pipe line\n" 
	"	if {$line != \"\"} {\n"
	"		set cmd [lindex $line 0]\n"
	"		if {[llength $line] == 2} {\n" 
	"			$cmd [lindex $line 1]\n"
	"		} else {\n"
	"			eval $line\n" 
	"		}\n"
	"	}\n"
	"}\n"
	"set ::pipe [open \"|$wishInterpreter $prefix/$panacheDirectory/Panache.tcl\" w+]\n"
	"fconfigure $::pipe -blocking 0\n"
	"\n"};

		
char *charMalloc (int size) {
	char *mem = NULL;
	
	mem = (char *)	malloc ((sizeof (char)) * size); 
	
	if (mem == NULL) {
		fprintf (stderr, "malloc failed to allocate memory  This means that Panache \
and other applications could have problems if they continue running.\n\n  \
exiting Panache now!");
		exit (-1);
	}
	
	return mem;
}


void sendConfigureNotify (Window win, unsigned long value_mask, XWindowChanges *winChanges) {
	XEvent xe;
	XWindowAttributes wattr;

	if (XGetWindowAttributes (dis, win, &wattr) == 0) {
		return;
	}

	xe.type = ConfigureNotify;	
	xe.xconfigure.type = ConfigureNotify;
	xe.xconfigure.event = win;
	xe.xconfigure.window = win;
	
	
	xe.xconfigure.x = (value_mask & CWX) ? winChanges->x : wattr.x;
	xe.xconfigure.y = (value_mask & CWY) ? winChanges->y : wattr.y;	
	xe.xconfigure.width = (value_mask & CWWidth) ? winChanges->width : wattr.width;
	xe.xconfigure.height = (value_mask & CWHeight) ? winChanges->height : wattr.height;
			
	xe.xconfigure.border_width = 0;
	xe.xconfigure.above = None;
	xe.xconfigure.override_redirect = 0;
	
	XSendEvent (dis, win, 0, StructureNotifyMask, &xe);
	
	XFlush (dis);	
}


void sendMapNotify (Window win) {
	XEvent mapNotify;

	mapNotify.type = MapNotify;
	mapNotify.xmap.type = MapNotify;
	mapNotify.xmap.window = win;
	mapNotify.xmap.display = dis;
	mapNotify.xmap.event = win;
	XSendEvent (dis, win, 0, StructureNotifyMask, &mapNotify);
	XFlush (dis);
}


int PanacheAddAllWindowsCmd CMD_ARGS {
	Window dummy;
	Window *children = NULL; 
	unsigned int nchildren;
	unsigned int i; 
	char *winId;
	char *transientForWinId;
	char str[] = "sendToPipe [list add [list $winTitle] $winId $winType $transientForWinId]";
	Window twin;
	
	XSync (dis, 0);
	/*XGrabServer (dis);*/
		
	if (XQueryTree (dis, 
			root, 
			&dummy, 
			&dummy, 
			&children, 
			&nchildren) == 0) {
	
		fprintf (stderr, "Error querying the tree for the root window.\n");
	}

	for (i = 0; i < nchildren; i++) {
		XTextProperty xtp;
		XWMHints *wmHints = XGetWMHints (dis, children[i]);
		XWindowAttributes wattr;
		
		xtp.value = NULL;
		
		if (wmHints == NULL) {
			continue;
		}
		
		if (wmHints->flags & IconWindowHint) {
			continue;
		}
		
		if (XGetWindowAttributes (dis, children[i], &wattr) == 0) {
			continue;
		}
		
		if (wattr.override_redirect == 1) {
			continue;
		}
			
		if (wmHints->flags & StateHint) { 
			if (wmHints->initial_state & WithdrawnState) {
				continue;
			} else if (wattr.map_state == 0 && PanacheGetWMState (children[i]) == 0) {
				continue;
			}  
		} 			
	
		XFree (wmHints);
		
		XGetWMName (dis, children[i], &xtp);
			
		winId = charMalloc (winIdLength);

		sprintf (winId, "%ld", children[i]);
		Tcl_SetVar (interp, "winTitle", (char *) xtp.value, 0);
		Tcl_SetVar (interp, "winId", winId, 0);

		if (XGetTransientForHint (dis, children[i], &twin) == 1) {
			Tcl_SetVar (interp, "winType", "transient", 0);
	
			transientForWinId =  charMalloc (winIdLength);
			sprintf (transientForWinId, "%ld", twin);
			Tcl_SetVar (interp, "transientForWinId", transientForWinId, 0);
			free (transientForWinId);
			
			PanacheSelectInputForRootParented (children[i]);
	
		} else {
			Tcl_SetVar (interp, "winType", "normal", 0);
			Tcl_SetVar (interp, "transientForWinId", "", 0);
				
			/*Maybe I should compare the first char and then do strcmp?*/	
			if (xtp.value != NULL && strcmp ((char *)xtp.value, "___Panache_GUI") != 0) {
				PanacheConfigureNormalWindow (children[i], CWX|CWY|CWWidth|CWHeight);	
				PanacheSelectInputForRootParented (children[i]);
			} 
		}
		
		XFree (xtp.value);
		free (winId);

		if (Tcl_Eval (interp, str) != TCL_OK) {
			fprintf (stderr, "Error in PanacheAddAllWindowsCmd: %s\n", Tcl_GetStringResult (interp));
		}
	}

	if (children != NULL) {
		XFree (children);
	}

	/*XUngrabServer (dis);*/ 
	XSync (dis, 0);

	return TCL_OK;
}


void PanacheConfigureRequest (XConfigureRequestEvent *event) {
	XWindowChanges wc;
	Window twin;
	int maxWidth;
	int maxHeight;

	if (event->parent != root) { 
		return;
	}

#ifdef DEBUG		
	fprintf (stderr, "ConfigureRequest win %ld\n", event->window);
	fprintf (stderr, "CWSibling %d\n", (event->value_mask & CWSibling) == 1);
	fprintf (stderr, "CWStackMode %d\n", (event->value_mask & CWStackMode) == 1);
#endif
	
	maxWidth = (DisplayWidth (dis, screen) - distance_from_edge - 4);
	maxHeight = DisplayHeight (dis, screen);
		
	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	
	if (event->window == workspace_manager) {
		wc.width = distance_from_edge;
		wc.height = maxHeight;
		wc.x = 0;
		wc.y = 0;	
		XConfigureWindow(dis, event->window, CWX|CWY|CWWidth|CWHeight, &wc);
		sendConfigureNotify (event->window, CWX|CWY|CWWidth|CWHeight, &wc);
		return;
	} else {
		PanacheSelectInputForRootParented (event->window);
	} 
	
	if (XGetTransientForHint (dis, event->window, &twin) == 1) {
		if (event->width > maxWidth) {
			wc.width = maxWidth;
		} else {
			wc.width = event->width;
		} 
	
		wc.height = event->height;
			
		if (event->x < distance_from_edge) {
			wc.x = distance_from_edge;
		} else {
			wc.x = event->x;		
		}	 
	
		wc.y = event->y;		
		XConfigureWindow (dis, event->window, event->value_mask, &wc);
		sendConfigureNotify (event->window, event->value_mask, &wc);
	} else {
		PanacheConfigureNormalWindow (event->window, event->value_mask);
	}
				
	XFlush (dis);
}


/*This configures the window and sends a ConfigureNotify event. 
 *It's designed for normal non-transient windows
 */
void PanacheConfigureNormalWindow (
	Window win, unsigned long value_mask) 
{
	XWindowChanges wc;
	XSizeHints sizeHints;
	long ljunk = 0;
	int maxWidth = (DisplayWidth (dis, screen) - distance_from_edge - 4);
	int maxHeight = DisplayHeight (dis, screen);

	wc.border_width = 0;
	wc.sibling = None;
	wc.stack_mode = Above;
	
	wc.x = distance_from_edge;
	wc.y = 0;	
	wc.width = maxWidth;
	wc.height = maxHeight;
	
	if (XGetWMNormalHints (dis, win, &sizeHints, &ljunk)) {
		if (sizeHints.flags & PMaxSize) {
			wc.width = (sizeHints.max_width > maxWidth) ? maxWidth : sizeHints.max_width;
			wc.height = (sizeHints.max_height > maxHeight) ? maxHeight : sizeHints.max_height;
#ifdef DEBUG
			fprintf (stderr, "MaxSize %d %d\n", sizeHints.max_width, sizeHints.max_height);
#endif
		}	
#ifdef DEBUG
		if (sizeHints.flags & PResizeInc) {
			fprintf (stderr, "PResizeInc\n");
			fprintf (stderr, "incr %d %d\n", sizeHints.width_inc, sizeHints.height_inc);
		}		
		if (sizeHints.flags & PAspect) {
			fprintf (stderr, "PAspect x %d\n", sizeHints.min_aspect.x);
		}		
#endif
	}

	XConfigureWindow (dis, win, value_mask, &wc);
	sendConfigureNotify (win, value_mask, &wc);	
}


/*This appends windows that are not to be managed by 
 *Panache to a list, and Panache will later on raise 
 *them above other windows. 
 */
void PanacheCreateNotify (XCreateWindowEvent *event) {
							
	if (event->override_redirect == 0 || event->parent != root) {
		return;
	}
	
	CListAppend (keepAboveWindowList, event->window);
}

/*X has told Panache that a DestroyNotify event occured
 *to a child of the root window, so Panache removes the
 *window from the window list.
 */
void PanacheDestroyNotify (XDestroyWindowEvent *event) {
	Window win;
	char *winId;
	char str[] = "sendToPipe [list remove $winId]";

	win = event->window;

	winId = charMalloc (winIdLength);
	sprintf (winId, "%ld", win);
	
	Tcl_SetVar (interp, "winId", winId, 0);
	free (winId);

#ifdef DEBUG
	fprintf (stderr, "DestroyNotify\n");
#endif

	CListRemove (keepAboveWindowList, event->window);		

	/*Tell Panache_GUI to remove the window*/
	if (Tcl_Eval (interp, str) != TCL_OK) {
		fprintf (stderr, "Tcl_Eval error in PanacheDestroyNotify %s\n", Tcl_GetStringResult (interp));
	}	
}


/*Panache_GUI calls this to send WM_DELETE_WINDOW or 
 *invoke XKillClient (if the window doesn't support
 *WM_DELETE_WINDOW).  We can't use XKillClient on all
 *windows, because if the application has multiple 
 *toplevel windows sending XKillClient would destroy
 *them all.
 */
int PanacheDestroyCmd CMD_ARGS {
	XClientMessageEvent ev;
	Window win;
	Atom *wmProtocols = NULL;
	Atom *protocol;
	int i;
	int numAtoms;
	int handlesWM_DELETE_WINDOW = 0;
		
	
	Tcl_GetLongFromObj (interp, objv[1], (long *) &win);

	if (XGetWMProtocols (dis, win, &wmProtocols, &numAtoms) == 1) {
		for (i = 0, protocol = wmProtocols; i < numAtoms; i++, protocol++) {
			if (*protocol == (Atom)_XA_WM_DELETE_WINDOW) {
				handlesWM_DELETE_WINDOW = 1;
			}
		} 
		if (wmProtocols) {
			XFree (wmProtocols);
		}
	}
	
	if (handlesWM_DELETE_WINDOW == 1) {
		ev.type = ClientMessage;
		ev.window = win;
		ev.message_type = _XA_WM_PROTOCOLS;
		ev.format = 32;
		ev.data.l[0] = _XA_WM_DELETE_WINDOW; 
		ev.data.l[1] = CurrentTime;
		XSendEvent (dis, win, 0, 0L, (XEvent *) &ev); 
	} else {
		XKillClient (dis, win); 
	}
	
	XFlush (dis);
		
	return TCL_OK;
}


int PanacheDFECmd CMD_ARGS {
	Tcl_GetIntFromObj (interp, objv[1], &distance_from_edge);
	return TCL_OK;
}

		
/*Panache_GUI sends focus $winId to get here.*/
int PanacheFocusCmd CMD_ARGS {
	Window win;
	
	Tcl_GetLongFromObj (interp, objv[1], (long *) &win);

	if (XSetInputFocus (dis, win, RevertToParent, CurrentTime) != 1) {
		fprintf (stderr, "XSetInputFocus failure within PanacheFocusCmd()");
	}

	XFlush (dis);
	
	return TCL_OK;
}


int PanacheGetWMState (Window win) {
	int returnValue = 0;
	Atom type;
	int ijunk;
	unsigned long ljunk;
	unsigned long *state = NULL;

	XGetWindowProperty (
		dis, 
		win, 
		_XA_WM_STATE, 
		0L, 
		1L, 
		0, 
		_XA_WM_STATE,
		&type, 
		&ijunk, 
		&ljunk, 
		&ljunk, 
		(unsigned char **) &state
	);
		
	if (type == _XA_WM_STATE) {
 		returnValue = (int) *state;
	} else {
		/*Don't know what to do*/	
		returnValue = 0;
	}
	
	if (state != NULL) {
		XFree (state);
	}

	return returnValue;
}
	
/*A window to keep above has the override_redirect 
 *attribute set to 1.
 */
 	
void PanacheRaiseKeepAboveWindows () {
	Window win;

	CListRewind (keepAboveWindowList);
	
	while ((win = CListGet (keepAboveWindowList)) != NULL) {
		XRaiseWindow (dis, win);
	}

	XFlush (dis);
}
			

void PanacheRecursivelyGrabKey (Window win, int keycode) {
	Window dummy;
	Window *children = NULL; 
	unsigned int nchildren;
	int i;
	
	
	if (XQueryTree (dis, win, &dummy, &dummy, &children, &nchildren) == 0) {
		return;
	}	
	
	for (i = 0; i < nchildren; i++) {
		PanacheRecursivelyGrabKey (children[i], keycode); 
		XGrabKey (dis, keycode, Mod1Mask, win, 1, GrabModeAsync, GrabModeSync);
	} 
	
	if (children != NULL) { 
		XFree (children);
	}
}	


int PanacheReparentCmd CMD_ARGS {
	Window newParent;
	Window win;

	Tcl_GetLongFromObj (interp, objv[1], (long *) &win);
	Tcl_GetLongFromObj (interp, objv[2], (long *) &newParent);
		
	XReparentWindow (dis, win, newParent, 0, 20); 	
	
	return TCL_OK;
}

		
void PanacheSelectInputForRootParented (Window win) {
		
	XSelectInput (dis, win, eventMask);
}


void PanacheSetWMState (Window win, int state) {
	unsigned long data[2];
	data[0] = state;
	data[1] = None;

	XChangeProperty (dis, win, _XA_WM_STATE, _XA_WM_STATE, 32,
		PropModeReplace, (unsigned char *) data, 2
	);
		
	XSync (dis, 0);
}

	
int PanacheTransientCmd CMD_ARGS {
	Window parent;
	Window win;

	Tcl_GetLongFromObj (interp, objv[1], (long *) &win);
	Tcl_GetLongFromObj (interp, objv[2], (long *) &parent);
	
	XSetTransientForHint (dis, win, parent);
		
	return TCL_OK;
}
	
/*This sends a string to Panache_GUI with info about the window, 
 *such as its title and window id.  This information is processed
 *within Panache_GUI and if desired PanacheMapCmd will map the 
 *window.
 */
void PanacheMapRequest (XMapRequestEvent *event) {
	char *winId;
	char *transientForWinId;
	XTextProperty xtp;
	char str[] = "sendToPipe [list add [list $winTitle] $winId $winType $transientForWinId]";
	Window twin;
	
	if (event->window == NULL) { 
		return;			 
	}

	/*This makes the state iconic, so that if the user presses
	 *restart before mapping the window, the window will show up.
	 */
	PanacheSetWMState (event->window, IconicState);
	
	xtp.value = NULL;
	
	XGetWMName (dis, event->window, &xtp);
		
	winId = charMalloc (winIdLength);
		
	sprintf (winId, "%ld", event->window);
	PanacheSelectInputForRootParented (event->window);	
			
	Tcl_SetVar (interp, "winTitle", (char *) xtp.value, 0);
	Tcl_SetVar (interp, "winId", winId, 0);
	
	if (XGetTransientForHint (dis, event->window, &twin) == 1) {
		Tcl_SetVar (interp, "winType", "transient", 0);
		transientForWinId = charMalloc (winIdLength);
		sprintf (transientForWinId, "%ld", twin);
		Tcl_SetVar (interp, "transientForWinId", transientForWinId, 0);	
		free (transientForWinId);
	} else {
		Tcl_SetVar (interp, "winType", "normal", 0);
		Tcl_SetVar (interp, "transientForWinId", "", 0);	
	}
			
	XFree (xtp.value);
	free (winId);
		
	if (Tcl_Eval (interp, str) != TCL_OK) {
		fprintf (stderr, "Error in PanacheMapRequest: %s\n", Tcl_GetStringResult (interp));
	}
}


/*This maps a window.  It may be called after PanacheMapRequest by
 *Panache_GUI.  This is also called when a window is over another
 *window and the user selects the button for the window to display
 *which causes this function to raise the window.
 */
int PanacheMapCmd CMD_ARGS {
	Window win;
	Window twin;
	XWindowAttributes winAttrib; 
	
	Tcl_GetLongFromObj (interp, objv[1], (long *) &win);
	
	PanacheSelectInputForRootParented (win);

	/*XGrabKey (dis, XK_Tab, Mod1Mask, win, 1, GrabModeAsync, GrabModeAsync);*/ 
	/*PanacheRecursivelyGrabKey (win, XK_Tab);*/

	XGetWindowAttributes (dis, win, &winAttrib);	
	
	if (winAttrib.x < distance_from_edge) {
		winAttrib.x = distance_from_edge;
		if (winAttrib.y < 0) {
			winAttrib.y = 0;
		}
		XMoveWindow (dis, win, winAttrib.x, winAttrib.y);			
	}  
		
	if (XGetTransientForHint (dis, win, &twin) == 1) {
		PanacheSetWMState (win, NormalState);
		XMapRaised (dis, win);
		sendMapNotify (win);
		mapped_window = win;
		PanacheRaiseKeepAboveWindows ();
		
		return TCL_OK;
	}

	
	if ((PanacheGetWMState (win)) == 1) {
		XRaiseWindow (dis, win);
		PanacheRaiseKeepAboveWindows ();
			
		return TCL_OK;
	}	
	
	/*If we are here the window hasn't had its size set, or 
	 *the WM_STATE was not 1.
	 */
			
	PanacheSetWMState (win, NormalState);

	/*I've found that some applications get upset if you sent
	 *a ConfigureNotify before the MapNotify, when they are
	 *expecting the MapNotify to be eminent.
	 */
	
	XMapRaised (dis, win);
	sendMapNotify (win);

	PanacheConfigureNormalWindow (win, CWX|CWY|CWWidth|CWHeight);
		
	mapped_window = win;
	PanacheRaiseKeepAboveWindows ();

	return TCL_OK;
}


int PanacheMapWorkspaceCmd CMD_ARGS {
	XWindowChanges wc;
	Window win;
	
	Tcl_GetLongFromObj (interp, objv[1], (long *) &win);
	workspace_manager = win;
	PanacheSetWMState (win, NormalState);

	wc.x = 0;
	wc.y = 0;
	wc.width = distance_from_edge;
	wc.height = DisplayHeight (dis, screen);
	
	XConfigureWindow(dis, win, CWX|CWY|CWWidth|CWHeight, &wc);
	sendConfigureNotify (win, CWX|CWY|CWWidth|CWHeight, &wc);
	
	XMapWindow (dis, win);
	sendMapNotify (win);
	mapped_window = win;
	XFlush (dis);
	
	return TCL_OK;
}


int PanacheMoveCmd CMD_ARGS {
	XEvent event;
	unsigned int buttonPressed;
	Window wjunk;
	int ijunk;
	Cursor handCursor;		
	Window win;	 
	int oldX;
	int oldY;
	int x;
	int y;
	int internalX;
	int internalY;
	unsigned int maskReturn;	
	int continueEventLoop = 1;
	XWindowAttributes winAttr;
				
	handCursor = XCreateFontCursor (dis, XC_hand2);

	XGrabPointer (dis, root, 1, 
		ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | \
			PointerMotionHintMask,
		GrabModeAsync, GrabModeAsync,
		None, 
		handCursor,
		CurrentTime
	);
		
	/*Wait until the user has selected the window to move.*/
	XMaskEvent (dis, ButtonPressMask, &event);

	/*The button being held down while dragging the window.*/
	buttonPressed = event.xbutton.button;

	/*fprintf (stderr, "ButtonPressed %d\n", buttonPressed);*/
			
	XQueryPointer (dis, root, 
		&wjunk, &win, 
		&oldX, &oldY, 
		&internalX, &internalY,
		&maskReturn
	);
		
	if (win == workspace_manager) {
		XUngrabPointer (dis, CurrentTime);	
		XFreeCursor (dis, handCursor);
		XSync (dis, 0);			
	
		return TCL_OK;
	} 
					
				
	XGetWindowAttributes (dis, win, &winAttr);
				
	while (continueEventLoop == 1) {
		XNextEvent (dis, &event);
		switch (event.type) {
			case ButtonRelease:
			{
				if (event.xbutton.button == buttonPressed) {
					continueEventLoop = 0;
				}
			}
			break;
			case MotionNotify:
			{
				XWindowChanges wc;
				int newX;
				int newY;
												
				while (XCheckTypedEvent (dis, MotionNotify, &event));
			
				XQueryPointer (dis, root, &wjunk, &wjunk, 
					&x, &y,
					&ijunk, &ijunk,
					&maskReturn
				);
						
				newX = x - oldX + winAttr.x;
				newY = y - oldY + winAttr.y;
				
				if (newX < distance_from_edge) {
				
					if (winAttr.override_redirect == 1) {		
						XMoveWindow (dis, win, distance_from_edge, newY);
					} else {
						wc.x = distance_from_edge;
						wc.y = newY;				
						XConfigureWindow (dis, win, CWX | CWY, &wc);			
						sendConfigureNotify (win, CWX | CWY, &wc);
					}
					continue;
				} 					
				
				if (winAttr.override_redirect == 1) {		
					XMoveWindow (dis, win, newX, newY);
				} else {
					wc.x = newX;
					wc.y = newY;				
					XConfigureWindow (dis, win, CWX | CWY, &wc);			
					sendConfigureNotify (win, CWX | CWY, &wc);
				}
			} 
			break;		
		}	
	}
		
	/*fprintf (stderr, "win is %ld\n", win);*/

	XUngrabPointer (dis, CurrentTime);	
	XFreeCursor (dis, handCursor);
	
	XSync (dis, 0);
	
	return TCL_OK;
}
	

XErrorHandler PanacheErrorHandler (Display *dis, XErrorEvent *event) {
/*I've discovered that errors are frequently timing problems.
Maybe XSync would help in some areas.
Most errors are not fatal.
*/
	return 0;
}


int main() {
	fd_set readfds;
	int nfds;
	int xFd;
	int pipeFd;
	int inputPipeFd;
	ClientData data;
	int fdsTcl;
	

	dis = XOpenDisplay (NULL);
	screen = DefaultScreen (dis);
	root = RootWindow (dis, screen);
	interp = Tcl_CreateInterp ();

	XSetErrorHandler ((XErrorHandler) PanacheErrorHandler);
			
	_XA_WM_STATE = XInternAtom (dis, "WM_STATE", 0);	
	_XA_WM_PROTOCOLS = XInternAtom (dis, "WM_PROTOCOLS", 0);
	_XA_WM_DELETE_WINDOW = XInternAtom (dis, "WM_DELETE_WINDOW", 0); 

	keepAboveWindowList = CListInit ();

#ifdef FORK_ON_START
	{
	int res;
		res = fork();
	
		if (res == -1) {
			fprintf (stderr, "Unable to fork process.");
			return 1;
		}

		if (res != 0) {
			exit (0);
		}
	}
#endif
		
	if (Tcl_Init (interp) != TCL_OK) {
		printf ("Tcl_Init error\n");
		exit (-1);
	}

#define CREATE_CMD(cmdName,func) Tcl_CreateObjCommand (interp, \
cmdName, func, (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL)
	
	CREATE_CMD ("map_workspace", PanacheMapWorkspaceCmd);
	CREATE_CMD ("distance_from_edge", PanacheDFECmd);
	CREATE_CMD ("map", PanacheMapCmd);
	CREATE_CMD ("destroy", PanacheDestroyCmd);
	CREATE_CMD ("add_all_windows", PanacheAddAllWindowsCmd);
	CREATE_CMD ("focus", PanacheFocusCmd);
	CREATE_CMD ("transient", PanacheTransientCmd);
	CREATE_CMD ("reparent", PanacheReparentCmd);
	CREATE_CMD ("move", PanacheMoveCmd);

	Tcl_SetVar (interp, "wishInterpreter", WISH_INTERPRETER, 0);
	Tcl_SetVar (interp, "prefix", PREFIX, 0);
	Tcl_SetVar (interp, "panacheDirectory", PANACHE_DIRECTORY, 0);
			
		
	if (Tcl_Eval (interp, Panache_Init_script) != TCL_OK) {
		fprintf (stderr, "Error while evaluating Panache_Init_script within main()%s\n", Tcl_GetStringResult (interp));
		exit (-1);
	}	

	XSelectInput (dis, root, LeaveWindowMask | EnterWindowMask| \
		PropertyChangeMask | SubstructureRedirectMask | \
		SubstructureNotifyMask | KeyPressMask | KeyReleaseMask | \
		ResizeRedirectMask | FocusChangeMask
	);

	xFd = ConnectionNumber (dis);
	
	Tcl_GetChannelHandle (Tcl_GetChannel (interp, Tcl_GetVar (interp, "pipe", NULL), NULL), TCL_WRITABLE, &data);
	pipeFd = (int) data;
	/*fprintf (stderr, "pipeFd %d", pipeFd);*/ 
	
	Tcl_GetChannelHandle (Tcl_GetChannel (interp, Tcl_GetVar (interp, "pipe", NULL), NULL), TCL_READABLE, &data);
	inputPipeFd = (int) data;
	
	XFlush(dis);
		
	for (;;) {
			
		FD_ZERO (&readfds);
		FD_SET (xFd, &readfds);
		FD_SET (pipeFd, &readfds);
		FD_SET (inputPipeFd, &readfds);
						
		fdsTcl = (pipeFd > inputPipeFd) ? pipeFd : inputPipeFd;
		nfds = (xFd > fdsTcl) ? xFd + 1: fdsTcl + 1;	
	
		select (nfds, &readfds, NULL, NULL, NULL);

		if (FD_ISSET (inputPipeFd, &readfds) != 0) { 
			if (Tcl_Eval (interp, "getFromPipe") != TCL_OK) {
				fprintf (stderr, "getFromPipe error %s\n", Tcl_GetStringResult (interp));
			} 
		} 
			
		if (FD_ISSET (pipeFd, &readfds) != 0) {
			while (Tcl_DoOneEvent (TCL_DONT_WAIT));	
		}
						
		if (FD_ISSET (xFd, &readfds) == 0) {
			continue;
		}

		while (XPending (dis) > 0) {
			XNextEvent (dis, &report);	
			
			/*fprintf (stderr, "type %d\n", report.type);*/
			switch  (report.type) {
			case ConfigureNotify: 
			/*fprintf (stderr, "ConfigureNotify \n");*/
			break;	

			case CreateNotify:
				PanacheCreateNotify (&report.xcreatewindow);
			break;

			case ConfigureRequest:
				PanacheConfigureRequest (&report.xconfigurerequest);
			break;

			case DestroyNotify:
				PanacheDestroyNotify (&report.xdestroywindow);
			break;

			case EnterNotify:
			{
				Window win = report.xcrossing.window;
				char *winId = NULL; 
				char cmd[] = "sendToPipe [list activateWindow $winId]";
	
				winId = charMalloc (winIdLength);
				sprintf (winId, "%ld", win); 
				Tcl_SetVar (interp, "winId", winId, 0);
				free (winId);
				
				if (Tcl_Eval (interp, cmd) != TCL_OK) {
					fprintf (stderr, "Error evaluating cmd in EnterNotify within main() %s\n", Tcl_GetStringResult (interp));
				}
	
			}
			break;

			case FocusIn:
			break;

	
			case KeyPress:
			{
				char cmd[] = "sendToPipe next";
								
				if (XLookupKeysym (&report.xkey, 0) == XK_Tab && (report.xkey.state & Mod1Mask)) {	
					fprintf (stderr, "alt tab win %ld\n", report.xkey.window);
					if (Tcl_Eval (interp, cmd) != TCL_OK) { 
						fprintf (stderr, "Error evaluating cmd in KeyPress within main() %s\n", Tcl_GetStringResult (interp));
					}
				} else {
					/*Send XK_Tab*/
				}
													
				/*
				fprintf (stderr, "1 %d \n", report.xkey.state == Mod1Mask);
				fprintf (stderr, "2 %d \n", report.xkey.state == Mod2Mask);
				fprintf (stderr, "3 %d \n", report.xkey.state == Mod3Mask);
				fprintf (stderr, "4 %d \n", report.xkey.state == Mod4Mask);
				fprintf (stderr, "5 %d \n", report.xkey.state == Mod5Mask);
				*/
			}
			break;

			case MapRequest: 
				PanacheMapRequest (&report.xmaprequest);
			break;

			case UnmapNotify:
			{
				int state = PanacheGetWMState (report.xunmap.window);
				/*Mapped or Iconified*/
				if (state == 1 || state == 3) {
					char *winId = NULL;
					char cmd[] = "sendToPipe [list remove $winId]";
		
					winId = charMalloc (winIdLength);
					sprintf (winId, "%ld", report.xunmap.window);				
					
					Tcl_SetVar (interp, "winId", winId, 0);
					free (winId);
							
					PanacheSetWMState (report.xunmap.window, WithdrawnState);
				
					if (Tcl_Eval (interp, cmd) != TCL_OK) {
						fprintf (stderr, "Tcl_Eval error in UnmapNotify within main() %s", Tcl_GetStringResult (interp));
					}
				}
			}
			break;

			case PropertyNotify:
			{
				XTextProperty xtp;
				xtp.value = NULL;

				if (XGetWMName (dis, report.xproperty.window, &xtp) == 1) {
					char *winId;
					char cmd[] = "sendToPipe [list title [list $winTitle] $winId]";
						
					winId = charMalloc (winIdLength);
					sprintf (winId, "%ld", report.xproperty.window);
		
					Tcl_SetVar (interp, "winTitle", (char *) xtp.value, 0);
					Tcl_SetVar (interp, "winId", winId, 0);
					
					free (winId);
					XFree (xtp.value);
				
					if (Tcl_Eval (interp, cmd) != TCL_OK) {
						fprintf (stderr, "Tcl_Eval error in PropertyNotify: within main() %s\n", Tcl_GetStringResult (interp));
					}
				}
			}
			break;		


			case ReparentNotify:
			{
				Window win = report.xreparent.window;
				Window parent = report.xreparent.parent;
								
				/*
				fprintf (stderr, "ReparentNotify\n");
				fprintf (stderr, "win %ld parent %ld event %ld\n", win, parent, event);
				*/			
				XSelectInput (dis, win, 0); 

				if (parent != root) { 
					char *winId;				
					char cmd[] = "sendToPipe [list remove $winId]"; 
		
					winId = charMalloc (winIdLength);
					sprintf (winId, "%ld", win); 
					Tcl_SetVar (interp, "winId", winId, 0);		
					free (winId);
						
					if (Tcl_Eval (interp, cmd) != TCL_OK) { 
						fprintf (stderr, "Tcl_Eval error in ReparentNotify within main() %s\n", Tcl_GetStringResult (interp));
					} 
				}				
			}			
			break;


			case ResizeRequest:
			{
				Window twin;
				Window win = report.xresizerequest.window;				
								
				if (XGetTransientForHint (dis, win, &twin) == 1) {
					XResizeWindow (dis, win, 
						report.xresizerequest.width, report.xresizerequest.height
					);
				} 
				
				XFlush (dis);
			}
			break;
		
			default:
			break;
			}
		}
	}
	return 0;
}
