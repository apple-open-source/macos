/*
 * DPSCAPproto.h -- CAP requests constants and alignment values,
 *  		  analgous to Xproto.h
 * 		 
 * (c) Copyright 1991-1994 Adobe Systems Incorporated.
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


#ifndef _DPSCAPproto_h
#define _DPSCAPproto_h

/* === DEFINITIONS === */

#define DPSCAPPROTOVERSION	3
#define DPSCAPPROTOVERSION_2	2

#define CSDPSPORT		6016 /* add agent instance number */
#define DPS_NX_SERV_NAME	"dpsnx" /* name to look up in /etc/services */

/* Request Opcodes */

#define DPSCAPOPCODEBASE	126
#define DPSXOPCODEBASE		125

#define X_CAPFlushAgent		1
#define X_CAPNotify		2
#define X_CAPSetArg		3

/* === REQUESTS === */

typedef struct _CAPConnSetup {
    BYTE byteorder;		/* #x42 MSB, #x6C LSB */
    BYTE dpscapVersion;		/* proto version of connecting client */
    CARD16 flags B16;		/* functional hint flags */
    CARD32 libraryversion B32;	/* as for XPSInit */
    CARD16 authProtoNameLength B16;	/* in bytes */
    CARD16 authProtoDataLength B16;	/* in bytes */
    CARD16 displayStringLength B16;	/* in bytes */
    CARD16 nodeStringLength B16;	/* in bytes */
    CARD16 transportStringLength B16;	/* in bytes */
    CARD16 display B16;		/* Display number */
    CARD16 screen B16;		/* Screen number */
    CARD16 reserved B16;
    CARD32 clientWindow B32;	/* window for ClientMessage */
} xCAPConnSetupReq;
#define sz_xCAPConnSetupReq	28

typedef struct {
    BYTE success;
    BYTE reasonLength;
    CARD16 additionalLength B16;
} xCAPConnReplyPrefix;
#define sz_xCAPConnReplyPrefix	4

typedef struct _CAPConnFailed {
    BYTE success;		/* failed = 0, success = 1 */
    BYTE reasonLength;		/* in bytes if failed, ignore if success */
    CARD16 additionalLength B16;/* quadbytes ADDITIONAL length */
    CARD32 serverVersion B32;	/* as for XPSInit */
    CARD8 dpscapVersion;	/* proto version of agent */
    CARD8 pad;
    CARD16 reserved B16;
} xCAPConnFailed;
#define sz_xCAPConnFailed	12

typedef struct _CAPConnSuccess {
    BYTE success;		/* failed = 0, success = 1 */
    BYTE reasonLength;		/* in bytes if failed, ignore if success */
    CARD16 additionalLength B16;/* quadbytes ADDITIONAL length */
    CARD32 serverVersion B32;	/* as for XPSInit */
    CARD8 dpscapVersion;	/* proto version of agent */
    CARD8 reserved;
    CARD16 flagsUsed B16;	/* mask of functional hint flags used */
    CARD32 preferredNumberFormat B32;	/* as for XPSInit */
    CARD32 floatingNameLength B32;	/* as for XPSInit */
    CARD32 agentWindow B32;     /* client sends messages to this window */
} xCAPConnSuccess;
#define sz_xCAPConnSuccess	24

typedef struct _CAPFlushAgent {
    CARD8 reqType;		/* always DPSCAPOPCODEBASE */
    CARD8 type;			/* always X_CAPFlushAgent */
    CARD16 length B16;		/* quadbyte length of request */
    CARD32 cxid B32;		/* context XID */
} xCAPFlushAgentReq;
#define sz_xCAPFlushAgentReq	8

typedef struct _CAPNotify {
    CARD8 reqType;		/* always DPSCAPOPCODEBASE */
    CARD8 type;			/* always X_CAPNotify */
    CARD16 length B16;		/* quadbyte length of request */
    CARD32 cxid B32;		/* context XID */
    CARD32 notification B32;	/* notify code */
    CARD32 data B32;		/* data word */
    CARD32 extra B32;		/* extra word */
} xCAPNotifyReq;
#define sz_xCAPNotifyReq	20

typedef struct _CAPSetArg {
    CARD8 reqType;		/* always DPSCAPOPCODEBASE */
    CARD8 type;			/* always X_CAPNotify */
    CARD16 length B16;		/* quadbyte length of request */
    CARD32 arg B32;		/* argument type */
    CARD32 val B32;		/* value */
} xCAPSetArgReq;
#define sz_xCAPSetArgReq	12

/* === ERRORS === */

typedef struct _DPSCAPError {
    BYTE type;			/* always 0 */
    BYTE errorCode;		/* always 255 */
    CARD16 sequenceNumber B16;	/* the nth request from this client */
    CARD8 subLength;		/* how much of 21 bytes are used */
    CARD8 unused;
    CARD16 reserved B16;
    CARD16 minorOpcode B16;
    CARD8 majorOpcode;		/* always 0 */
    BYTE subData1;
    CARD32 subData2 B32;
    CARD32 subData3 B32;
    CARD32 subData4 B32;
    CARD32 subData5 B32;
    CARD32 subData6 B32;
    CARD32 subData7 B32;
} xDPSCAPError;

/* === EVENTS === */

/* Events sent from agent to client via XSendEvent */

#define DPSCAP_OUTPUT_OVERHEAD		4
#define DPSCAP_BYTESPEROUTPUTEVENT	(20 - DPSCAP_OUTPUT_OVERHEAD)
#define DPSCAP_DATA_LEN			(DPSCAP_BYTESPEROUTPUTEVENT-1)

typedef struct {
    CARD32 cxid;
    CARD8 data[DPSCAP_BYTESPEROUTPUTEVENT];
} DPSCAPOutputEvent;

typedef struct {
    BYTE status;
    BYTE unused;
    CARD16 sequenceNumber;
    CARD32 cxid;
} DPSCAPStatusEvent;

#endif /* _DPSCAPproto_h */
