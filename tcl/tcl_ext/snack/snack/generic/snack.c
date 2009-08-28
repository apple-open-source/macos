/* 
 * Copyright (C) 1997-2004 Kare Sjolander <kare@speech.kth.se>
 *
 * This file is part of the Snack Sound Toolkit.
 * The latest version can be found at http://www.speech.kth.se/snack/
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

#include <string.h>
#include "tk.h"
#include "snack.h"
#include "jkCanvItems.h"

#if defined(__WIN32__)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN

#ifdef __cplusplus
extern "C" {
#endif

EXTERN int Snack_Init(Tcl_Interp *interp);
EXTERN int Snack_SafeInit(Tcl_Interp *interp);

#ifdef __cplusplus
}
#endif

HINSTANCE snackHInst = NULL;

BOOL APIENTRY
DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  snackHInst = hInst;
  return TRUE;
}
#endif

#ifdef MAC
#include <string.h>
int main (void)
{
	return 0;
}

double
hypotd(double x, double y)
{
    double sum;

    sum = x*x + y*y;
    return sqrt(sum);
}
#endif

extern Tk_ItemType snackWaveType;
extern Tk_ItemType snackSpectrogramType;
extern Tk_ItemType snackSectionType;
extern Tk_CustomOption waveTagsOption;
extern Tk_CustomOption spegTagsOption;
extern Tk_CustomOption sectTagsOption;

#define play_width 19
#define play_height 19
static unsigned char play_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
   0x78, 0x00, 0x00, 0xf8, 0x01, 0x00, 0xf8, 0x07, 0x00, 0xf8, 0x1f, 0x00,
   0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x1f, 0x00, 0xf8, 0x07, 0x00,
   0xf8, 0x01, 0x00, 0x78, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define rec_width 19
#define rec_height 19
static unsigned char rec_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0x00,
   0xe0, 0x1f, 0x00, 0xf0, 0x3f, 0x00, 0xf0, 0x3f, 0x00, 0xf8, 0x7f, 0x00,
   0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf0, 0x3f, 0x00,
   0xf0, 0x3f, 0x00, 0xe0, 0x1f, 0x00, 0x80, 0x07, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define stop_width 19
#define stop_height 19
static unsigned char stop_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f, 0x00,
   0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00,
   0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00,
   0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0xf8, 0x7f, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define pause_width 19
#define pause_height 19
static unsigned char pause_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7c, 0x00,
   0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00,
   0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00,
   0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00, 0xf8, 0x7c, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define playnext_width 20
#define playnext_height 19
static unsigned char playnext_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xe0, 0x00, 
  0x78, 0xe0, 0x00, 0xf8, 0xe1, 0x00, 0xf8, 0xe7, 0x00, 0xf8, 0xff, 0x00, 
  0xf8, 0xff, 0x00, 0xf8, 0xff, 0x00, 0xf8, 0xff, 0x00, 0xf8, 0xe7, 0x00, 
  0xf8, 0xe1, 0x00, 0x78, 0xe0, 0x00, 0x18, 0xe0, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define playprev_width 20
#define playprev_height 19
static unsigned char playprev_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0xc0, 0x00, 
  0x38, 0xf0, 0x00, 0x38, 0xfc, 0x00, 0x38, 0xff, 0x00, 0xf8, 0xff, 0x00, 
  0xf8, 0xff, 0x00, 0xf8, 0xff, 0x00, 0xf8, 0xff, 0x00, 0x38, 0xff, 0x00, 
  0x38, 0xfc, 0x00, 0x38, 0xf0, 0x00, 0x38, 0xc0, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Tcl_Channel snackDebugChannel = NULL;
static Tcl_Interp *debugInterp = NULL;
int debugLevel = 0;
char *snackDumpFile = NULL;

int
Snack_DebugCmd(ClientData cdata, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  int len;
  char *str;
  CONST84 char *patchLevelStr;
  
  if (objc > 1) {
    if (Tcl_GetIntFromObj(interp, objv[1], &debugLevel) != TCL_OK)
      return TCL_ERROR;
  }
  if (objc >= 3) {
    if (Tcl_IsSafe(interp)) {
      Tcl_AppendResult(interp, "can not open log file in a safe interpreter",
		       (char *) NULL);
      return TCL_ERROR;
    }
    str = Tcl_GetStringFromObj(objv[2], &len);
    if (len > 0) {
      snackDebugChannel = Tcl_OpenFileChannel(interp, str, "w", 420);
      if (snackDebugChannel == 0) {
	return TCL_ERROR;
      }
    }
  }
  if (objc == 4) {
    if (Tcl_IsSafe(interp)) {
      Tcl_AppendResult(interp, "can not open dump file in a safe interpreter",
		       (char *) NULL);
      return TCL_ERROR;
    }
    str = Tcl_GetStringFromObj(objv[3], &len);
    snackDumpFile = (char *) ckalloc(len + 1);
    strcpy(snackDumpFile, str);
  }
  if (debugLevel > 0) {
    patchLevelStr = Tcl_GetVar(interp, "snack::patchLevel", TCL_GLOBAL_ONLY);
    Tcl_Write(snackDebugChannel, "Snack patch level: ", 19);
    Tcl_Write(snackDebugChannel, patchLevelStr, strlen(patchLevelStr));
    Tcl_Write(snackDebugChannel, "\n", 1);
    Tcl_Flush(snackDebugChannel);
  }

  return TCL_OK;
}

#ifdef SNACK_CSLU_TOOLKIT
extern int fromCSLUshWaveCmd(Sound *s, Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]);
extern int toCSLUshWaveCmd(Sound *s, Tcl_Interp *interp, int objc,
			   Tcl_Obj *CONST objv[]);
#endif

int useOldObjAPI = 0;

int
Snack_setUseOldObjAPI(ClientData cdata, Tcl_Interp *interp, int objc,
		      Tcl_Obj *CONST objv[])
{
  useOldObjAPI = 1;

  return TCL_OK;
}


static int initialized = 0;
int littleEndian = 0;

#ifdef __cplusplus
extern "C" SnackStubs *snackStubs;
#else
extern SnackStubs *snackStubs;
#endif

extern Tcl_HashTable *filterHashTable;
extern Tcl_HashTable *hsetHashTable;
extern Tcl_HashTable *arHashTable;

#if defined(Tcl_InitHashTable) && defined(USE_TCL_STUBS)
#undef Tcl_InitHashTable
#define Tcl_InitHashTable (tclStubsPtr->tcl_InitHashTable)
#endif

extern char defaultOutDevice[];
int defaultSampleRate = 16000;

int
Snack_Init(Tcl_Interp *interp)
{
  Tcl_CmdInfo infoPtr;
  CONST84 char *version;
  Tcl_HashTable *soundHashTable;
  union {
    char c[sizeof(short)];
    short s;
  } order;
  char rates[100];
  
#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8", 0) == NULL) {
    return TCL_ERROR;
  }
#endif

  version = Tcl_GetVar(interp, "tcl_version",
		       (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG));
  
  if (strcmp(version, "8.0") == 0) {
    useOldObjAPI = 1;
  }

#ifdef TCL_81_API
  if (Tcl_PkgProvideEx(interp, "snack", SNACK_VERSION,
		       (ClientData) &snackStubs) != TCL_OK) {
    return TCL_ERROR;
  }
#else
  if (Tcl_PkgProvide(interp, "snack", SNACK_VERSION) != TCL_OK) {
    return TCL_ERROR;
  }
#endif

  if (Tcl_GetCommandInfo(interp, "button", &infoPtr) != 0) {
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8", 0) == NULL) {
      return TCL_ERROR;
    }
#endif
	
    if (initialized == 0) {
      Tk_CreateItemType(&snackWaveType);
      Tk_CreateItemType(&snackSpectrogramType);
      Tk_CreateItemType(&snackSectionType);
    }
#if !defined(MAC) && !defined(MAC_OSX_TCL)
    Tk_DefineBitmap(interp, Tk_GetUid("play"),   (char *) play_bits,
		    play_width, play_height);
    Tk_DefineBitmap(interp, Tk_GetUid("record"), (char *) rec_bits,
		    rec_width, rec_height);
    Tk_DefineBitmap(interp, Tk_GetUid("stop"),   (char *) stop_bits,
		    stop_width, stop_height);
    Tk_DefineBitmap(interp, Tk_GetUid("pause"),  (char *) pause_bits,
		    pause_width, pause_height);
#endif
    Tk_DefineBitmap(interp, Tk_GetUid("snackPlay"), (char *) play_bits,
		    play_width, play_height);
    Tk_DefineBitmap(interp, Tk_GetUid("snackRecord"), (char *) rec_bits,
		    rec_width, rec_height);
    Tk_DefineBitmap(interp, Tk_GetUid("snackStop"), (char *) stop_bits,
		    stop_width, stop_height);
    Tk_DefineBitmap(interp, Tk_GetUid("snackPause"), (char *) pause_bits,
		    pause_width, pause_height);
    Tk_DefineBitmap(interp, Tk_GetUid("snackPlayNext"), (char *) playnext_bits,
		    playnext_width, playnext_height);
    Tk_DefineBitmap(interp, Tk_GetUid("snackPlayPrev"), (char *) playprev_bits,
		    playprev_width, playprev_height);
    waveTagsOption.parseProc = Tk_CanvasTagsParseProc;
    waveTagsOption.printProc = Tk_CanvasTagsPrintProc;
    spegTagsOption.parseProc = Tk_CanvasTagsParseProc;
    spegTagsOption.printProc = Tk_CanvasTagsPrintProc;
    sectTagsOption.parseProc = Tk_CanvasTagsParseProc;
    sectTagsOption.printProc = Tk_CanvasTagsPrintProc;
  }

  soundHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
  filterHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
  hsetHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
  arHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));

  Tcl_CreateObjCommand(interp, "sound", Snack_SoundCmd,
		       (ClientData) soundHashTable, (Tcl_CmdDeleteProc *)NULL);
  
  Tcl_CreateObjCommand(interp, "snack::sound", Snack_SoundCmd,
		       (ClientData) soundHashTable, Snack_SoundDeleteCmd);
  
  Tcl_CreateObjCommand(interp, "audio", Snack_AudioCmd,
		       NULL, (Tcl_CmdDeleteProc *)NULL);
  
  Tcl_CreateObjCommand(interp, "snack::audio", Snack_AudioCmd,
		       NULL, Snack_AudioDeleteCmd);
  
  Tcl_CreateObjCommand(interp, "snack::mixer", Snack_MixerCmd,
		       NULL, Snack_MixerDeleteCmd);
  
  Tcl_CreateObjCommand(interp, "snack::filter", Snack_FilterCmd,
		       (ClientData) filterHashTable, Snack_FilterDeleteCmd);

  Tcl_CreateObjCommand(interp, "snack::hset", Snack_HSetCmd,
		       (ClientData) hsetHashTable, Snack_HSetDeleteCmd);

  Tcl_CreateObjCommand(interp, "snack::ca", Snack_arCmd,
		       (ClientData) arHashTable, Snack_arDeleteCmd);

  Tcl_CreateObjCommand(interp, "snack::isyn", isynCmd,
		       NULL, (Tcl_CmdDeleteProc *)NULL);

  Tcl_CreateObjCommand(interp, "snack::osyn", osynCmd,
		       NULL, (Tcl_CmdDeleteProc *)NULL);
   
  Tcl_CreateObjCommand(interp, "snack::debug",
		       (Tcl_ObjCmdProc*) Snack_DebugCmd,
		       NULL, (Tcl_CmdDeleteProc *)NULL);

  Tcl_CreateObjCommand(interp, "snack::setUseOldObjAPI",
		       (Tcl_ObjCmdProc*) Snack_setUseOldObjAPI,
		       NULL, (Tcl_CmdDeleteProc *)NULL);

  snackDebugChannel = Tcl_GetStdChannel(TCL_STDERR);
  debugInterp = interp;
  
  Tcl_SetVar(interp, "snack::patchLevel", SNACK_PATCH_LEVEL, TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "snack::version",    SNACK_VERSION,     TCL_GLOBAL_ONLY);

  Tcl_InitHashTable(soundHashTable, TCL_STRING_KEYS);
  Tcl_InitHashTable(filterHashTable, TCL_STRING_KEYS);
  Tcl_InitHashTable(hsetHashTable, TCL_STRING_KEYS);
  Tcl_InitHashTable(arHashTable, TCL_STRING_KEYS);

  if (initialized == 0) {
    SnackDefineFileFormats(interp);
    SnackCreateFilterTypes(interp);
    
    SnackAudioInit();
    
    Tcl_CreateExitHandler(Snack_ExitProc, (ClientData) NULL);
    
    initialized = 1;
  }
#ifdef SNACK_CSLU_TOOLKIT
  Snack_AddSubCmd(SNACK_SOUND_CMD, "fromCSLUshWave",
		  (Snack_CmdProc *) fromCSLUshWaveCmd, NULL);
  Snack_AddSubCmd(SNACK_SOUND_CMD, "toCSLUshWave",
		  (Snack_CmdProc *) toCSLUshWaveCmd, NULL);
#endif

  /* Compute the byte order of this machine. */

  order.s = 1;
  if (order.c[0] == 1) {
    littleEndian = 1;
  }
  
  /* Determine a default sample rate for this machine, usually 16kHz. */
  
  SnackAudioGetRates(defaultOutDevice, rates, 100);
  if (strstr(rates, "16000") != NULL ||
      sscanf(rates, "%d", &defaultSampleRate) != 1) {
    defaultSampleRate = 16000;
  }

  return TCL_OK;
}

int
Snack_SafeInit(Tcl_Interp *interp)
{
  return Snack_Init(interp);
}

void
Snack_WriteLog(char *str)
{
  if (snackDebugChannel == NULL) {
    snackDebugChannel = Tcl_OpenFileChannel(debugInterp, "_debug.txt", "w",
					    420);
  }
  Tcl_Write(snackDebugChannel, str, strlen(str));
  Tcl_Flush(snackDebugChannel);
}

void
Snack_WriteLogInt(char *str, int num)
{
  char buf[20];

  if (snackDebugChannel == NULL) {
    snackDebugChannel = Tcl_OpenFileChannel(debugInterp, "_debug.txt", "w",
					    420);
  }
  Tcl_Write(snackDebugChannel, str, strlen(str));
  sprintf(buf, " %d", num);
  Tcl_Write(snackDebugChannel, buf, strlen(buf));
  Tcl_Write(snackDebugChannel, "\n", 1);
  Tcl_Flush(snackDebugChannel);
}

/*
static double snack_t0;

void
TimerStart() {
  snack_t0 = SnackCurrentTime();
}

void
TimerStop() {
  char buf[20];
  sprintf(buf, "%f", SnackCurrentTime()-snack_t0);
  Tcl_Write(snackDebugChannel, buf, strlen(buf));
  Tcl_Write(snackDebugChannel, "\n", 1);
  Tcl_Flush(snackDebugChannel);
}
*/
