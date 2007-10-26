/* $Xorg: globals.h,v 1.4 2001/02/09 02:03:30 xorgcvs Exp $ */

/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/SM/globals.h,v 1.4 2001/12/14 19:53:55 dawes Exp $ */

/*
 * Author: Ralph Mor, X Consortium
 */

extern void _SmcDefaultErrorHandler ();
extern void _SmsDefaultErrorHandler ();

extern IcePoAuthStatus _IcePoMagicCookie1Proc ();
extern IcePaAuthStatus _IcePaMagicCookie1Proc ();

extern void _SmcProcessMessage ();
extern void _SmsProcessMessage ();

int 	_SmcOpcode = 0;
int 	_SmsOpcode = 0;

int	_SmVersionCount = 1;

IcePoVersionRec	_SmcVersions[] = {
	  	    {SmProtoMajor, SmProtoMinor, _SmcProcessMessage}};

IcePaVersionRec _SmsVersions[] = {
	  	    {SmProtoMajor, SmProtoMinor, _SmsProcessMessage}};

int		_SmAuthCount = 1;
char		*_SmAuthNames[] = {"MIT-MAGIC-COOKIE-1"};
IcePoAuthProc 	_SmcAuthProcs[] = {_IcePoMagicCookie1Proc};
IcePaAuthProc 	_SmsAuthProcs[] = {_IcePaMagicCookie1Proc};

#ifndef __UNIXOS2__
SmsNewClientProc _SmsNewClientProc;
SmPointer        _SmsNewClientData;
#else
SmsNewClientProc _SmsNewClientProc = 0;
SmPointer        _SmsNewClientData = 0;
#endif

SmcErrorHandler _SmcErrorHandler = _SmcDefaultErrorHandler;
SmsErrorHandler _SmsErrorHandler = _SmsDefaultErrorHandler;
