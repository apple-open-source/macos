/* $Xorg: Global.c,v 1.4 2001/02/09 02:05:57 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#include "RxPlugin.h"

#include <stdio.h>

/* global variable to handle data global to all instances */
PluginGlobal RxGlobal;


NPError
NPP_Initialize(void)
{
#ifdef PLUGIN_TRACE
    (void) fprintf (stderr, "%s\n", "NPP_Initialize");
#endif
    RxGlobal.has_appgroup = RxUndef;
    RxGlobal.has_real_server = RxUndef;
    RxGlobal.has_ui_lbx = RxUndef;
    RxGlobal.has_print_lbx = RxUndef;
    RxGlobal.has_printer = RxUndef;
    RxGlobal.has_ui_fwp = RxUndef;
    RxGlobal.has_print_fwp = RxUndef;
    RxGlobal.pdpy_name = NULL;
    RxGlobal.printer_name = NULL;
    RxGlobal.fwp_dpyname = NULL;
    RxGlobal.pfwp_dpyname = NULL;
    RxGlobal.ice_conn = NULL;
    RxGlobal.pm_opcode = 0;
    RxGlobal.get_prefs = True;
    RxGlobal.dpy = NULL;
    RxGlobal.pdpy = NULL;
    RxGlobal.wm_delete_window = 0;
    RxGlobal.wm_protocols = 0;
    return NPERR_NO_ERROR;
}

void
NPP_Shutdown(void)
{
#ifdef PLUGIN_TRACE
    (void) fprintf (stderr, "%s\n", "NPP_Shutdown");
#endif
/*
 * Netscape doesn't try to cache plug-ins at all, and it unloads
 * the plug-in immediately after the last instance that uses it is
 * gone. So if you load a page that uses the plug-in, and then reload 
 * the page, Netscape will unload the plug-in and call NPP_Shutdown. 
 * Then it will reload the plug-in and call NPP_Initialize. And when
 * it reloads it, zeroes the plug-in's DATA segment so even if we 
 * tried to be smart in NPP_Initialize, Netscape would defeat us 
 * anyway.
 */
    if (RxGlobal.pdpy_name != NULL)
	NPN_MemFree(RxGlobal.pdpy_name);
    if (RxGlobal.printer_name != NULL)
	NPN_MemFree(RxGlobal.printer_name);
    if (RxGlobal.fwp_dpyname != NULL)
	NPN_MemFree(RxGlobal.fwp_dpyname);
    if (RxGlobal.pfwp_dpyname != NULL)
	NPN_MemFree(RxGlobal.pfwp_dpyname);
    if (RxGlobal.get_prefs == False)
	FreePreferences(&RxGlobal.prefs);
    if (RxGlobal.pdpy != NULL && RxGlobal.pdpy != RxGlobal.dpy)
	XCloseDisplay(RxGlobal.pdpy);
    if (RxGlobal.ice_conn) {
	IceProtocolShutdown (RxGlobal.ice_conn, RxGlobal.pm_opcode);
	IceCloseConnection (RxGlobal.ice_conn);
    }
}
