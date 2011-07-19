/* -*- mode: C; coding: macintosh; -*- */

#ifndef OSXMACTCL_H
#define OSXMACTCL_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
	
#if TARGET_RT_MAC_MACHO

OSErr FSpLocationFromPath (int length, CONST84 char *path, FSRefPtr fileRefPtr);

OSErr FSpPathFromLocation (FSRefPtr fsrefP, int* length, Handle *fullPath);

/* CFString to external DString */
int CFStringToExternalDString(Tcl_Interp * interp, CFStringRef strRef, Tcl_DString * dsPtr);

/* CFString to DString */
int CFStringToUtfDString(Tcl_Interp * interp, CFStringRef strRef, Tcl_DString * dsPtr);

/* decomposed utf8 buffer to external DString */
int DUtfToExternalDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr);

/* decomposed utf8 buffer to DString */
int DUtfToUtfDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr);

/* external buffer to decomposed utf8 DString */
int ExternalToDUtfDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr);

/* utf8 buffer to decomposed utf8 DString */
int UtfToDUtfDString(Tcl_Interp * interp, CONST84 char * src, int length, Tcl_DString * dsPtr);

/* CFStringRef to decomposed Unicode Tcl_Obj */
Tcl_Obj * CFStringToTclObj(CFStringRef strRef);

/* Unicode Tcl_Obj * to CFStringRef */
CFStringRef TclObjToCFString(Tcl_Obj * inObj);

#endif

#ifdef __cplusplus
}
#endif

#endif
