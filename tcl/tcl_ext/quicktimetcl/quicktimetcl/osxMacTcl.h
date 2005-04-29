#ifndef OSXMACTCL_H
#define OSXMACTCL_H
#pragma once

#if TARGET_RT_MAC_MACHO

int	Tcl_BeepObjCmd (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST84 objv[]);

OSErr FSpLocationFromPath (int length, CONST84 char *path, FSSpecPtr fileSpecPtr);

OSErr FSpPathFromLocation (FSSpecPtr spec, int* length, Handle *fullPath);

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

#endif

#endif
 