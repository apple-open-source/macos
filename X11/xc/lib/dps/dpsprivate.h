/*
 * dpsprivate.h -- Private interface for the DPS client library implementation.
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */

#ifndef	DPSPRIVATE_H
#define	DPSPRIVATE_H

#include "DPS/dpsconfig.h"
#include "DPS/dpsclient.h"

typedef enum {
  dps_hiFirst, dps_loFirst
  } DPSByteOrder;

typedef enum {
  dps_ieee, dps_native
  } DPSNumFormat;

typedef void (*DPSClientPrintProc)(
  DPSContext		/* cid */,
  char *		/* buf */,
  unsigned int		/* count */);
  /* Call-back procedure to handle output from the PostScript server for
     context with id 'cid'. Passed to DPSServicePostScript.
    'buf' contains 'count' bytes. */

extern void DPSInitClient(
  DPSTextProc		textProc,
  void			(* /* releaseProc */) (char *, char *));
/* Initialize the environment-specific parts of the client library */

#ifndef DPSDefaultProgramEncoding
#define DPSDefaultProgramEncoding dps_binObjSeq
#endif /* not DPSDefaultProgramEncoding */

#ifndef DPSDefaultByteOrder
#if SWAPBITS
#define DPSDefaultByteOrder dps_loFirst
#else /* SWAPBITS */
#define DPSDefaultByteOrder dps_hiFirst
#endif /* SWAPBITS */
#endif /* DPSDefaultByteOrder */

#ifndef DPSDefaultNumFormat
#if IEEEFLOAT
#define DPSDefaultNumFormat dps_ieee
#else /* IEEEFLOAT */
#define DPSDefaultNumFormat dps_native
#endif /* IEEEFLOAT */
#endif /* not DPSDefaultNumFormat */

#ifndef DPSDefaultNameEncoding
#define DPSDefaultNameEncoding dps_indexed
#endif /* not DPSDefaultNameEncoding */

extern void DPSCheckRaiseError(DPSContext c);
  /* Checks the resynching flag in a DPSPrivContext and raises an exception
     if true */

/* system name table boundaries */
#define DPS_LAST_COMMON_SYSNAME		212
#define DPS_FIRST_AUX_SYSNAME		256
#define DPS_LAST_AUX_SYSNAME		427

extern char **DPSSysNames;
extern char **DPSSysNamesAux;

  /* System name tables.  DPSSysNames[index] is the string for the name.
     DPSSysNamesAux[index - DPS_FIRST_AUX_SYSNAME] is the string
     for an uncommon system name. */

extern int DPSInitialize(void);
extern void DPSInitSysNames(void);

extern char *DPSSetWh(DPSContext /* ctxt */, char * /* newWh */);
  /* set new window handle, returns old window handle */

extern void DPSOutOfMemory(void);
  /*
     This is called by the DPS software when it cannot allocate any more
     storage from the heap (e.g., when malloc returns NIL).

     MAC ONLY
     DPSOutOfMemory calls CantHappen (see dpsexcept.h) unless you
     have used DPSSetOutOfMemoryProc to establish a different procedure
     to be called, in which case DPSOutOfMemory calls your procedure instead.
     If your procedure returns, DPSOutOfMemory returns to its caller, which
     re-attempts the allocation. Note that another allocation failure will
     cause DPSOutOfMemory to be called again (and again ...).
     
     */

#endif /* DPSPRIVATE_H */
