/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *      Copyright (c) 1997 Apple Computer, Inc.
 *
 *      The information contained herein is subject to change without
 *      notice and  should not be  construed as a commitment by Apple
 *      Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *      for any errors that may appear.
 *
 *      Confidential and Proprietary to Apple Computer, Inc.
 *
 */

/* at_proto.h --  Prototype Definitions for the AppleTalk API

   See the "AppleTalk Programming Interface" document for a full
   explanation of these functions and their parameters.

   Required header files are as follows:

   #include <netat/appletalk.h>
*/

#ifndef _AT_PROTO_H_
#define _AT_PROTO_H_

/* Appletalk Stack status Function. */

enum {
	  RUNNING
	, NOTLOADED
	, LOADED
	, OTHERERROR
};

int checkATStack();

/* Datagram Delivery Protocol (DDP) Functions */

   /* The functions ddp_open(), ddp_close(), atproto_open() and 
      adspproto_open() have been replaced by standard BSD socket 
      functions, e.g. socket(AF_APPLETALK, SOCK_RAW, [ddp type]);

      See AppleTalk/nbp_send.c for an example of how these functions 
      are used,
   */

/* Routing Table Maintenance Protocol (RTMP) Function */

   /* The rtmp_netinfo() function has been replaced by the 
      AIOCGETIFCFG ioctl.

      See AppleTalk/nbp_send.c for an example of how this ioctl is used.
   */
     
/* AppleTalk Transaction Protocol (ATP) Functions */

int atp_open(at_socket *socket);
int atp_close(int fd);
int atp_sendreq(int fd,
		at_inet_t *dest,
		char *buf,
		int len, 
		int userdata, 
		int xo, 
		int xo_relt,
		u_short *tid,
		at_resp_t *resp,
		at_retry_t *retry,
		int nowait);
int atp_getreq(int fd,
	       at_inet_t *src,
	       char *buf,
	       int *len, 
	       int *userdata, 
	       int *xo,
	       u_short *tid,
	       u_char *bitmap,
	       int nowait);
int atp_sendrsp(int fd,
		at_inet_t *dest,
		int xo,
		u_short tid,
		at_resp_t *resp);
int atp_getresp(int fd,
		u_short *tid,
		at_resp_t *resp);
int atp_look(int fd);
int atp_abort(int fd,
	      at_inet_t *dest,
	      u_short tid);

/* Name Binding Protocol (NBP) Functions */

int nbp_parse_entity(at_entity_t *entity,
		     char *str);
int nbp_make_entity(at_entity_t *entity, 
		    char *obj, 
		    char *type, 
		    char *zone);
int nbp_confirm(at_entity_t *entity,
		at_inet_t *dest,
		at_retry_t *retry);
int nbp_lookup(at_entity_t *entity,
	       at_nbptuple_t *buf,
	       int max,
	       at_retry_t *retry);
int nbp_register(at_entity_t *entity, 
		 int fd, 
		 at_retry_t *retry);
int nbp_remove(at_entity_t *entity, 
	       int fd);	     	/* fd is not currently used */

int nbp_reg_lookup(at_entity_t *entity,
		   at_retry_t *retry);
/*
  Used to make sure an NBP entity does not exist before it is registered.
  Returns 1 	if the entity already exists, 
  	  0 	if the entry does not exist
	 -1	for an error; e.g.no local zones exist
  Does the right thing in multihoming mode, namely if the zone is
  "*" (the default), it does the lookup in the default zone for
  each interface.

*/




/* Printer Access Protocol (PAP) Functions */

int pap_open(at_nbptuple_t *tuple);
int pap_read(int fd,
	     u_char *data,
	     int len);
int pap_read_ignore(int fd);
char *pap_status(at_nbptuple_t *tuple);
int pap_write(int fd,
	      char *data,
	      int len,
	      int eof,
	      int flush);
int pap_close(int fd);

/* AppleTalk Data Stream Protocol (ADSP) Functions: */

int ADSPaccept(int fd, 
	       void *name, 
	       int *namelen);
int ADSPbind(int fd, 
	     void *name, 
	     int namelen);
int ADSPclose(int fd);
int ADSPconnect(int fd, 
		void *name, 
		int namelen);
int ADSPfwdreset(int fd);
int ADSPgetpeername(int fd, 
		    void *name, 
		    int *namelen);
int ADSPgetsockname(int fd, 
		    void *name, 
		    int *namelen);
int ADSPgetsockopt(int fd, 
		   int level, 
		   int optname, 
		   char *optval, 
		   int *optlen);
int ADSPlisten(int fd, 
	       int backlog);
int ADSPrecv(int fd, 
	     char *buf, 
	     int len, 
	     int flags);
int ADSPsend(int fd,
	     char *buf,
	     int len,
	     int flags);
int ADSPsetsockopt(int fd,
		   int level,
		   int optname,
		   char *optval,
		   int optlen);
int ADSPsocket(int fd,
	       int type,
	       int protocol);
int ASYNCread(int fd,
	      char *buf,
	      int len);
int ASYNCread_complete(int fd,
		       char *buf,
		       int len);

/* AppleTalk Session Protocol (ASP) Functions */

int SPAttention(int SessRefNum,
		unsigned short AttentionCode,
		int *SPError,
		int NoWait);
int SPCloseSession(int SessRefNum,
		   int *SPError);
int SPCmdReply(int SessRefNum,
	       unsigned short ReqRefNum,
	       int CmdResult,
	       char *CmdReplyData,
	       int CmdReplyDataSize,
	       int *SPError);
void SPConfigure(unsigned short TickleInterval,
		 unsigned short SessionTimer,
		 at_retry_t *Retry);
void SPGetParms(int *MaxCmdSize,
		int *QuantumSize,
		int SessRefNum);
int SPGetProtoFamily(int SessRefNum,
		      int *ProtoFamily,
		      int *SPError);
int SPGetRemEntity(int SessRefNum,
		   void *SessRemEntityIdentifier,
		   int *SPError);
int SPGetReply(int SessRefNum,
	       char *ReplyBuffer,
	       int ReplyBufferSize,
	       int *CmdResult,
	       int *ActRcvdReplyLen,
	       int *SPError);
int SPGetRequest(int SessRefNum,
		 char *ReqBuffer,
		 int ReqBufferSize,
		 unsigned short *ReqRefNum,
		 int *ReqType,
		 int *ActRcvdReqLen,
		 int *SPError);
int SPGetSession(int SLSRefNum,
		 int *SessRefNum,
		 int *SPError);
int SPInit(at_inet_t *SLSEntityIdentifier,
	   char *ServiceStatusBlock,
	   int ServiceStatusBlockSize,
	   int *SLSRefNum,
	   int *SPError);
int SPLook(int SessRefNum,
	   int *SPError);
int SPNewStatus(int SLSRefNum,
		char *ServiceStatusBlock,
		int ServiceStatusBlockSize,
		int *SPError);
int SPRegister(at_entity_t *SLSEntity,
	       at_retry_t *Retry,
	       at_inet_t *SLSEntityIdentifier,
	       int *SPError);
/*
 * the following API is added to fix bug 2285307;  It replaces SPRegister 
 * which now only behaves as asp over appletalk.
 */
int SPRegisterWithTCPPossiblity(at_entity_t *SLSEntity,
           at_retry_t *Retry,
           at_inet_t *SLSEntityIdentifier,
           int *SPError);
int SPRemove(at_entity_t *SLSEntity,
	     at_inet_t *SLSEntityIdentifier,
	     int *SPError);
/* *** Why do we need to be able to set the pid from the ASP API? *** */
int SPSetPid(int SessRefNum,
	     int SessPid,
	     int *SPError);
int SPWrtContinue(int SessRefNum,
		  unsigned short ReqRefNum,
		  char *Buff,
		  int BuffSize,
		  int *ActLenRcvd,
		  int *SPError,
		  int NoWait);
int SPWrtReply(int SessRefNum,
	       unsigned short ReqRefNum,
	       int CmdResult,
	       char *CmdReplyData,
	       int CmdReplyDataSize,
	       int *SPError);

/* Zone Information Protocol (ZIP) Functions */

#define ZIP_FIRST_ZONE 1
#define ZIP_NO_MORE_ZONES 0
#define ZIP_DEF_INTERFACE NULL

/* zip_getzonelist() will return 0 on success, and -1 on failure. */

int zip_getmyzone(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	at_nvestr_t *zone
);

/* zip_getzonelist() will return the zone count on success, 
   and -1 on failure. */

int zip_getzonelist(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETZONELIST request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
);

/* zip_getlocalzones() will return the zone count on success, 
   and -1 on failure. */

int zip_getlocalzones(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETLOCALZONES request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
);

/* These functions are used to read/write defaultss in persistent storage,
   for now /etc/appletalk.nvram.[interface name, e.g. en0].
*/

/* at_getdefaultzone() returns
      0 on success
     -1 on error
*/
int at_getdefaultzone(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     at_nvestr_t *zone
          /* The return value for the default zone, from persistent 
             storage */
);

/* at_setdefaultzone() returns
      0 on success
     -1 on error
*/
int at_setdefaultzone(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     at_nvestr_t *zone
          /* The value of the default zone, to be set in persistent 
             storage */
);

/* at_getdefaultaddr() returns
      0 on success
     -1 on error
*/
int at_getdefaultaddr(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     struct at_addr *init_address
          /* The return value for the address hint, from persistent 
             storage */
);

/* at_setdefaultaddr() returns
      0 on success
     -1 on error
*/
int at_setdefaultaddr(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     struct at_addr *init_address
          /* The return value for the address hint, from persistent 
             storage */
);

/* Save the current configuration in persistent storage.

   at_savecfgdefaults() returns
      0 on success
     -1 on error
*/
int at_savecfgdefaults(
     int fd,
	/* An AppleTalk socket, if one is open, otherwise 0 */  
     char *ifName
	/* If ifName is a null pointer the name of the default
	   interface will be used. */
);

/* 
   ***
   The following routines are scheduled be replaced/discontinued soon:
   ***
*/

int at_send_to_dev(int fd, int cmd, char *dp, int *length);
	/* Used to send an old-style (pre-BSD) IOC to the AppleTalk stack. */

int ddp_config(int fd, ddp_addr_t *addr);
	/* Used to provide functionality similar to BSD getsockname().
	   Will be replaced with a sockopt as soon as the ATP and the ADSP
	   protocols have been socketized */
#endif _AT_PROTO_H_
