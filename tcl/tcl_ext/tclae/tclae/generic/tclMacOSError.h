/* -*- mode: C; coding: macintosh; -*- (nowrap)
 * ###################################################################
 *  tclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclMacOSError.h"
 *                                    created: 8/20/1999 {9:28:03 PM} 
 *                                last update: 2/15/00 {9:37:30 PM} 
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: POMODORO no seisan
 *     www: http://www.his.com/~jguyer/
 *  
 * ========================================================================
 *               Copyright © 1999-2000 Jonathan Guyer
 * ========================================================================
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that the copyright notice and warranty disclaimer appear in
 * supporting documentation.
 * 
 * Jonathan Guyer disclaims all warranties with regard to this software,
 * including all implied warranties of merchantability and fitness.  In
 * no event shall Jonathan Guyer be liable for any special, indirect or
 * consequential damages or any damages whatsoever resulting from loss of
 * use, data or profits, whether in an action of contract, negligence or
 * other tortuous action, arising out of or in connection with the use or
 * performance of this software.
 * ========================================================================
 *  Description: 
 * 
 *  History
 * 
 *  modified   by  rev reason
 *  ---------- --- --- -----------
 *  8/20/1999  JEG 1.0 original
 * ###################################################################
 */

#pragma once

#ifndef _TCL_MACOSERROR
#define _TCL_MACOSERROR

#ifndef _TCL
#include <tcl.h>
#endif
#ifndef __MACTYPES__
#include <MacTypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

char *	Tcl_MacOSError(Tcl_Interp *interp, OSStatus err);

#ifdef __cplusplus
}
#endif

#endif /* _TCL_MACOSERROR */
