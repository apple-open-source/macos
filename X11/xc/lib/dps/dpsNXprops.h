/*
 * dpsNXprops.h
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
/* $XFree86: xc/lib/dps/dpsNXprops.h,v 1.3 2001/07/29 05:01:13 tsi Exp $ */

#ifndef DPSNXPROPS_H
#define DPSNXPROPS_H


/* ---DEFINES--- */

/* --- X Window Properties --- */

#define XDPSNX_BILLBOARD_PROP "_ADOBE_DPS_NX_BILLBOARD" /* On dpy root win */

/* Following are per Agent window properties */

#define XDPSNX_WILLINGNESS_PROP     "_ADOBE_DPS_NX_WILLINGNESS_PROP"
#define XDPSNX_LICENSE_METHOD_PROP  "_ADOBE_DPS_NX_LICENSE_METHODS_PROP"
#define XDPSNX_TRANSPORT_INFO_PROP  "_ADOBE_DPS_NX_TRANSPORT_INFO_PROP"
#define XDPSNX_HOST_NAME_PROP       "_ADOBE_DPS_NX_HOST_NAME_PROP"

#define XDPSNX_X_CLASS_NAME         "XDPSNX"
#define XDPSNX_X_RESOURCE           "xdpsnx.agenthost"
#define XDPSNX_X_CLASS              "XDPSNX.AgentHost"

#define AGENT_ENV_VAR               "DPSNXHOST"


/* - Predefined Willingness Prop. Values.  Range: [0, 100] - */

#define XDPSNX_MAX_WILLINGNESS 100
#define XDPSNX_NOT_WILLING     0 /* 0:= agent wishes to "refuses" service to
				    new clients */
#define XDPSNX_MIN_WILLINGNESS 1 /* 1:= agent may delegate a client's request
				    for a connection to another "more willing"
				    agent */

/* ---Predefined License Method Values--- */

#define LICENSE_METHOD_PREFIX "_ADOBE_DPS_NX_LICENSE_LOCK_&_KEY_"
#define LICENSE_METHOD_OPEN   "_ADOBE_DPS_NX_LICENSE_OPEN_SERVICE"
#define OPEN_LICENSE_VERSION 1

#include <stddef.h>
  

/* ---Types--- */

typedef struct {
  int transport;
  int port;
} TransportInfo;


#endif	/* DPSNXPROPS_H */
