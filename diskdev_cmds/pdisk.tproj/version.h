/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * version.h - version number for pdisk program
 *
 * Written by Eryk Vershen (eryk@apple.com)
 */

/*
 * Copyright 1997 by Apple Computer, Inc.
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */


/*
 * Defines
 */
/*
 *	TO ADJUST THE VERSION - change the following six macros.
 *
 * A version is of the form:	N.M{.X}{yZ}
 * 
 * 	N is two digits indicating the major version
 *	M is a single digit indicating relative revision
 *	X is a single digit indicating a bug fix revision
 *	y is a character from the set [dab] indicating stage (dev,alpha,beta)
 *	Z is two digits indicating the delta within the stage
 *
 * Note that within the 'vers' resource all these fields end up
 * comprising a four byte unsigned integer with the property that any later
 * version will be be represented by a larger number.
 */

#define	VERSION	"0.5a2"
#define RELEASE_DATE "22 July 1997"

#define	kVersionMajor	0x00		/* ie. N has two BCD digits */
#define	kVersionMinor	0x5		/* ie. M has a single BCD digit */
#define kVersionBugFix	0x0		/* ie. X has a single BCD digit */
#define	kVersionStage	alpha		/* ie. y is one of the set - */
					/*    {development,alpha,beta,final}
					 * also, release is a synonym for final
					 */
#define	kVersionDelta	0x02		/* ie. Z has two BCD digits */


/*
 * Types
 */


/*
 * Global Constants
 */


/*
 * Global Variables
 */


/*
 * Forward declarations
 */
