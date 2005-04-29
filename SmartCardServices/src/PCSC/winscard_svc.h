/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard_svc.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 03/30/01
	    License: Copyright (C) 2001 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This demarshalls functions over the message
	             queue and keeps track of clients and their
                     handles.

********************************************************************/

#ifndef __winscard_svc_h__
#define __winscard_svc_h__

#include "winscard_msg.h"

#ifdef __cplusplus
extern "C"
{
#endif

	enum pcsc_adm_commands
	{
		CMD_FUNCTION = 0xF1,
		CMD_CLIENT_DIED = 0xF4
	};

	typedef union request_message_union
	{
		request_header header;
		establish_request establish;
		release_request release;
		connect_request connect;
		reconnect_request reconnect;
		disconnect_request disconnect;
		begin_request begin;
		end_request end;
		cancel_request cancel;
		status_request status;
		transmit_request transmit;
	} request_message;

	typedef union reply_message_union
	{
		reply_header header;
		establish_reply establish;
		release_reply release;
		connect_reply connect;
		reconnect_reply reconnect;
		disconnect_reply disconnect;
		begin_reply begin;
		end_reply end;
		cancel_reply cancel;
		status_reply status;
		transmit_reply transmit;
	} reply_message;

	typedef struct request_object_struct
	{
		unsigned int mtype; /* One of enum pcsc_adm_commands */
		unsigned int socket;
		void *additional_data;
		request_message message;
	} request_object;

	typedef struct reply_object_struct
	{
		void *additional_data;
		reply_message message;
	} reply_object;


	/* Server only functions. */
	int MSGServerSetupCommonChannel();
	void MSGServerCleanupCommonChannel(int, char *);
	int MSGServerProcessCommonChannelRequest();
	int MSGServerProcessEvents(request_object *request, int blockAmount);


	LONG MSGFunctionDemarshall(const request_object *request,
		reply_object *reply);
	LONG MSGAddContext(SCARDCONTEXT, DWORD);
	LONG MSGRemoveContext(SCARDCONTEXT, DWORD);
	LONG MSGAddHandle(SCARDCONTEXT, DWORD, SCARDHANDLE);
	LONG MSGRemoveHandle(SCARDCONTEXT, DWORD, SCARDHANDLE);
	LONG MSGCleanupClient(request_object *);

#ifdef __cplusplus
}
#endif

#endif
