/* $Xorg: XpLocale.c,v 1.3 2000/08/17 19:46:07 cpqbld Exp $ */
/******************************************************************************
 ******************************************************************************
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
/* $XFree86: xc/lib/Xp/XpLocale.c,v 1.5 2002/10/16 00:37:32 dawes Exp $ */

#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

#include <X11/Xlocale.h>


/*
 * Global hangers for locale hint machinery.
 */
XPHinterProc  _xp_hinter_proc = NULL;
char         *_xp_hinter_desc = NULL;
int           _xp_hinter_init = 1;	/* need to init */


extern char *_xpstrdup();


/******************************************************************************
 *
 * THE DEFAULT LOCALE HINTER
 *
 * Make changes here only.
 */
static char *_XpLocaleHinter()
{
#ifdef hpux
    char lbuf[ LC_BUFSIZ ];

    if ( setlocale_r( LC_CTYPE, (char *) NULL, lbuf, LC_BUFSIZ ) ) {
#else
    char *lbuf;

    if ((lbuf = setlocale(LC_CTYPE, (char *) NULL)) == NULL) {
#endif
	return( _xpstrdup( "" ) );
    }
    else {
	return( _xpstrdup( lbuf ) );
    }
}

#define _XPLOCALEHINTERDESC NULL


/******************************************************************************
 *
 * XpSetLocaleHinter
 */
void
XpSetLocaleHinter (
    XPHinterProc hinter_proc,
    char         *hinter_desc
)
{

    _XLockMutex(_Xglobal_lock);

    /*
     * Free up existing hinter description.
     */
    if (_xp_hinter_desc)
	XFree( _xp_hinter_desc );

    /*
     * Either install the default hinter, or install the
     * one provided by the caller.
     */
    if (!hinter_proc) {
	_xp_hinter_proc = _XpLocaleHinter;
	_xp_hinter_desc = _xpstrdup( _XPLOCALEHINTERDESC );
	_xp_hinter_init = 0;
    }
    else {
	_xp_hinter_proc = hinter_proc;
	_xp_hinter_desc = _xpstrdup( hinter_desc );
	_xp_hinter_init = 0;
    }

    _XUnlockMutex(_Xglobal_lock);
}


/******************************************************************************
 *
 * XpGetLocaleHinter
 */
char *
XpGetLocaleHinter (
    XPHinterProc *hinter_proc
)
{
    char *tmpstr;

    _XLockMutex(_Xglobal_lock);

    if (_xp_hinter_init) {
	_xp_hinter_proc = _XpLocaleHinter;
	_xp_hinter_desc = _xpstrdup( _XPLOCALEHINTERDESC );
	_xp_hinter_init = 0;
    }

    *hinter_proc = _xp_hinter_proc;
    tmpstr = _xpstrdup( _xp_hinter_desc );

    _XUnlockMutex(_Xglobal_lock);

    return( tmpstr );
}


/******************************************************************************
 *
 * XpGetLocaleNetString
 *
 * this is the routine that uses the locale hint machinery
 * to construct the actual "locale_hint" that is passed in
 * the various protocols and ICCCM-selection requests.
 *
 * A "Locale Net String" is made up of two components, a "locale hint"
 * that is the locale, and a "locale description" that is a description
 * of how the locale was derived.
 *
 * If a locale hint and description are available, then the net string
 * will be the description, and if the description contains the
 * keyword "%locale%", that keyword will be replaced with the locale
 * hint.  For example:
 *
 *      locale_desc = XOPEN;%locale%;01_00;XFN-001001
 *      locale_hint = de_DE
 *
 *      result is   = XOPEN;de_DE;01_00;XFN-001001
 *
 * If only a locale description is available, then it becomes the
 * entire net string.
 *
 * If only a locale hint is available, then it becomes the
 * entire net string.
 *
 * If neither a hint or description exists, a NULL is returned.
 */
char *XpGetLocaleNetString()
{
    XPHinterProc  locale_hinter;

    char          *locale_desc;
    char          *locale_hint;
    char          *tptr1;
    char          *locale_net_string;


    /*
     * Fetch the current locale hinter machinery.
     */
    locale_desc = XpGetLocaleHinter( &locale_hinter );

    /*
     * Run it.
     */
    locale_hint = (locale_hinter)();

    /*
     * Use locale_desc and locale_hint to build a full
     * locale net string.
     */
    if (locale_desc && locale_hint) {
	/*
	 * Combine the two portions to form locale_net_string.
	 *
	 * For example:
	 */

	tptr1 = strstr( locale_desc, "%locale%" );

	if (tptr1) {
	    /*
	     * Insert locale_hint into locale_desc.
	     *
	     * Note: strlen("%locale%") = 8
	     */
	    locale_net_string = Xmalloc( strlen(locale_desc) - 8 +
					 strlen(locale_hint) + 1 );

	    /*
	     * Copy first portion of locale_desc.
	     */
	    *tptr1 = '\0';
	    strcpy( locale_net_string, locale_desc );

	    /*
	     * Copy middle portion consisting of locale_hint.
	     */
	    strcat( locale_net_string, locale_hint );

	    /*
	     * Copy first portion of locale_desc.
	     */
	    tptr1 = tptr1 + 8;		/* skip by %Locale% keyword */

	    strcat( locale_net_string, tptr1 );

	    /*
	     * Free up and return.
	     */
	    XFree( locale_hint );
	    XFree( locale_desc );
	    return( locale_net_string );
	}
	else {
	    /*
	     * Since a locale_desc was given WITHOUT a place to insert
	     * the locale_hint, the locale_desc wins out.
	     */
	    XFree( locale_hint );
	    return( locale_desc );
	}
    }
    else if (locale_desc) {
	return( locale_desc );
    }
    else if (locale_hint) {
	return( locale_hint );
    }
    else {
	return( (char *) NULL );
    }
}

