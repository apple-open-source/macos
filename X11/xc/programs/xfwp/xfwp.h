/* $Xorg: xfwp.h,v 1.6 2001/02/09 02:05:46 xorgcvs Exp $ */

/*
Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.
*/
/* $XFree86: xc/programs/xfwp/xfwp.h,v 1.10 2001/12/14 20:01:44 dawes Exp $ */

#ifndef _XFWP_H
#define _XFWP_H

#include <X11/Xos.h> /* struct timeval */

#define FALSE		0
#define TRUE		1

#ifndef min
#define	min(a,b)		((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define	max(a,b)		((a) > (b) ? (a) : (b))
#endif

/*
 * Default connection array sizes
 */
#define	MAX_PM_CONNS			10
#define	MAX_SERVERS			100

#define	RWBUFFER_SIZE			2048

enum CLIENT_CONN_STATE {
    CLIENT_WAITING, 
    SERVER_WAITING, 
    SERVER_REPLY,
    CONNECTION_READY
};

enum PM_CONN_STATE {
    START, 
    WAIT_SERVER_INFO, 
    SENT_PORT_INFO, 
    PM_EXCHANGE_DONE
};

enum CONFIG_CHECK {
    FAILURE, 
    SUCCESS
};

enum CONFIG_TYPE {
    PM, 
    REM_CLIENT
};

enum LISTEN_STATE {
    AVAILABLE, 
    IN_USE
};

enum SERVICE_ID_TYPES {
    CLIENT, 
    PMGR, 
    FINDPROXY
};

enum LOG_EVENTS {
  CLIENT_ACCEPT,          /* event 0:  client connection granted */
  CLIENT_REJECT_CONFIG,   /* event 1:  client conn rejected by config file */
  CLIENT_REJECT_SERVER    /* event 2:  client conn rejected by server query */
};

#ifdef notyet
typedef void fp1();
typedef Bool fp2();
#endif

struct ICE_setup_info
{
        
 int       opcode;
 int       versionCount;
 IcePaVersionRec PMVersions[1];
#ifdef notyet
 void (*fp1) ();
 Bool (*fp2) ();
#endif
};

struct client_conn_buf
{
  char 			readbuf[RWBUFFER_SIZE];
  char 			writebuf[RWBUFFER_SIZE];
  int 			rbytes;
  int 			wbytes;
  int 			conn_to;
  int 			wclose;
  int    		state;  
  int			time_to_close;
  int			creation_time;
  int			fd;
  char *		source;
  char * 	        destination;
};

struct pm_conn_buf
{
  char 			readbuf[RWBUFFER_SIZE];
  int 			rbytes;
  int    		state;  
  int			fd;
  IceConn  		ice_conn;
  int			creation_time;
  int			time_to_close;
} ;

struct config
{
  int 			num_client_conns;
  int 			num_pm_conns;
  int 			num_servers;
  int			num_pm_listen_ports;
  int			idle_timeout;
  int 			pm_data_timeout;
  int 			client_listen_timeout;
  int 			client_data_timeout;
  struct timeval	select_timeout;
  char *		pm_listen_port;
  char *   		config_file_path;
  char * 		log_file_path;
  int			lines_allocated;
  int			rule_count;
  struct config_line **  config_file_data;
  int			log_level;
};

struct server_list
{
  char * 		x_server_hostport;
  char *		listen_port_string;
  int			client_listen_fd;
  int 			server_fd;
  int			done_accept;
  int			creation_time;
  int			time_to_close;
};	

struct clientDataStruct
{
  int *			  nfds;
  fd_set *		  rinit;
  fd_set *		  winit;
  int			  major_opcode;
  struct config *	  config_info;
};

struct config_line
{
  char * 		permit_deny;
  char * 		source_hostname;
  unsigned int		source_host;
  char * 		source_netmask;
  unsigned int  	source_net;
  char * 		dest_hostname;
  unsigned int 	 	dest_host;
  char * 		dest_netmask;
  unsigned int	 	dest_net;
#if defined(__cplusplus) || defined(c_plusplus)
  char * 		c_operator;
#else
  char * 		operator;
#endif
  char * 		service;
  int	 		service_id;
};


/*
 * Global variables
 */
extern struct clientDataStruct          global_data;    /* for ICE callbacks */

extern struct pm_conn_buf 		** pm_conn_array;
extern struct server_list 		** server_array;
extern struct client_conn_buf 		** client_conn_array;

extern char **SitePolicies;
extern int SitePolicyCount;
extern int SitePolicyPermit;

/*
 * Handy ICE message parsing macros
 */

/*
 * Pad to a 64 bit boundary
 */

#define PAD64(_bytes) ((8 - ((unsigned int) (_bytes) % 8)) % 8)

#define PADDED_BYTES64(_bytes) (_bytes + PAD64 (_bytes))


/*
 * Number of 8 byte units in _bytes.
 */

#define WORD64COUNT(_bytes) (((unsigned int) ((_bytes) + 7)) >> 3)


/*
 * Compute the number of bytes for a STRING representation
 */

#define STRING_BYTES(_str) (2 + (_str ? strlen (_str) : 0) + \
		     PAD64 (2 + (_str ? strlen (_str) : 0)))



#define SKIP_STRING(_pBuf, _swap) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _pBuf += _len; \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}

/*
 * STORE macros
 */

#define STORE_CARD16(_pBuf, _val) \
{ \
    *((CARD16 *) _pBuf) = _val; \
    _pBuf += 2; \
}

#define STORE_STRING(_pBuf, _string) \
{ \
    int _len = _string ? strlen (_string) : 0; \
    STORE_CARD16 (_pBuf, _len); \
    if (_len) { \
        memcpy (_pBuf, _string, _len); \
        _pBuf += _len; \
    } \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}


/*
 * EXTRACT macros
 */

#define EXTRACT_CARD16(_pBuf, _swap, _val) \
{ \
    _val = *((CARD16 *) _pBuf); \
    _pBuf += 2; \
    if (_swap) \
        _val = lswaps (_val); \
}

#define EXTRACT_STRING(_pBuf, _swap, _string) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _string = (char *) malloc (_len + 1); \
    memcpy (_string, _pBuf, _len); \
    _string[_len] = '\0'; \
    _pBuf += _len; \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
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


#define CHECK_AT_LEAST_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _severity) \
    if ((((_actual_len) - SIZEOF (iceMsg)) >> 3) > _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       return; \
    }


#define CHECK_COMPLETE_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _pStart, _severity) \
    if (((PADDED_BYTES64((_actual_len)) - SIZEOF (iceMsg)) >> 3) \
        != _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       IceDisposeCompleteMessage (iceConn, _pStart); \
       return; \
    }

#endif /* _XFWP_H */
