/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  sl_words.h - Headers for the Secondary Loader Words.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_SL_WORDS_H_
#define _BOOTX_SL_WORDS_H_

#include <ci.h>

// The iHandle for the SL Words.
extern CICell SLWordsIH;

// Call InitSLWords before using SLWordsIH.
// outputLevel is used to supress output.
// 0 - all off
// 1 - OF stdout only
// 2 - SLW's emit and cr.
// 3 - ???
extern long InitSLWords(void);


//  Suported words.

//  slw_set_output_level ( level -- )
//   set the current output level.
extern void SetOutputLevel(long level);

//  slw_emit ( ch -- )                     Output Level: 2
//    calls emit ( ch -- )
extern void Emit(char ch);

//  slw_cr ( -- )                          Output Level: 2
//    calls cr ( -- )
extern void CR(void);

//  slw_init_keymap ( keyboardIH -- keyMap )
//  sets the ihandle for the keyboard and
//  puts the address of the keyMap on the stack
extern char *InitKeyMap(CICell keyboardIH);

//  slw_update_keymap ( -- )
//  Called by slw_spin to make sure all the keys are caught.
extern void UpdateKeyMap(void);

// slw_spin_init ( screenIH cursorAddr cursorX cursorY cursorW cursorH --)
//    Sets up the wait cursor.
extern void SpinInit(CICell screenIH, char *cursorAddr,
		     long cursorX, long cursorY,
		     long cursorW, long cursorH,
		     long frames, long fps,
		     long pixelSize, long spare);

// slw_spin ( -- )
//    Spins the wait cursor.
extern void Spin(void);

extern long GetPackageProperty(CICell phandle, char *propName,
			       char **propAddr, long *propLen);

// slw_pwd ( phandle addr len -- act )
// does pwd the hard way.

#define SL_DEBUG (0)

/*
  .sc ( -- )
    does a simple stack crawl.



 */

#endif /* ! _BOOTX_SL_WORDS_H_ */
