/* $Xorg: Prefs.h,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
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

#ifndef _Prefs_h
#define _Prefs_h

#include <X11/Intrinsic.h>

typedef struct {
    unsigned int mask;
    unsigned int value;
} AddressFilter;

typedef struct {
    Boolean has_fwp;
    AddressFilter *internal_webservers;
    AddressFilter *trusted_webservers;
    AddressFilter *fast_webservers;
    int internal_webservers_count;
    int trusted_webservers_count;
    int fast_webservers_count;
} Preferences;


extern void GetPreferences(Widget widget, Preferences *preferences);
extern void FreePreferences(Preferences *preferences);
extern void ComputePreferences(Preferences *preferences, char *webserver,
	     Boolean *trusted_ret, Boolean *use_fwp_ret, Boolean *use_lbx_ret);

#endif /* _Prefs_h */
