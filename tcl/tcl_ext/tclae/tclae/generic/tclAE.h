/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  tclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAE.h"
 *                                    created: 8/20/1999 {9:31:41 PM} 
 *                                last update: 8/21/1999 {11:14:26 AM} 
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: POMODORO no seisan
 *     www: http://www.his.com/~jguyer/
 *  
 * ========================================================================
 *               Copyright © 1999 Jonathan Guyer
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

#ifndef _TCLAE
#define _TCLAE

#include "tclAEid.h"

#ifdef __cplusplus
extern "C" {
#endif

int Tclae_Init(Tcl_Interp *interp);

#ifdef __cplusplus
}
#endif

#endif /* _TCLAE */
