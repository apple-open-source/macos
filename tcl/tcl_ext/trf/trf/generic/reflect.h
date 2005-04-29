#ifndef TRF_REFLECT_H
#define TRF_REFLECT_H

/* -*- c -*-
 * reflect.h -
 *
 * internal definitions shared by 'reflect.c' and 'ref_opt.c'.
 *
 * Copyright (c) 1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: reflect.h,v 1.2 2001/03/27 13:08:32 tcl Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "transformInt.h"

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_Trf
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _ReflectControl_ {

  Trf_WriteProc* write;
  ClientData     writeClientData;
  Tcl_Obj*       command; /* tcl code to execute for a buffer */
  Tcl_Interp*    interp;  /* interpreter creating the channel */

  /* Information queried dynamically from the tcl-level */

  int                 maxRead;
  Trf_SeekInformation naturalRatio;

} ReflectControl;

/*
 * Execute callback for buffer and operation.
 */

extern int
RefExecuteCallback _ANSI_ARGS_ ((ReflectControl* ctrl,
				 Tcl_Interp*     interp,
				 unsigned char*  op,
				 unsigned char*  buf,
				 int             bufLen,
				 int             transmit,
				 int             preserve));


/* Allowed values for transmit.
 */

#define TRANSMIT_DONT  (0) /* No transfer to do */
#define TRANSMIT_DOWN  (1) /* Transfer to the underlying channel */
#define TRANSMIT_NUM   (4) /* Transfer number to 'maxRead' */
#define TRANSMIT_RATIO (5) /* 2-element list containing seek ratio */


#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef __cplusplus
}
#endif /* C++ */
#endif /* TRF_REFLECT_H */
