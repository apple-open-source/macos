/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * mslp.h : Minimal SLP v2 definitions.
 *
 * Version: 1.11
 * Date:    10/05/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#ifndef _MSLP_
#define _MSLP_

#define _MATH_H_		// squash warnings and dup symbols

#include <CoreFoundation/CoreFoundation.h>

#ifndef _STDARG_H
#include <stdarg.h>
#endif

#ifndef _STDLIB_H_
#include <stdlib.h>
#endif

#ifndef	_STDIO_H_
#include <stdio.h>
#endif

#include <assert.h>

extern EXPORT int GetSLPPort(void);

#ifndef Boolean
typedef unsigned char                   Boolean;
#endif

#define SLP_DEFAULT_SCOPE			"DEFAULT"
#define SLP_DEFAULT_SA_ONLY_SCOPE	"DEFAULT"

#define MAXLINE				4096
#define SLP_PORT            GetSLPPort()

#define BROADCAST           inet_addr("255.255.255.255")
#define SLP_MCAST           inet_addr("239.255.255.253")
#define SLP_VER             2

#define CASE_INSENSITIVE    1
#define CASE_SENSITIVE      0

/*
 * FUNCTION values (Message ids) supported
 */
#define SRVRQST      1
#define SRVRPLY      2
#define SRVREG       3
#define SRVDEREG     4
#define SRVACK       5
#define ATTRRQST     6
#define ATTRRPLY     7
#define DAADVERT     8
#define SRVTYPERQST  9
#define SRVTYPERPLY 10
#define SAADVERT    11
#define PluginInfoReq 0xFF

/*
 * Extension IDs
 */
#define SCOPE_SPONSER_EXTENSION_ID	0x8123
 
/*
 * Error codes to be returned via SLP protocol
 */

// these are errors we return over the wire in response to requests/reg/dereg etc.
typedef enum {
    NO_ERROR					= 0x0000,
	LANGUAGE_NOT_SUPPORTED		= 0x0001,
	PARSE_ERROR					= 0x0002,
	INVALID_REGISTRATION		= 0x0003,
	SCOPE_NOT_SUPPORTED			= 0x0004,
	AUTHENTICATION_UNSUPP		= 0x0005,
	AUTHENTICATION_ABSENT		= 0x0006,
	AUTHENTICATION_FAILED		= 0x0007,
	VER_NOT_SUPPORTED			= 0x0009,
	INTERNAL_ERROR				= 0x0010,
	DA_BUSY_NOW					= 0x0011,
	OPTION_NOT_UNDERSTOOD		= 0x0012,
	INVALID_UPDATE				= 0x0013,
	RQST_NOT_SUPPORTED			= 0x0014,
	REFRESH_REJECTED			= 0x0015
} SLPReturnError;

extern EXPORT SLPReturnError InternalToReturnError( SLPInternalError iErr );

/*
 * flags
 */
#define MCASTFLAG 0x20
#define FRESHFLAG 0x40
#define OVERFLOWFLAG  0x80

/*
 * OFFSETS to fields in the header
 */

#define VER        0
#define FUN        1
#define LEN        2
#define FLAGS      5
#define NEXTOP     7
#define XID       10
#define LANGLEN   12
#define HDRLEN    14

#define MINHDRLEN	HDRLEN+2	// 'en' lang
#define MAX_REPLY_LENGTH 0xFFFFFF	// max size is 3byte length param!
/*
 * These macros assume that pc points to the beginning of a SLPv2 packet.
 */

#define GETBYTE(pc,o)    (0x00FF & *(unsigned char*) &((pc)[(o)])) 
#define GETSHT(pc,o)     ((0xFF00 & GETBYTE((pc),(o)) << 8) + \
			  (0xFF & GETBYTE((pc),(o)+1)) )
#define SETBYTE(pc,i,o)  (pc)[(o)] = (unsigned char) (i)
#define SETSHT(pc,i,o)   { SETBYTE((pc),(0xFF & ((i) >> 8)),(o)); \
                           SETBYTE((pc),(0xFF & (i)),(o)+1); }

#define GETVER(pc)       GETBYTE((pc),VER)
#define GETFUN(pc)       GETBYTE((pc),FUN)
#define GETLEN(pc)       ((0xff0000 & (*(unsigned char*) &((pc)[2]) <<16))+ \
                          (0xff00 & (*(unsigned char*) &((pc)[3]) << 8))+ \
                          (0xff & (*(unsigned char*) &((pc)[4]))))
#define GETFLAGS(pc)     GETBYTE((pc),FLAGS)
#define GETLANGLEN(pc)   GETSHT((pc),LANGLEN)
#define GETNEXTOP(pc)    GETSHT((pc),NEXTOP)
#define GETXID(pc)       GETSHT((pc),XID)

#define GETHEADERLEN(pc) GETLANGLEN((pc))+HDRLEN

#define SETVER(pc,i)     SETBYTE((pc),i,VER)
#define SETFUN(pc,i)     SETBYTE((pc),i,FUN)
#define SETLEN(pc,i)     { SETSHT((pc),((i & 0xffff00) >> 8),LEN); \
                           SETBYTE((pc),(i&0x0000FF),LEN+2); } 
#define SETFLAGS(pc,i)   SETBYTE((pc),i,FLAGS)
#define SETLANG(pc,pcL)  { SETSHT((pc),strlen(pcL),LANGLEN); \
                           memcpy(&(pc)[HDRLEN],(pcL),strlen((pcL))); }
#define SETNEXTOP(pc,i)  SETSHT((pc),i,NEXTOP)
#define SETXID(pc,i)     SETSHT((pc),i,XID)

typedef struct slphdr {

  unsigned char  h_ucVer;
  unsigned char  h_ucFun;
  unsigned long  h_ulLen;
  unsigned short h_usXID;
  unsigned short h_usFlags;
  char *         h_pcLangTag;
  unsigned char  h_usErrCode;
  int            h_iOffset;
} Slphdr;

/*
 * Interface Configureation info
 * From "Unix Network Programming" by Richard Stevens
 */
 
 #include <net/if.h>
 #define IFI_NAME	16					// same as IFNAMSIZ in <net/if.h>
 #define IFI_HADDR	8					// allow for 64 bit EUI-64 in future
 
 struct ifi_info {
    char				ifi_name[IFI_NAME];		// interface name, null terminated
    u_char				ifi_haddr[IFI_HADDR];	// hardware address
    u_short				ifi_hlen;				// #bytes in hardware address: 0, 6, 8
    short				ifi_flags;				// IFF_xxx constants from <net/if.h>
    short				ifi_myflags;			// our own IFI_xxx flags
    struct sockaddr*	ifi_addr;				// primary address
    struct sockaddr*	ifi_brdaddr;			// broadcast address
    struct sockaddr*	ifi_dstaddr;			// destination address
    struct ifi_info*	ifi_next;				// next of these structures
 };
 
#define IFI_ALIAS 1							// ifi_addr is an alias

struct in_pktinfo {
    struct in_addr		ipi_addr;				// destination IPv4 address
    int					ipi_ifindex;			// received interface index
};

char* 				sock_ntop( const struct sockaddr* sa, u_char salen );
const char* 		slp_inet_ntop( int family, const void* addrptr, char* strptr, size_t len );
struct ifi_info*	get_ifi_info(int family, int doalises);
void				free_ifi_info( struct ifi_info* ifihead);

/*
 * CONFIGURATION
 */

#define NUMBUCKETS 100

typedef struct bucket {
  struct bucket *pBucketNext;
  char          *pcKey;
  char          *pcVal;  
} MslpHashbucket;

typedef struct mslphashtable {
  MslpHashbucket ** bucket;
} MslpHashtable;

#define kMaxSizeOfParam	32000	+ 1	// since the scope list is going to be the biggest param and we
#define kMaxScopeListLenForUDP	1200	+ 1	// since the scope list is going to be the biggest param and we
                                    // limit it to 1200 bytes...


  /* Set timeout values so that 3 times them is maximum wait */
    #define WAIT_MSEC           1000
    #define DADISCMSEC          15000

  /* Increase or decrease this value as needed */
    #define RECVMTU             8192
    #define SENDMTU             "1400"

  /* The TTL for multicasting requests with SLP.  */
    #define MCAST_TTL           "255"

  /* The maximum time that a unicast request will wait before it times out */
    #define MAX_UNICAST_WAIT 3000

/* 
 * INTERNAL DEFINITIONS
 */ 

#define LISTINCR             256
#define MIN_REFRESH_DEFAULT  10800
#define kSecsToReregisterBeforeExpiration 300		// five minutes

#define SHARE_FLAGS  (S_IRUSR | S_IWUSR | S_IRGRP | \
                      S_IWGRP | S_IROTH | S_IWOTH)

/*
 * LOG DEFINITIONS
 */

/* the following macro makes mini slp compatible with a SLPv2 test harness */
#define slperror             slp_strerror

typedef enum LogLevel{
  SLP_LOG_DROP			=	0x0001, 
  SLP_LOG_REG			=	0x0002, 
  SLP_LOG_DA			=	0x0004, 
  SLP_LOG_MSG			=	0x0008,
  SLP_LOG_FAIL			=	0x0010, 
  SLP_LOG_ERR			=	0x0020, 
  SLP_LOG_DEBUG			=	0x0040, 
  SLP_LOG_SA			=	0x0080,
  SLP_LOG_RADMIN		=	0x0100, 
  SLP_LOG_EXP			=	0x0200, 
  SLP_LOG_SR			=	0x0400, 
  SLP_LOG_STATE			=	0x0800,
  SLP_LOG_NOTIFICATIONS	=	0x1000,
  SLP_LOG_CONFIG		=	0x2000,
  SLP_LOG_SIGNAL		=	0x4000
} LogLevel;

#define LOG_DEBUG_MESSAGES			0

/*
#if	LOG_DEBUG_MESSAGES
    #define LOG_DEBUG( format, args... ) \
                SLP_LOG(SLP_LOG_DEBUG, format , ## args)
#else
    #define LOG_DEBUG( format, args... )
#endif

#define LOG_SLP_MSG(format, args...) \
        	SLP_LOG( SLP_LOG_MSG, format , ## args )
            
#define LOG_REG(format, args...) \
        	SLP_LOG( SLP_LOG_REG, format , ## args )
            
#define LOG_DA(format, args...) \
        	SLP_LOG( SLP_LOG_DA, format , ## args )
            
#define LOG_DROP(format, args...) \
        	SLP_LOG( SLP_LOG_DROP, format , ## args )
            
#define LOG_RADMIN(format, args...) \
        	SLP_LOG( SLP_LOG_RADMIN, format , ## args )
            
#define LOG_STATE(format, args...) \
        	SLP_LOG( SLP_LOG_STATE, format , ## args )
           
#define LOG_ERR(format, args...) \
        	SLP_LOG( SLP_LOG_ERR, format , ## args )
           
#define LOG_FAIL(format, args...) \
        	SLP_LOG( SLP_LOG_FAIL, format , ## args )
*/           
#ifdef LOG
#undef LOG
#endif

#define LOG(lev,pc)         mslplog(lev,pc,NULL)
#define FAILERR(pc,e)       { SLP_LOG(SLP_LOG_FAIL,"%s: %s",pc,strerror(e)); return e; } 
#define LOG_STD_ERROR_AND_RETURN(lev,pc,e)    { SLP_LOG(lev,"%s: %s",pc,strerror(e)); return e; }
#define LOG_SLP_ERROR_AND_RETURN(lev,pc,e) { SLP_LOG(lev,"%s: %s",pc,slperror(e)); return e; }

/*
 * To reduce the footprint of the implementation and to remove
 * logging, substitute the following definitions for the above
 * ones:
 *
 * #define FAIL(pc)
 * #define FAILERR(pc,e)
 * #define LOG(lev,pc)
 * #define LOG_STD_ERROR_AND_RETURN(lev,pc,e)    return e;
 * #define LOG_SLP_ERROR_AND_RETURN(lev,pc,e) return e;
 *
 */

/*
 *  Callback types, used so that process_reply can callback variously.
 */
#ifdef MAC_OS_X
typedef enum { SLPSRVURL_CALLBACK = 1, SLPREG_REPORT = 2,
	       SLPDAADVERT_CALLBACK = 3, SLPSAADVERT_CALLBACK = 4,
	       SLPATTR_CALLBACK = 5, SLPSRVTYPE_CALLBACK = 6, SLPSAADVERT_ASYNC_CALLBACK = 7
} CBType;
#else
typedef enum { SLPSRVURL_CALLBACK = 1, SLPREG_REPORT = 2,
	       SLPDAADVERT_CALLBACK = 3, SLPSAADVERT_CALLBACK = 4,
	       SLPATTR_CALLBACK = 5, SLPSRVTYPE_CALLBACK = 6
} CBType;
#endif /* MAC_OS_X */

typedef void SLPDAAdvertCallback(
    SLPHandle           hSLP,
    int                 iErrorCode,
    struct sockaddr_in  sin,
    const char         *pcScopeList,
    const char         *pcDAAttrs,
    long                lBootTime,
    void               *pvUser);

/*
 * In every case except for safe_malloc, the return value is the
 * error code, and the last parameter is a pointer to the returned
 * value.  Error codes are consistent:  0 means success, negative
 * numbers are well defined errors in slp.h.
 */

/* --------------------------------------------------------------- mslplib.c */
extern EXPORT SLPInternalError	SLP2APIerr(unsigned short usErr);
extern EXPORT const char *		get_fun_str(int i);

/* ------------------------------------------------------------- mslp_util.c */
extern EXPORT void slp_strcat( char* targetStr, const char* strToAppend );
extern EXPORT char*				safe_malloc(int s, const char *pbuf, int iCpybuf);
extern EXPORT SLPInternalError	add_header(const char *pcLangTag, char *pcSndBuf, int iSendSz, int iFun, int iLen, int *piLen);                    
extern EXPORT SLPInternalError	add_string(char *pcBuf,int iMax,const char *pc, int *piLen);
extern EXPORT SLPInternalError	add_sht(char *pcBuf, int iBufSz, int iVal, int *piLen);
extern EXPORT SLPInternalError	get_string(const char *pcBuf, int iMaxLen, int *piOffset, char **pcString);
extern EXPORT SLPInternalError	get_header(const char *pcSend, const char *pcRcv, int len, Slphdr *pslph, int *piLen);
extern EXPORT SLPInternalError	get_sht(const char *pcBuf, int maxlen, int *piOffset, int *piOut);
extern EXPORT SLPInternalError	get_sin_from_url(const char *pcURL, int iLen, struct sockaddr_in *psin);
extern EXPORT int				set_len_in_header(char *pcBuf, int len);
extern EXPORT char*				get_next_string(const char *pcSeps, const char *pcStringList, int *piIndex, char *pcDelim);
extern EXPORT struct in_addr	get_in_addr_by_name(const char* pcAddr);
extern EXPORT const char*		slperror(SLPInternalError);

extern EXPORT void 				SLP_LOG(LogLevel lev, const char* format, ...);

extern EXPORT void 				mslplog(LogLevel l, const char *pc, const char *pcSysMsg);
extern EXPORT void 				newlog(LogLevel lev, const char* format, va_list ap );
extern EXPORT SLPInternalError	isAttrvalEscapedOK(const char *pc);
extern EXPORT SLPInternalError	isURLEscapedOK(const char *pcVal);
/* ------------------------------------------------------------ mslp_list.c */

/*
 *  These constants are used when calling list_merge.  If CHECK is the
 *  last parameter, duplicates are suppressed.  If NO_CHECK is the last
 *  parameter, elements are simply appended on.
 */
   
#define NO_CHECK 0
#define CHECK    1
    
extern EXPORT int      list_intersection(const char *pcL1, const char *pcL2);
extern EXPORT char*    list_pack(const char *pc);
extern EXPORT void     list_merge(const char *pc, char **ppc, int *piLen,int);
extern EXPORT int      list_subset(const char *pc1, const char *pc2);
extern EXPORT char * list_remove_element(const char *list, const char *element); 

/* ------- system configuration -------- */
extern EXPORT const char*	GetEncodedScopeToRegisterIn( void );
extern EXPORT bool			ServerScopeSponsoringEnabled( void );
extern EXPORT int			SizeOfServerScopeSponsorData( void );
extern EXPORT const char* 	GetServerScopeSponsorData( void );
extern EXPORT void			InitializeSLPSystemConfigurator( CFRunLoopRef runLoopRef = 0 );
extern EXPORT void 			DeleteRegFileIfFirstStartupSinceBoot( void );
extern EXPORT CFStringRef 	CopyCurrentActivePrimaryInterfaceName( void );
extern EXPORT CFStringRef	CopyConfiguredInterfaceToUse( void );

extern EXPORT bool			OnlyUsePreConfiguredDAs( void );

/* ------------------------------------------------------------- mslp_net.c */
extern EXPORT int		readn(SOCKET, void *, size_t);
extern EXPORT int		writen(SOCKET, void *, size_t);
extern EXPORT int		GetOurIPAdrs( struct in_addr* ourIPAddr, const char** pcInterf );
extern EXPORT int		CalculateOurIPAddress( struct in_addr* ourIPAddr, const char** pcInterf );

/* ------------------------------------------------------------ mslp_utf8.c */
extern EXPORT int          utf8_strcmp( int, const char*, int, const char*, int, int*);
extern EXPORT unsigned int utf8_convert(const char*,int*,int,unsigned int*);

/* ------------------------------------------------------------ mslp_disc.c */
extern EXPORT SLPInternalError active_da_discovery(SLPHandle, time_t, SOCKET, int, struct sockaddr_in, const char *pcScopeList, void*, void*, CBType);
extern EXPORT SLPInternalError handle_daadvert_in(const char *, const char *, int, void *, SLPHandle, void *, CBType);

extern EXPORT SLPInternalError StartSLPDALocator( void* daadvert_callback, CFRunLoopRef runLoop, SLPHandle serverState );
extern EXPORT void StopSLPDALocator( void );
extern EXPORT void KickSLPDALocator( void );
extern EXPORT int GlobalDATableCreationCompleted( void );		/* to check to see if our initial DA lookup has finished */

#ifdef EXTRA_MSGS
extern SLPInternalError active_sa_discovery(SLPHandle hSLP, const char *pcHint);
extern SLPInternalError handle_saadvert_in(const char *, const char *, int, void *, SLPHandle, void *, CBType); /* used only by mslplib */
#endif /* EXTRA_MSGS */

/* ----------------------------------------------------------- mslp_prefs.c */

extern EXPORT MslpHashtable * mslp_hash_init();
extern EXPORT const char * mslp_hash_find(MslpHashtable *ph, const char *pcKey);
extern EXPORT void mslp_hash_add(MslpHashtable *ph, const char *pcKey, const char *pcVal);
extern void mslp_hash_read(MslpHashtable *ph, const char *pcFileName);
extern void mslp_hash_write(MslpHashtable *ph, const char *pcFileName);
extern int  mslp_conf_int(MslpHashtable *ph, const char *pcKey, int index, int iDefault);
extern EXPORT void mslp_hash_log_values(MslpHashtable *ph);

#ifdef EXTRA_MSGS
typedef void MslpHashDoFun(const char *pcKey, const char *pcVal, void *pvParam);
EXPORT void mslp_hash_do(MslpHashtable *ph, MslpHashDoFun dofun,void *pv);
EXPORT void mslp_hash_free(MslpHashtable *ph);
#endif /* EXTRA_MSGS */

/* ----------------------------------------------------------- mslp_prefs.c */
extern EXPORT SLPInternalError set_multicast_sender_interf(SOCKET sd);

#ifdef	__cplusplus
//}
#endif




#endif







