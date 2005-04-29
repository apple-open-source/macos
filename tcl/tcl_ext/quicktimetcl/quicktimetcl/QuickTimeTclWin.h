/*
 * QuickTimeTclWin.h --
 *
 *		A collection of Windows specific stuff that can be found in 
 *      'MW_QuickTimeTclHeader.pch' on the Mac version.
 *		It is part of the QuickTimeTcl package which provides Tcl/Tk bindings for QuickTime.
 *
 * Copyright (c) 2000-2003  Mats Bengtsson
 *
 * $Id: QuickTimeTclWin.h,v 1.2 2004/05/29 10:33:47 matben Exp $
 */


#ifdef _WIN32
#ifndef _QUICKTIMETCLWIN_H
#define _QUICKTIMETCLWIN_H 1

#	define USE_NON_CONST

/*
 * Windows specific stuff.
 */

#   include <windows.h>
#   include <Stdlib.h>

/*
 * QuickTime stuff.
 */

#   include <QTML.h>
#   include <Movies.h>
#   include <MoviesFormat.h>
#   include <QuickTimeComponents.h>
#   include <QuickTimeVR.h>
#   include <QuickTimeVRFormat.h>
#   include <FileTypesAndCreators.h>
#   include <MediaHandlers.h>
#   include <ImageCodec.h>

/*
 * Other Mac specific stuff ported(?) to Windows.
 */
 
#   include <Strings.h>
#   include <Gestalt.h>
#   include <FixMath.h>
#   include <Scrap.h>

/*
 * Windows specific Tcl/Tk stuff.
 */

#   include <TkWinInt.h>

#endif      /* end of _QUICKTIMETCLWIN_H */
#endif      /* end of _WIN32 */
