/* $Xorg: ICElibint.h,v 1.4 2001/02/09 02:03:26 xorgcvs Exp $ */
/******************************************************************************


Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Author: Ralph Mor, X Consortium
******************************************************************************/
/* $XFree86: xc/lib/ICE/ICElibint.h,v 1.6 2001/12/14 19:53:35 dawes Exp $ */

#ifndef _ICELIBINT_H_
#define _ICELIBINT_H_

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/ICE/ICEproto.h>
#include <X11/ICE/ICEconn.h>
#include <X11/ICE/ICEmsg.h>

#include <stdlib.h>
#include <stddef.h>


/*
 * Vendor & Release
 */

#define IceVendorString  "MIT"
#define IceReleaseString "1.0"


/*
 * Pad to a 64 bit boundary
 */

#define PAD64(_bytes) ((8 - ((unsigned int) (_bytes) % 8)) % 8)

#define PADDED_BYTES64(_bytes) (_bytes + PAD64 (_bytes))


/*
 * Pad to 32 bit boundary
 */

#define PAD32(_bytes) ((4 - ((unsigned int) (_bytes) % 4)) % 4)

#define PADDED_BYTES32(_bytes) (_bytes + PAD32 (_bytes))


/*
 * Number of 8 byte units in _bytes.
 */

#define WORD64COUNT(_bytes) (((unsigned int) ((_bytes) + 7)) >> 3)


/*
 * Number of 4 byte units in _bytes.
 */

#define WORD32COUNT(_bytes) (((unsigned int) ((_bytes) + 3)) >> 2)


/*
 * Given a string, compute the number of bytes for the STRING representation
 */

#define STRING_BYTES(_string) \
    (2 + strlen (_string) + PAD32 (2 + strlen (_string)))


/*
 * Size of ICE input/output buffers
 */

#define ICE_INBUFSIZE 1024

#define ICE_OUTBUFSIZE 1024


/*
 * Maxium number of ICE authentication methods allowed, and maxiumum
 * number of authentication data entries allowed to be set in the
 * IceSetPaAuthData function.
 *
 * We should use linked lists, but this is easier and should suffice.
 */

#define MAX_ICE_AUTH_NAMES 32
#define ICE_MAX_AUTH_DATA_ENTRIES 100


/*
 * ICE listen object
 */

struct _IceListenObj {
    struct _XtransConnInfo 	*trans_conn; /* transport connection object */
    char			*network_id;
    IceHostBasedAuthProc 	host_based_auth_proc;
};


/*
 * Some internal data structures for processing ICE messages.
 */

typedef void (*_IceProcessCoreMsgProc) (
#if NeedFunctionPrototypes
    IceConn 		/* iceConn */,
    int			/* opcode */,
    unsigned long	/* length */,
    Bool		/* swap */,
    IceReplyWaitInfo *  /* replyWait */,
    Bool *		/* replyReadyRet */,
    Bool *		/* connectionClosedRet */
#endif
);

typedef struct {
    int 			major_version;
    int 			minor_version;
    _IceProcessCoreMsgProc	process_core_msg_proc;
} _IceVersion;


/*
 * STORE FOO
 */

#define STORE_CARD8(_pBuf, _val) \
{ \
    *((CARD8 *) _pBuf) = _val; \
    _pBuf += 1; \
}

#ifndef WORD64

#define STORE_CARD16(_pBuf, _val) \
{ \
    *((CARD16 *) _pBuf) = _val; \
    _pBuf += 2; \
}

#define STORE_CARD32(_pBuf, _val) \
{ \
    *((CARD32 *) _pBuf) = _val; \
    _pBuf += 4; \
}

#else /* WORD64 */

#define STORE_CARD16(_pBuf, _val) \
{ \
    struct { \
        int value   :16; \
        int pad     :16; \
    } _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, 2); \
    _pBuf += 2; \
}

#define STORE_CARD32(_pBuf, _val) \
{ \
    struct { \
        int value   :32; \
    } _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, 4); \
    _pBuf += 4; \
}

#endif /* WORD64 */

#define STORE_STRING(_pBuf, _string) \
{ \
    CARD16 _len = strlen (_string); \
    STORE_CARD16 (_pBuf, _len); \
    memcpy (_pBuf, _string, _len); \
    _pBuf += _len; \
    if (PAD32 (2 + _len)) \
        _pBuf += PAD32 (2 + _len); \
}


/*
 * EXTRACT FOO
 */

#define EXTRACT_CARD8(_pBuf, _val) \
{ \
    _val = *((CARD8 *) _pBuf); \
    _pBuf += 1; \
}

#ifndef WORD64

#define EXTRACT_CARD16(_pBuf, _swap, _val) \
{ \
    _val = *((CARD16 *) _pBuf); \
    _pBuf += 2; \
    if (_swap) \
        _val = lswaps (_val); \
}

#define EXTRACT_CARD32(_pBuf, _swap, _val) \
{ \
    _val = *((CARD32 *) _pBuf); \
    _pBuf += 4; \
    if (_swap) \
        _val = lswapl (_val); \
}

#else /* WORD64 */

#define EXTRACT_CARD16(_pBuf, _swap, _val) \
{ \
    _val = *(_pBuf + 0) & 0xff; 	/* 0xff incase _pBuf is signed */ \
    _val <<= 8; \
    _val |= *(_pBuf + 1) & 0xff;\
    _pBuf += 2; \
    if (_swap) \
        _val = lswaps (_val); \
}

#define EXTRACT_CARD32(_pBuf, _swap, _val) \
{ \
    _val = *(_pBuf + 0) & 0xff; 	/* 0xff incase _pBuf is signed */ \
    _val <<= 8; \
    _val |= *(_pBuf + 1) & 0xff;\
    _val <<= 8; \
    _val |= *(_pBuf + 2) & 0xff;\
    _val <<= 8; \
    _val |= *(_pBuf + 3) & 0xff;\
    _pBuf += 4; \
    if (_swap) \
        _val = lswapl (_val); \
}

#endif /* WORD64 */

#define EXTRACT_STRING(_pBuf, _swap, _string) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _string = (char *) malloc (_len + 1); \
    memcpy (_string, _pBuf, _len); \
    _pBuf += _len; \
    _string[_len] = '\0'; \
    if (PAD32 (2 + _len)) \
        _pBuf += PAD32 (2 + _len); \
}

#define EXTRACT_LISTOF_STRING(_pBuf, _swap, _count, _strings) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        EXTRACT_STRING (_pBuf, _swap, _strings[_i]); \
}


#define SKIP_STRING(_pBuf, _swap, _end, _bail) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _pBuf += _len + PAD32(2+_len); \
    if (_pBuf > _end) { \
	_bail; \
    } \
} 

#define SKIP_LISTOF_STRING(_pBuf, _swap, _count, _end, _bail) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        SKIP_STRING (_pBuf, _swap, _end, _bail); \
}



/*
 * Byte swapping
 */

/* byte swap a long literal */
#define lswapl(_val) ((((_val) & 0xff) << 24) |\
		   (((_val) & 0xff00) << 8) |\
		   (((_val) & 0xff0000) >> 8) |\
		   (((_val) >> 24) & 0xff))

/* byte swap a short literal */
#define lswaps(_val) ((((_val) & 0xff) << 8) | (((_val) >> 8) & 0xff))



/*
 * ICE replies (not processed via callbacks because we block)
 */

#define ICE_CONNECTION_REPLY	1
#define ICE_CONNECTION_ERROR	2
#define ICE_PROTOCOL_REPLY	3
#define ICE_PROTOCOL_ERROR	4

typedef struct {
    int		  type;
    int 	  version_index;
    char	  *vendor;
    char          *release;
} _IceConnectionReply;

typedef struct {
    int		  type;
    char	  *error_message;
} _IceConnectionError;

typedef struct {
    int		  type;
    int 	  major_opcode;
    int		  version_index;
    char	  *vendor;
    char	  *release;
} _IceProtocolReply;

typedef struct {
    int		  type;
    char	  *error_message;
} _IceProtocolError;


typedef union {
    int			type;
    _IceConnectionReply	connection_reply;
    _IceConnectionError	connection_error;
    _IceProtocolReply	protocol_reply;
    _IceProtocolError	protocol_error;
} _IceReply;


/*
 * Watch for ICE connection create/destroy.
 */

typedef struct _IceWatchedConnection {
    IceConn				iceConn;
    IcePointer				watch_data;
    struct _IceWatchedConnection	*next;
} _IceWatchedConnection;

typedef struct _IceWatchProc {
    IceWatchProc		watch_proc;
    IcePointer			client_data;
    _IceWatchedConnection	*watched_connections;
    struct _IceWatchProc	*next;
} _IceWatchProc;


/*
 * Locking
 */

#define IceLockConn(_iceConn)
#define IceUnlockConn(_iceConn)


/*
 * Extern declarations
 */

extern IceConn     	_IceConnectionObjs[];
extern char	    	*_IceConnectionStrings[];
extern int     		_IceConnectionCount;

extern _IceProtocol	_IceProtocols[];
extern int         	_IceLastMajorOpcode;

extern int		_IceAuthCount;
extern char		*_IceAuthNames[];
extern IcePoAuthProc	_IcePoAuthProcs[];
extern IcePaAuthProc	_IcePaAuthProcs[];

extern int		_IceVersionCount;
extern _IceVersion	_IceVersions[];

extern _IceWatchProc	*_IceWatchProcs;

extern IceErrorHandler   _IceErrorHandler;
extern IceIOErrorHandler _IceIOErrorHandler;


extern void _IceErrorBadMajor (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* offendingMajor */,
    int			/* offendingMinor */,
    int			/* severity */
#endif
);

extern void _IceErrorNoAuthentication (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* offendingMinor */
#endif
);

extern void _IceErrorNoVersion (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* offendingMinor */
#endif
);

extern void _IceErrorSetupFailed (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* offendingMinor */,
    char *		/* reason */
#endif
);

extern void _IceErrorAuthenticationRejected (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* offendingMinor */,
    char *		/* reason */
#endif
);

extern void _IceErrorAuthenticationFailed (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* offendingMinor */,
    char *		/* reason */
#endif
);

extern void _IceErrorProtocolDuplicate (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    char *		/* protocolName */
#endif
);

extern void _IceErrorMajorOpcodeDuplicate (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* majorOpcode */
#endif
);

extern void _IceErrorUnknownProtocol (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    char *		/* protocolName */
#endif
);

extern void _IceAddOpcodeMapping (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* hisOpcode */,
    int			/* myOpcode */
#endif
);

extern char *_IceGetPeerName (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */
#endif
);

extern void _IceFreeConnection (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */
#endif
);

extern void _IceAddReplyWait (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    IceReplyWaitInfo *	/* replyWait */
#endif
);

extern IceReplyWaitInfo *_IceSearchReplyWaits (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    int			/* majorOpcode */
#endif
);

extern void _IceSetReplyReady (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    IceReplyWaitInfo *	/* replyWait */
#endif
);

extern Bool _IceCheckReplyReady (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */,
    IceReplyWaitInfo *	/* replyWait */
#endif
);

extern void _IceConnectionOpened (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */
#endif
);

extern void _IceConnectionClosed (
#if NeedFunctionPrototypes
    IceConn		/* iceConn */
#endif
);

extern void _IceGetPoAuthData (
#if NeedFunctionPrototypes
    char *		/* protocol_name */,
    char *		/* address */,
    char *		/* auth_name */,
    unsigned short *	/* auth_data_length_ret */,
    char **		/* auth_data_ret */
#endif
);

extern void _IceGetPaAuthData (
#if NeedFunctionPrototypes
    char *		/* protocol_name */,
    char *		/* address */,
    char *		/* auth_name */,
    unsigned short *	/* auth_data_length_ret */,
    char **		/* auth_data_ret */
#endif
);

extern void _IceGetPoValidAuthIndices (
#if NeedFunctionPrototypes
    char *		/* protocol_name */,
    char *		/* address */,
    int			/* num_auth_names */,
    char **		/* auth_names */,
    int	*		/* num_indices_ret */,
    int	*		/* indices_ret */
#endif
);

extern void _IceGetPaValidAuthIndices (
#if NeedFunctionPrototypes
    char *		/* protocol_name */,
    char *		/* address */,
    int			/* num_auth_names */,
    char **		/* auth_names */,
    int	*		/* num_indices_ret */,
    int	*		/* indices_ret */
#endif
);

#endif /* _ICELIBINT_H_ */
