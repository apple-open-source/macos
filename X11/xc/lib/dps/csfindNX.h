/*
 * csfindNX.h
 *
 * (c) Copyright 1992-1994 Adobe Systems Incorporated.
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

#ifndef CSFINDNX_H
#define CSFINDNX_H


/* ---DEFINES--- */

#define XDPSNX_DEFAULT_EXEC_NAME "dpsnx.agent"

/* ---TYPES--- */

typedef enum {
  findnx_found,
  findnx_not_found,
  findnx_error
} XDPSNXFindNXResult;


/* ---Functions--- */

extern XDPSNXFindNXResult
XDPSNXFindNX (
		   Display *dpy, /* Display client wants to attach to */
		   char *licenseMethod,     /* license method client speaks */
		   char **host,	           /* RETURN: Agent host name */
		   int *transport,         /* RETURN: Agent transport */
		   int *port);	           /* RETURN: Agent's port    */
     /*
      * Find an agent for this client that will service display dpy.
      * The licenseMethod arg. specifies the type of licensing that the client
      * is capable of speaking.  A (char *) NULL, means that it can handle
      * any type (i.e. master key in lock & key).
      * Return values indicates success in finding an agent.
      * Success: host, transport, and port indicate where the agent is.
      * !Success: the values of host, transport, and port are undefined.
      * The client of the call is responsible for freeing the space (XFree())
      * allocated for the host name string.
      */


extern void
XDPSGetNXArg (
		   int arg,	        /* Argument to be retrieved */
		   void **value);	   /* value of the argument */
     /*
       Gets the value of the given argument.  Returns the value in val.  If the
       value will fit in val (ie, a Bool) then the value is placed there,
       otherwise a pointer to the value is placed there.  It is assumed that
       the caller will know the correct type.  The types of arguments are
       detailed with the argument definitions in dpsNXargs.h.
       NOTE: the caller should NOT modify any values returned via pointer.
       XDPSGetNXArg does not copy the data, rather it returns a pointer to the
       internally held structure.
      */

#endif  /* CSFINDNX_H */
