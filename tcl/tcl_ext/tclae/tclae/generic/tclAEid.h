/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  tclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEid.h"
 *                                    created: 8/20/1999 {9:31:41 PM} 
 *                                last update: 7/31/10 {11:45:46 PM}
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: POMODORO no seisan
 *     www: http://www.his.com/~jguyer/
 *  
 * ========================================================================
 *               Copyright © 1999-2009 Jonathan Guyer
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

#ifndef _TCLAE_ID
#define _TCLAE_ID

#define TCLAE_NAME			PACKAGE_NAME

#if TARGET_API_MAC_CARBON // das 25/10/00: Carbonization
#define TCLAE_FILENAME			"TclAECarbon"
#else
#define TCLAE_FILENAME			"TclAE"
#endif

#define	TCLAE_MAJOR			2				// BCD (0—99)
#define	TCLAE_MINOR			0				// BCD (0—9)
#define	TCLAE_PATCH			5				// BCD (0—9)
#define	TCLAE_STAGE			finalStage			// {developStage, alphaStage, betaStage, finalStage}
#define TCLAE_PRERELEASE	0				// unsigned binary (0—255)

#define TCLAE_VERSION		PACKAGE_VERSION
#define TCLAE_BASIC_VERSION PACKAGE_VERSION

#endif /* _TCLAE_ID */
