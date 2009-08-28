/* 
 * Copyright (C) 1997-2003 Kare Sjolander <kare@speech.kth.se>
 *
 * This file contains a sample module which demonstrates how to write
 * extensions to the Snack sound extension for Tcl/Tk.
 * The latest version of Snack can be found at http://www.speech.kth.se/snack/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "snack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Windows magic */

#if defined(__WIN32__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#define EXPORT(a,b) __declspec(dllexport) a b
BOOL APIENTRY
DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
#else
#define EXPORT(a,b) a b
#endif

int
Square(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Sound *sound;
  int i;

  /* Get the sound structure for this sound */
      
  sound = Snack_GetSound(interp, Tcl_GetStringFromObj(objv[0], NULL));

  /* create a simple square wave */

  for (i = 0; i < Snack_GetLength(sound); i++) {
    if ((i/10)%2) {
      Snack_SetSample(sound, 0, i, 10000.0f);
    } else {
      Snack_SetSample(sound, 0, i, -10000.0f);
    }
  }

  /* update the max/min members of the sound structure */

  Snack_UpdateExtremes(sound, 0, sound->length, SNACK_NEW_SOUND);

  /* execute callbacks for stuff like canvas items */

  Snack_ExecCallbacks(sound, SNACK_NEW_SOUND);

  return TCL_OK;
}

/*
  Initialize the square package and create a new sound command 'square'.
  The syntax is: sndName square
 */

EXPORT(int, Square_Init)(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8", 0) == NULL) {
    return TCL_ERROR;
  }
#endif

#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8", 0) == NULL) {
      return TCL_ERROR;
    }
#endif

#ifdef USE_SNACK_STUBS
  if (Snack_InitStubs(interp, "2", 0) == NULL) {
    return TCL_ERROR;
  }
#endif

  if (Tcl_PkgProvide(interp, "square", "1.0") != TCL_OK) {
    return TCL_ERROR;
  }

  Snack_AddSubCmd(SNACK_SOUND_CMD, "square", (Snack_CmdProc *) Square, NULL);

  return TCL_OK;
}

EXPORT(int, Square_SafeInit)(Tcl_Interp *interp)
{
  return Square_Init(interp);
}

#ifdef __cplusplus
}
#endif
