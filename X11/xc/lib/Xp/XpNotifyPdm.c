/* $Xorg: XpNotifyPdm.c,v 1.4 2000/08/17 19:46:07 cpqbld Exp $ */
/******************************************************************************
 ******************************************************************************
 **
 ** Description:  XpNotifyPdm is used to engage a Page Dialog Manager
 **               (PDM).  Though the PDM is not a formal part of the
 **               Print Extension, the concept and likely usage of
 **               PDM's is strong enough that this routine is being
 **               provided as a sample standard mechanism for engaging
 **               PDM's from the Print Extension.
 **
 ** (c) Copyright 1996 Hewlett-Packard Company
 ** (c) Copyright 1996 International Business Machines Corp.
 ** (c) Copyright 1996 Sun Microsystems, Inc.
 ** (c) Copyright 1996 Novell, Inc.
 ** (c) Copyright 1996 Digital Equipment Corp.
 ** (c) Copyright 1996 Fujitsu Limited
 ** (c) Copyright 1996 Hitachi, Ltd.
 ** 
 ** Permission is hereby granted, free of charge, to any person obtaining a copy
 ** of this software and associated documentation files (the "Software"), to deal
 ** in the Software without restriction, including without limitation the rights
 ** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 ** copies of the Software, and to permit persons to whom the Software is
 ** furnished to do so, subject to the following conditions:
 **
 ** The above copyright notice and this permission notice shall be included in
 ** all copies or substantial portions of the Software.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 ** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 ** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **
 ** Except as contained in this notice, the names of the copyright holders shall
 ** not be used in advertising or otherwise to promote the sale, use or other
 ** dealings in this Software without prior written authorization from said
 ** copyright holders.
 **
 ******************************************************************************
 *****************************************************************************/
/* $XFree86: xc/lib/Xp/XpNotifyPdm.c,v 1.6 2002/10/16 00:37:32 dawes Exp $ */

#include <X11/extensions/Print.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xauth.h>
#include <stdlib.h>
#include <X11/Xos.h>

/*
 * Alternate selection environment variables.
 */
#define ALT_PDM_SELECTION_ENV_VAR  "XPDMSELECTION"
#define ALT_PMD_DISPLAY_ENV_VAR    "XPDMDISPLAY"

/*
 * X-Authority file for authorization tickets.
 */
#define PDM_XAUTHORITY_ENV_VAR     "XPDMXAUTHORITY"

/*
 * str_dup using Xmalloc
 */
char *_xpstrdup(char * str)
{
    int len;
    char *newstr;

    if (!str)
	return( (char *) NULL );

    len = strlen(str) + 1;
    newstr = (char *) Xmalloc( len );
    memcpy( newstr, str, len );
    return( newstr );
}

/******************************************************************************
 *
 * _XpGetSelectionServer
 *
 * Routine that considers print_display, video_display, and alt
 * environment variables, and figures out the selection_display
 * and selection_atom.
 *
 * selection_display can be one of print_display, video_display,
 * or a brand new display connection that the caller will later
 * have to close.
 */
static Display *
_XpGetSelectionServer (
    Display         *print_display,
    Display         *video_display,
    Atom            *sel_atom			/* return value */
)
{
    char          *tstr1, *tstr2, *tstr3, *tstrptr;
    char          *sel_displaystr;
    Display       *sel_display;
    char          *selectionstr;


    /*
     * Possibly tickle a selection on an alternate display.
     */
    if ( (sel_displaystr = getenv( ALT_PMD_DISPLAY_ENV_VAR )) != NULL ) {
	if ( !strcmp(sel_displaystr, "print") ) {
	    /*
	     * Explicitly use the Print display.
	     */
	    sel_display = print_display;
	}
	else if ( !strcmp(sel_displaystr, "video") ) {
	    /*
	     * Explicitly use the Video display.
	     */
	    sel_display = video_display;
	}
	else {
	    /*
	     * Do more work to determine the selection server connection.
	     * The following is not clever enough to handle host aliases.
	     */
	    tstr1 = _xpstrdup( sel_displaystr );
	    tstr2 = _xpstrdup( XDisplayString(print_display) );
	    tstr3 = _xpstrdup( XDisplayString(video_display) );

	    /*
	     * remove ".scr" portion from "host:disp.scr" strings.
	     */
	    if (( tstrptr = strchr( tstr1, ':' ) ))
		if (( tstrptr = strchr( tstrptr, '.' ) ))
		    *tstrptr = '\0';

	    if (( tstrptr = strchr( tstr2, ':' ) ))
		if (( tstrptr = strchr( tstrptr, '.' ) ))
		    *tstrptr = '\0';

	    if (( tstrptr = strchr( tstr3, ':' ) ))
		if (( tstrptr = strchr( tstrptr, '.' ) ))
		    *tstrptr = '\0';

	    if ( !strcmp( tstr1, tstr2 ) ) {
		/*
		 * Explicitly use the Print display.
		 */
		sel_display = print_display;
	    }
	    else if ( !strcmp( tstr1, tstr3 ) ) {
		/*
		 * Explicitly use the Print display.
		 */
		sel_display = video_display;
	    }
	    else {
		/*
		 * Create a new display connection to a server
		 * never previously contacted. The caller will
		 * have to realize this is a new display handle
		 * (ie, its not equal to print_display or
		 * video_display) when done.
		 */
		sel_display = XOpenDisplay(sel_displaystr);
	    }

	    XFree( tstr3 );
	    XFree( tstr2 );
	    XFree( tstr1 );
	}
    }
    else {
	/*
	 * If no alt selection server, use the print server.
	 */
	sel_display = print_display;
    }

    if (sel_display) {
	/*
	 * Tickle (possibly alternate) PDM_MANAGER selection
	 */
	if ( (selectionstr = getenv( ALT_PDM_SELECTION_ENV_VAR )) == NULL )
	    selectionstr = "PDM_MANAGER";

	    *sel_atom = XInternAtom( sel_display, selectionstr, False );
    }

    return( sel_display );
}



/******************************************************************************
 *
 * XpGetPdmStartParams
 */
Status
XpGetPdmStartParams (
    Display         *print_display,
    Window          print_window,
    XPContext       print_context,
    Display         *video_display,
    Window          video_window,
    Display         **selection_display,	/* return value */
    Atom            *selection,			/* return value */
    Atom            *type,			/* return value */
    int             *format,			/* return value */
    unsigned char   **data,			/* return value */
    int             *nelements			/* return value */
)
{
    XTextProperty text_prop;
    int           status;
    char          str1[128], str2[128], str3[128];
    char          *list[6];


    *selection_display = _XpGetSelectionServer( print_display, video_display,
						selection );

    if (*selection_display == NULL) {
	/*
	 * Error - cannot determine or establish a selection_display.
	 */
	return( (Status) NULL );
    }

    /*
     * Create a property that can be included in the PDM_MANAGER
     * selection to communicate information.
     *
     *    video_display    host:display[.anyScreen]
     *    video_window
     *    print_display    host:display[.anyScreen]
     *    print_window
     *    print_context    use to derive host:display.properScreen and other
     *    locale_hint
     */
    list[0] = XDisplayString( video_display );
    sprintf( str1, "0x%lx", (long)video_window );
    list[1] = str1;

    list[2] = XDisplayString( print_display );
    sprintf( str2, "0x%lx", (long)print_window );
    list[3] = str2;

    sprintf( str3, "0x%lx", (long)print_context );
    list[4] = str3;

    list[5] = XpGetLocaleNetString();

    status = XSupportsLocale();
    if ( ! status ) {
	/*
	 * Error.
	 */
	if ( (*selection_display != print_display) &&
	     (*selection_display != video_display)    ) {
	    XCloseDisplay( *selection_display );
	    *selection_display = (Display *) NULL;
	}
	return( (Status) NULL );
    }

    status = XmbTextListToTextProperty( *selection_display, list, 6,
					XStdICCTextStyle, &text_prop );

    if ( status < 0 ) {
	/*
	 * Error.
	 */
	if ( (*selection_display != print_display) &&
	     (*selection_display != video_display)    ) {
	    XCloseDisplay( *selection_display );
	    *selection_display = (Display *) NULL;
	}
	return( (Status) NULL );
    }

    *type              = text_prop.encoding;
    *format            = text_prop.format;
    *data              = text_prop.value;
    *nelements         = text_prop.nitems;

    XFree(list[5]);

    return( (Status) 1 );
}

/******************************************************************************
 *
 * XpSendOneTicket
 *
 * The ticket transfer protocol is as follows:
 *    unsigned short ticket_state;
 *       - 0 means NULL ticket, terminating ticket
 *       - 1 means non-NULL ticket, terminating ticket
 *       - 2 means non-NULL ticket, and more tickets to send
 *    unsigned short address_length;
 *    unsigned short number_length;
 *    unsigned short name_length;
 *    unsigned short data_length;
 *    unsigned short family;
 *
 *    char *address;
 *    char *number;
 *    char *name;
 *    char *data;
 *
 */

Status XpSendOneTicket(
    Display *display,
    Window  window,
    Xauth   *ticket,
    Bool    more )
{
    XClientMessageEvent ev;
    int                 bigstrlen, left, gofor;
    char                *bigstr, *tptr;
    Status              status;

    /*
     * Fixed portion of event.
     */
    ev.type = ClientMessage;
    ev.display = display;
    ev.window  = window;
    ev.message_type = XInternAtom( display, "PDM_MAIL", False );

    /*
     * Special build the first ClientMessage to carry the
     * ticket transfer header.
     */
    ev.format = 16;

    if (!ticket) {
	ev.data.s[0] = 0;
    }
    else {
	if (more)
	    ev.data.s[0] = 2;
	else
	    ev.data.s[0] = 1;
	ev.data.s[1] = (short) ticket->address_length;
	ev.data.s[2] = (short) ticket->number_length;
	ev.data.s[3] = (short) ticket->name_length;
	ev.data.s[4] = (short) ticket->data_length;
	ev.data.s[5] = (short) ticket->family;
    }

    status = XSendEvent( display, window, False, 0L, (XEvent *) &ev );
    if (!status)
	return( (Status) 0 );

    if (!ticket)
	return( (Status) 1 );;

    /*
     * Break down the remaining ticket data and build the
     * second thru N ClientMessages.
     */
    ev.format = 8;
    bigstrlen = (int) ticket->address_length +
		(int) ticket->number_length +
		(int) ticket->name_length +
		(int) ticket->data_length;

    bigstr = Xmalloc( bigstrlen );

    tptr = bigstr;
    memcpy( tptr, ticket->address, ticket->address_length );
    tptr += ticket->address_length;
    memcpy( tptr, ticket->number, ticket->number_length );
    tptr += ticket->number_length;
    memcpy( tptr, ticket->name, ticket->name_length );
    tptr += ticket->name_length;
    memcpy( tptr, ticket->data, ticket->data_length );

    left = bigstrlen;
    tptr = bigstr;

    while ( left ) {
	if (left > 20)
	    gofor = 20;
	else
	    gofor = left;

	memcpy( ev.data.b, tptr, gofor );

	tptr += gofor;
	left -= gofor;

	status = XSendEvent( display, window, False, 0L, (XEvent *) &ev );
	if (!status) {
	    Xfree( bigstr );
	    return( (Status) 0 );
	}
    }

    Xfree( bigstr );

    return( (Status) 1 );
}

Status XpSendAuth( Display *display, Window window )
{

    FILE    *auth_file;
    char    *auth_name;
    Xauth   *entry;

    if ( !(auth_name = getenv( PDM_XAUTHORITY_ENV_VAR )) ) {
        return( (Status) 0 );
	/* auth_name = XauFileName (); */
    }

    if (access (auth_name, R_OK) != 0)          /* checks REAL id */
        return( (Status) 0 );

    auth_file = fopen (auth_name, "r");
    if (!auth_file)
        return( (Status) 0 );

    for (;;) {
        entry = XauReadAuth (auth_file);
        if (!entry)
            break;

	/*
	 * NOTE: in and around this area, an optimization can
	 * be made.  Rather than sending all the tickets in
	 * .Xauthority, just pull out the *one* that is needed
	 * by the PDM.
	 */
	XpSendOneTicket( display, window, entry, True );

	XauDisposeAuth (entry);
    }

    XpSendOneTicket( display, window, (Xauth *) NULL, False );

    (void) fclose (auth_file);

    return( (Status) 1 );
}

/******************************************************************************
 *
 *
 */
Status
XpGetAuthParams (
    Display         *print_display,
    Display         *video_display,
    Display         **sel_display,		/* return value */
    Atom            *sel_atom,			/* return value */
    Atom            *sel_target_atom		/* return value */
)
{
    *sel_display = _XpGetSelectionServer( print_display, video_display,
					  sel_atom );

    if (*sel_display == NULL) {
	/*
	 * Error - cannot determine or establish a selection_display.
	 */
	return (Status)0;
    }

    /*
     * Create property and transfer data to.
     */
    *sel_target_atom = XInternAtom( *sel_display, "PDM_MBOX", False );
    return (Status)1;
}


/******************************************************************************
 ******************************************************************************
 **
 ** UNSUPPORTED ROUTINES used for testing and debugging.
 **
 **/


/******************************************************************************
 *
 * Predicate routines to dig out events from the users event queue.
 */

/*
 * client_data structures to use with XIfEvent()
 */
typedef struct {
    Window	requestor;
    Atom	selection;
    Atom	target;
} Dosnrec;

typedef struct {
    Window	window;
    Atom	atom;
} Dopnrec, Docmrec;


/*
 * Dig out a selection notify from the users event
 * queue.
 */
static Bool digOutSelectionNotify(
    Display *display,
    XEvent *event,
    char *arg
)

{
    Dosnrec *t;

    if (event->type == SelectionNotify) {
	t = (Dosnrec *) arg;

	/*
	 * Selection complete because of good reply.
	 */
	if ( ( t->requestor == event->xselection.requestor ) &&
	     ( t->selection == event->xselection.selection ) &&
	     ( t->target    == event->xselection.target    )    ) {
	    return( True );
	}

	/*
	 * Selection complete because of problem, and X-Server
	 * had to generate the event.
	 */
	if ( ( t->requestor == event->xselection.requestor ) &&
	     ( t->selection == event->xselection.selection ) &&
	     ( None         == event->xselection.property  )    ) {
	    return( True );
	}
    }
    return( False );
}

/*
 * Dig out a property notify event for the XpNotifyPdm
 * selection mechanism from the users event queue.
 */
static Bool digOutPropertyNotify(
    Display *display,
    XEvent *event,
    char *arg
)
{
    Dopnrec *t;

    if (event->type == PropertyNotify) {
	t = (Dopnrec *) arg;
	if ( ( t->window == event->xproperty.window ) &&
	     ( t->atom   == event->xproperty.atom   )    ) {
	    return( True );
	}
    }
    return( False );
}

/*
 * Dig out a ClientMessage of type PDM_REPLY.
 */
static Bool digOutClientMessage(
    Display *display,
    XEvent *event,
    char *arg
)
{
    Docmrec *t;

    if (event->type == ClientMessage) {
	t = (Docmrec *) arg;
	if ( ( t->window  == event->xclient.window       ) &&
	     ( t->atom    == event->xclient.message_type )    ) {
	    return( True );
	}
    }
    return( False );
}



/******************************************************************************
 *
 * XpCookieToPdm
 */
static char *
XpCookieToPdm (
    Display         *print_display,
    Display         *video_display,
    Window          sel_window
)
{
    Display *sel_display;
    Atom    prop_atom, sel_atom, pdm_mbox_atom, ttype;
    Dosnrec dosnrec;
    Dopnrec dopnrec;
    XEvent  tevent;
    int           tformat;
    unsigned long nitems, bytes_after;
    unsigned char *return_data;
    char *sel_str;
    char    cdata[2048];
    Window tmpw;


    if ( !getenv(PDM_XAUTHORITY_ENV_VAR) ) {
	/*
	 * short cut for lack of cookie file.
	 */
        return( (char *) NULL );
    }

    if (! XpGetAuthParams( print_display, video_display,
			     &sel_display, &sel_atom, &pdm_mbox_atom ) ) {
	sprintf(cdata,
		 "XpCookieToPdm: XpGetAuthParams failed");
	return( _xpstrdup( cdata ) );
    }

    prop_atom = XInternAtom( sel_display, "PDM_MBOX_PROP", False );

    XConvertSelection( sel_display, sel_atom, pdm_mbox_atom,
		       prop_atom, sel_window, CurrentTime );

    /*
     * Hang out waiting for a SelectionNotify.  Dig out from
     * event queue when it arrives.
     */
    dosnrec.requestor = sel_window;
    dosnrec.selection = sel_atom;
    dosnrec.target    = pdm_mbox_atom;

    XIfEvent( sel_display, &tevent, digOutSelectionNotify, (char *) &dosnrec );

    /*
     * See if selection was successful.
     */
    if ( tevent.xselection.property == None ) {
	/*
	 * The selection failed.
	 */
	sel_str = XGetAtomName( sel_display, sel_atom );
	sprintf(cdata,
		 "XpCookieToPdm: Unable to make selection on %s", sel_str);
	XFree(sel_str);

	XDeleteProperty( sel_display, sel_window, prop_atom );
	if ((sel_display != print_display) && (sel_display != video_display))
	    XCloseDisplay( sel_display );
	return( _xpstrdup( cdata ) );
    }

    /*
     * Read property content for status.
     */
    XGetWindowProperty( sel_display, sel_window,
			prop_atom, 0, 100000, True, AnyPropertyType,
			&ttype, &tformat, &nitems, &bytes_after,
			&return_data );

    /*
     * So that the client won't see how XpCookieToPdm is implemented,
     * run through the event queue, dig out, and destroy all
     * PropertyNotify events related to this call.
     */
    dopnrec.window = sel_window;
    dopnrec.atom   = prop_atom;
    while ( XCheckIfEvent( sel_display, &tevent, digOutPropertyNotify,
		 (char *) &dopnrec ) );

    if ((sel_display != print_display) && (sel_display != video_display))
	XCloseDisplay( sel_display );

    if ((ttype != XA_WINDOW) && (tformat != 32) && (nitems != 1)) {
	/*
	 * Unable to read SelectionNotify property.
	 */ 
	sprintf(cdata,
	     "XpCookieToPdm: Unable to read SelectionNotify property" );
	return( _xpstrdup( cdata ) );
    }

    tmpw = *((Window *) return_data);
    Xfree( return_data );

    /*
     * Now send cookie information.
     */
    XpSendAuth( sel_display, tmpw );

    return( (char *) NULL );
}


/******************************************************************************
 *
 * XpNotifyPdm
 */
char *
XpNotifyPdm (
    Display         *print_display,
    Window          print_window,
    XPContext       print_context,
    Display         *video_display,
    Window          video_window,
    Bool            auth_flag
)
{
    enum { XA_PDM_CLIENT_PROP, XA_PDM_START, XA_PDM_START_OK,
	   XA_PDM_START_VXAUTH, XA_PDM_START_PXAUTH, XA_PDM_START_ERROR, 
	   NUM_ATOMS };
    static char *atom_names[] = { 
      "PDM_CLIENT_PROP", "PDM_START", "PDM_START_OK",
      "PDM_START_VXAUTH", "PDM_START_PXAUTH", "PDM_START_ERROR" };

    char          cdata[2048];
    char          *tptr;
    Dosnrec       dosnrec;
    Dopnrec       dopnrec;
    XEvent        tevent;

    Display       *sel_display;
    int           sel_screen;
    Atom          sel_atom;
    char          *sel_str;

    Window        sel_window;
    Atom          prop_type;
    int           prop_format;
    unsigned char *prop_data;
    int           prop_nelements;

    Atom          ttype;
    int           tformat;
    unsigned long nitems, bytes_after;
    unsigned char *return_data;

    int           tmpi;
    Atom	  atoms[NUM_ATOMS];



    if ( ! XpGetPdmStartParams( print_display, print_window, print_context,
			 video_display, video_window,
			 &sel_display,
			 &sel_atom,
			 &prop_type,
			 &prop_format,
			 &prop_data,
			 &prop_nelements ) ) {

	sprintf(cdata, "XpNotifyPdm: XpGetPdmStartParams failed" );
	return( _xpstrdup( cdata ) );
    }


    sel_screen = DefaultScreen( sel_display );
    sel_window = XCreateSimpleWindow( sel_display,
					DefaultRootWindow( sel_display ),
					0, 0, 1, 1, 1,
					BlackPixel(sel_display, sel_screen),
					WhitePixel(sel_display, sel_screen) );

    /*
     * Possibly send over authorization cookies first.
     */
    if (auth_flag) {
	tptr = XpCookieToPdm ( print_display, video_display, sel_window );
	if (tptr)
	    return( tptr );
    }

    /*
     * Create property and transfer data to.
     */
    XInternAtoms( sel_display, atom_names, NUM_ATOMS, False, atoms );

    XChangeProperty( sel_display,
		     sel_window, atoms[XA_PDM_CLIENT_PROP],
		     prop_type,
		     prop_format,
		     PropModeReplace,
		     prop_data,
		     prop_nelements );

    XFree( prop_data );

    /*
     * Tickle PDM_MANAGER selection with PDM_START target
     */
    XConvertSelection( sel_display, sel_atom, atoms[XA_PDM_START],
		       atoms[XA_PDM_CLIENT_PROP], sel_window, CurrentTime );

    /*
     * Hang out waiting for a SelectionNotify.  Dig out from
     * event queue when it arrives.
     */
    dosnrec.requestor = sel_window;
    dosnrec.selection = sel_atom;
    dosnrec.target    = atoms[XA_PDM_START];

    XIfEvent( sel_display, &tevent, digOutSelectionNotify, (char *) &dosnrec );

    /*
     * See if selection was successful.
     */
    if ( tevent.xselection.property == None ) {
	/*
	 * The selection failed.
	 */
	sel_str = XGetAtomName( sel_display, sel_atom );
	sprintf(cdata,
		 "XpNotifyPdm: Unable to make selection on %s", sel_str);
	XFree(sel_str);

	XDeleteProperty( sel_display, sel_window, atoms[XA_PDM_CLIENT_PROP] );
	XDestroyWindow( sel_display, sel_window );
	if ((sel_display != print_display) && (sel_display != video_display))
	    XCloseDisplay( sel_display );
	return( _xpstrdup( cdata ) );
    }

    /*
     * Read property content for status.
     */
    XGetWindowProperty( sel_display, sel_window,
			atoms[XA_PDM_CLIENT_PROP], 
			0, 100000, True, AnyPropertyType,
			&ttype, &tformat, &nitems, &bytes_after,
			&return_data );

    /*
     * So that the client won't see how XpNotifyPdm is implemented,
     * run through the event queue, dig out, and destroy all
     * PropertyNotify events related to this call.
     */
    dopnrec.window = sel_window;
    dopnrec.atom   = atoms[XA_PDM_CLIENT_PROP];
    while ( XCheckIfEvent( sel_display, &tevent, digOutPropertyNotify,
		 (char *) &dopnrec ) );

    XDestroyWindow( sel_display, sel_window );
    if ((sel_display != print_display) && (sel_display != video_display))
	XCloseDisplay( sel_display );

    if ((ttype != XA_ATOM) && (tformat != 32) && (nitems != 1)) {
	/*
	 * Unable to read SelectionNotify property.
	 */ 
	sprintf(cdata,
	     "XpNotifyPdm: Unable to read SelectionNotify property" );
	return( _xpstrdup( cdata ) );
    }

    tmpi = *((Atom *) return_data);
    Xfree( return_data );

    if ( tmpi == atoms[XA_PDM_START_OK] ) {
	return( (char *) NULL );
    }
    else if ( tmpi == atoms[XA_PDM_START_VXAUTH] ) {
	sprintf(cdata,
	     "XpNotifyPdm: PDM not authorized to connect to video display." );
	return( _xpstrdup( cdata ) );
    }
    else if ( tmpi == atoms[XA_PDM_START_PXAUTH] ) {
	sprintf(cdata,
	     "XpNotifyPdm: PDM not authorized to connect to print display." );
	return( _xpstrdup( cdata ) );
    }
    else if ( tmpi == atoms[XA_PDM_START_ERROR] ) {
	sprintf(cdata,
	     "XpNotifyPdm: PDM encountered an error. See PDM log file." );
	return( _xpstrdup( cdata ) );
    }
    else {
	sprintf(cdata,
	     "XpNotifyPdm: unknown PDM error." );
	return( _xpstrdup( cdata ) );
    }
}

