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
	    Title  : winscard_msg.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 03/30/01
	    License: Copyright (C) 2001 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This defines some structures and defines to
	             be used over the transport layer.

********************************************************************/

#ifndef __winscard_msg_h__
#define __winscard_msg_h__

#ifdef __cplusplus
extern "C"
{
#endif

	enum pcsc_msg_commands
	{
		SCARD_ESTABLISH_CONTEXT = 0x01,
		SCARD_RELEASE_CONTEXT = 0x02,
		SCARD_LIST_READERS = 0x03,
		SCARD_CONNECT = 0x04,
		SCARD_RECONNECT = 0x05,
		SCARD_DISCONNECT = 0x06,
		SCARD_BEGIN_TRANSACTION = 0x07,
		SCARD_END_TRANSACTION = 0x08,
		SCARD_TRANSMIT = 0x09,
		SCARD_CONTROL = 0x0A,
		SCARD_STATUS = 0x0B,
		SCARD_GET_STATUS_CHANGE = 0x0C,
		SCARD_CANCEL = 0x0D,
		SCARD_CANCEL_TRANSACTION = 0x0E
	};

	typedef struct request_header_struct
	{
		 // Size of the request struct including the header
		unsigned int size;

		// Size of the data blob following the request struct
  		unsigned int additional_data_size;

		unsigned int command;
	} request_header;

	typedef struct reply_header_struct
	{
		 // Size of the reply struct including the header
		unsigned int size;

		// Size of the data blob following the reply struct
  		unsigned int additional_data_size;

		LONG rv;
	} reply_header;


	typedef struct establish_request_struct
	{
		request_header header;
		DWORD dwScope;
	} establish_request;

	typedef struct establish_reply_struct
	{
		reply_header header;
		SCARDCONTEXT phContext;
	} establish_reply;

	typedef struct release_request_struct
	{
		request_header header;
		SCARDCONTEXT hContext;
	} release_request;

	typedef struct release_reply_struct
	{
		reply_header header;
	} release_reply;

	typedef struct connect_request_struct
	{
		request_header header;
		SCARDCONTEXT hContext;
		char szReader[MAX_READERNAME];
		DWORD dwShareMode;
		DWORD dwPreferredProtocols;
	} connect_request;

	typedef struct connect_reply_struct
	{
		reply_header header;
		SCARDHANDLE phCard;
		DWORD pdwActiveProtocol;
	} connect_reply;

	typedef struct reconnect_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
		DWORD dwShareMode;
		DWORD dwPreferredProtocols;
		DWORD dwInitialization;
	} reconnect_request;

	typedef struct reconnect_reply_struct
	{
		reply_header header;
		DWORD pdwActiveProtocol;
	} reconnect_reply;

	typedef struct disconnect_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
		DWORD dwDisposition;
	} disconnect_request;

	typedef struct disconnect_reply_struct
	{
		reply_header header;
	} disconnect_reply;

	typedef struct begin_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
	} begin_request;

	typedef struct begin_reply_struct
	{
		reply_header header;
	} begin_reply;

	typedef struct end_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
		DWORD dwDisposition;
	} end_request;

	typedef struct end_reply_struct
	{
		reply_header header;
	} end_reply;

	typedef struct cancel_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
	} cancel_request;

	typedef struct cancel_reply_struct
	{
		reply_header header;
	} cancel_reply;

	typedef struct status_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
		DWORD cbMaxAtrLen;
	} status_request;

	typedef struct status_reply_struct
	{
		reply_header header;
		char mszReaderNames[MAX_READERNAME];
		DWORD pcchReaderLen;
		DWORD pdwState;
		DWORD pdwProtocol;
		DWORD pcbAtrLen;
		UCHAR pbAtr[MAX_ATR_SIZE];
	} status_reply;

	typedef struct transmit_request_struct
	{
		request_header header;
		SCARDHANDLE hCard;
		SCARD_IO_REQUEST pioSendPci;
		DWORD cbMaxRecvLength;
	} transmit_request;

	typedef struct transmit_reply_struct
	{
		reply_header header;
		SCARD_IO_REQUEST pioRecvPci;
		DWORD cbRecvLength;
	} transmit_reply;


	/* Common functions for server and client. */
	int MSGSendData(int filedes, int blockAmount, const void *data,
		unsigned int dataSize);
	int MSGRecieveData(int filedes, int blockAmount, void *data,
		unsigned int dataSize);

#ifdef __cplusplus
}
#endif

#endif
