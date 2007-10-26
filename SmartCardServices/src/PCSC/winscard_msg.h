/*
 *  Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  winscard_msg.c
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: winscard_msg.h 2308 2007-01-06 20:26:57Z rousseau $
 */

/**
 * @file
 * @brief This defines some structures and \#defines to be used over
 * the transport layer.
 */

#ifndef __winscard_msg_h__
#define __winscard_msg_h__

#include "pcscexport.h"

/** Major version of the current message protocol */
#define PROTOCOL_VERSION_MAJOR 2
/** Minor version of the current message protocol */
#define PROTOCOL_VERSION_MINOR 2

#define WINSCARD_MSG_HEADER_TAG	0x12345678

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief General structure for client/serve message data exchange.
	 *
	 * It is used in the calls of \c SHMMessageSend and \c SHMMessageReceive.
	 * The field \c data is interpreted according to the values of the fields
	 * \c mtype and \c command. The possible structs the \c data field can
	 * represent are: \c version_struct \c client_struct \c establish_struct
	 * \c release_struct \c connect_struct \c reconnect_struct
	 * \c disconnect_struct \c begin_struct \c end_struct \c cancel_struct
	 * \c status_struct \c transmit_struct \c control_struct \c getset_struct
	 */
	typedef struct rxSharedSegment
	{
		uint32_t headerTag;		/** Always WINSCARD_MSG_HEADER_TAG */
		uint32_t msgSize;		/** size of the whole message being sent/received */
		uint32_t mtype;			/** one of the \c pcsc_adm_commands */
		uint32_t user_id;
		uint32_t group_id;
		uint32_t command;		/** one of the \c pcsc_msg_commands */
		uint32_t dummy;			/* was request_id in pcsc-lite <= 1.2.0 */
		time_t date;
		unsigned char key[PCSCLITE_MSG_KEY_LEN];
		unsigned char data[PCSCLITE_MAX_MESSAGE_SIZE];
	}
	sharedSegmentMsg, *psharedSegmentMsg;

	/**
	 * Command types available to use in the field \c sharedSegmentMsg.mtype.
	 */
	enum pcsc_adm_commands
	{
		CMD_FUNCTION = 0xF1,
		CMD_FAILED = 0xF2,
		CMD_SERVER_DIED = 0xF3,
		CMD_CLIENT_DIED = 0xF4,
		CMD_READER_EVENT = 0xF5,
		CMD_SYN = 0xF6,
		CMD_ACK = 0xF7,
		CMD_VERSION = 0xF8
	};

	/**
	 * @brief Commands available to use in the field \c sharedSegmentMsg.command.
	 */
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
		SCARD_CANCEL_TRANSACTION = 0x0E,
		SCARD_GET_ATTRIB = 0x0F,
		SCARD_SET_ATTRIB = 0x10,
		SCARD_TRANSMIT_EXTENDED = 0x11,
		SCARD_CONTROL_EXTENDED = 0x12
	};

	/**
	 * @brief Information transmitted in \c CMD_VERSION Messages.
	 */
	struct version_struct
	{
		int major;
		int minor;
		LONG rv;
	};
	typedef struct version_struct version_struct;

	struct client_struct
	{
		SCARDCONTEXT hContext;
	};
	typedef struct client_struct client_struct;

	/**
	 * @brief Information contained in \c SCARD_ESTABLISH_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct establish_struct
	{
		DWORD dwScope;
		SCARDCONTEXT phContext;
		LONG rv;
	};
	typedef struct establish_struct establish_struct;

	/**
	 * @brief Information contained in \c SCARD_RELEASE_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct release_struct
	{
		SCARDCONTEXT hContext;
		LONG rv;
	};
	typedef struct release_struct release_struct;

	/**
	 * @brief contained in \c SCARD_CONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct connect_struct
	{
		SCARDCONTEXT hContext;
		char szReader[MAX_READERNAME];
		DWORD dwShareMode;
		DWORD dwPreferredProtocols;
		SCARDHANDLE phCard;
		DWORD pdwActiveProtocol;
		LONG rv;
	};
	typedef struct connect_struct connect_struct;

	/**
	 * @brief contained in \c SCARD_RECONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct reconnect_struct
	{
		SCARDHANDLE hCard;
		DWORD dwShareMode;
		DWORD dwPreferredProtocols;
		DWORD dwInitialization;
		DWORD pdwActiveProtocol;
		LONG rv;
	};
	typedef struct reconnect_struct reconnect_struct;

	/**
	 * @brief contained in \c SCARD_DISCONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct disconnect_struct
	{
		SCARDHANDLE hCard;
		DWORD dwDisposition;
		LONG rv;
	};
	typedef struct disconnect_struct disconnect_struct;

	/**
	 * @brief contained in \c SCARD_BEGIN_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct begin_struct
	{
		SCARDHANDLE hCard;
		LONG rv;
	};
	typedef struct begin_struct begin_struct;

	/**
	 * @brief contained in \c SCARD_END_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct end_struct
	{
		SCARDHANDLE hCard;
		DWORD dwDisposition;
		LONG rv;
	};
	typedef struct end_struct end_struct;

	/**
	 * @brief contained in \c SCARD_CANCEL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct cancel_struct
	{
		SCARDHANDLE hCard;
		LONG rv;
	};
	typedef struct cancel_struct cancel_struct;

	/**
	 * @brief contained in \c SCARD_STATUS Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct status_struct
	{
		SCARDHANDLE hCard;
		char mszReaderNames[MAX_READERNAME];
		DWORD pcchReaderLen;
		DWORD pdwState;
		DWORD pdwProtocol;
		UCHAR pbAtr[MAX_ATR_SIZE];
		DWORD pcbAtrLen;
		LONG rv;
	};
	typedef struct status_struct status_struct;

	/**
	 * @brief contained in \c SCARD_TRANSMIT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct
	{
		SCARDHANDLE hCard;
		SCARD_IO_REQUEST pioSendPci;
		UCHAR pbSendBuffer[MAX_BUFFER_SIZE];
		DWORD cbSendLength;
		SCARD_IO_REQUEST pioRecvPci;
		BYTE pbRecvBuffer[MAX_BUFFER_SIZE];
		DWORD pcbRecvLength;
		LONG rv;
	};
	typedef struct transmit_struct transmit_struct;

	/**
	 * @brief contained in \c SCARD_TRANSMIT_EXTENDED Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct_extended
	{
		SCARDHANDLE hCard;
		SCARD_IO_REQUEST pioSendPci;
		DWORD cbSendLength;
		SCARD_IO_REQUEST pioRecvPci;
		DWORD pcbRecvLength;
		LONG rv;
		size_t size;
		BYTE data[1];
	};
	typedef struct transmit_struct_extended transmit_struct_extended;

	/**
	 * @brief contained in \c SCARD_CONTROL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct control_struct
	{
		SCARDHANDLE hCard;
		DWORD dwControlCode;
		UCHAR pbSendBuffer[MAX_BUFFER_SIZE];
		DWORD cbSendLength;
		UCHAR pbRecvBuffer[MAX_BUFFER_SIZE];
		DWORD cbRecvLength;
		DWORD dwBytesReturned;
		LONG rv;
	};
	typedef struct control_struct control_struct;

	/**
	 * @brief contained in \c SCARD_CONTROL_EXTENDED Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct control_struct_extended
	{
		SCARDHANDLE hCard;
		DWORD dwControlCode;
		DWORD cbSendLength;
		DWORD cbRecvLength;
		DWORD pdwBytesReturned;
		LONG rv;
		size_t size;
		BYTE data[1];
	};
	typedef struct control_struct_extended control_struct_extended;

	/**
	 * @brief contained in \c SCARD_GET_ATTRIB and \c  Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct getset_struct
	{
		SCARDHANDLE hCard;
		DWORD dwAttrId;
		UCHAR pbAttr[MAX_BUFFER_SIZE];
		DWORD cbAttrLen;
		LONG rv;
	};
	typedef struct getset_struct getset_struct;

	/*
	 * Now some function definitions
	 */

	int SHMClientReadMessage(psharedSegmentMsg msgStruct, DWORD dwClientID, size_t dataSize, int blockamount);
	int SHMClientRead(psharedSegmentMsg, DWORD, int);
	int SHMClientSetupSession(PDWORD);
	int SHMClientCloseSession(DWORD);
	int SHMInitializeCommonSegment(void);
	int SHMProcessEventsContext(PDWORD, psharedSegmentMsg, int);
	int SHMProcessEventsServer(PDWORD, int);
	int SHMMessageSend(void *buffer, size_t buffer_size, int filedes,
		int blockAmount);
	int SHMMessageReceive(void *buffer, size_t buffer_size,
		int filedes, int blockAmount);
	int WrapSHMWrite(unsigned int command, DWORD dwClientID, unsigned int dataSize,
		unsigned int blockAmount, void *data);
	void SHMCleanupSharedSegment(int, const char *);

	void SHSharedSegmentMsgToNetworkOrder(psharedSegmentMsg msg);
	void SHSharedSegmentMsgToHostOrder(psharedSegmentMsg msg);
	size_t SHMCalculateMessageSize(size_t dataSize);
	int SHMCommunicationTimeout();

	// Fix up byte ordering
	INTERNAL void htonlControlStructExtended(control_struct_extended *cs);
	INTERNAL void ntohlControlStructExtended(control_struct_extended *cs);
	INTERNAL void htonlTransmitStructExtended(transmit_struct_extended *ts);
	INTERNAL void ntohlTransmitStructExtended(transmit_struct_extended *ts);
	INTERNAL void htonlSCARD_IO_REQUEST(SCARD_IO_REQUEST *req);
	INTERNAL void ntohlSCARD_IO_REQUEST(SCARD_IO_REQUEST *req);
	INTERNAL void htonlEstablishStruct(establish_struct *es);
	INTERNAL void ntohlEstablishStruct(establish_struct *es);
	INTERNAL void htonlTransmitStruct(transmit_struct *ts);
	INTERNAL void ntohlTransmitStruct(transmit_struct *ts);
	INTERNAL void htonlReleaseStruct(release_struct *rs);
	INTERNAL void ntohlReleaseStruct(release_struct *rs);
	INTERNAL void htonlConnectStruct(connect_struct *Cs);
	INTERNAL void ntohlConnectStruct(connect_struct *cs);
	INTERNAL void htonlReconnectStruct(reconnect_struct *rc);
	INTERNAL void ntohlReconnectStruct(reconnect_struct *rc);
	INTERNAL void htonlDisconnectStruct(disconnect_struct *dc);
	INTERNAL void ntohlDisconnectStruct(disconnect_struct *dc);
	INTERNAL void htonlBeginStruct(begin_struct *bs);
	INTERNAL void ntohlBeginStruct(begin_struct *bs);
	INTERNAL void htonlCancelStruct(cancel_struct *cs);
	INTERNAL void ntohlCancelStruct(cancel_struct *cs);
	INTERNAL void htonlEndStruct(end_struct *es);
	INTERNAL void ntohlEndStruct(end_struct *es);
	INTERNAL void htonlStatusStruct(status_struct *ss);
	INTERNAL void ntohlStatusStruct(status_struct *ss);
	INTERNAL void htonlControlStruct(control_struct *cs);
	INTERNAL void ntohlControlStruct(control_struct *cs);
	INTERNAL void htonlGetSetStruct(getset_struct *gs);
	INTERNAL void ntohlGetSetStruct(getset_struct *gs);
	INTERNAL void htonlVersionStruct(version_struct *vs);
	INTERNAL void ntohlVersionStruct(version_struct *vs);

#ifdef __cplusplus
}
#endif

#endif
