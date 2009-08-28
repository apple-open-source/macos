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
 *  winscard_msg.h
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
 * $Id: winscard_msg.h 2900 2008-04-22 13:12:50Z rousseau $
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
#define PROTOCOL_VERSION_MAJOR 3
/** Minor version of the current message protocol */
#define PROTOCOL_VERSION_MINOR 0

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
		uint64_t date;
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
		CMD_VERSION = 0xF8	/**< version of the IPC */
	};

	/**
	 * @brief Commands available to use in the field \c sharedSegmentMsg.command.
	 */
	enum pcsc_msg_commands
	{
		SCARD_ESTABLISH_CONTEXT = 0x01,	/**< used by SCardEstablishContext() */
		SCARD_RELEASE_CONTEXT = 0x02,	/**< used by SCardReleaseContext() */
		SCARD_LIST_READERS = 0x03,		/**< used by SCardListReaders() */
		SCARD_CONNECT = 0x04,			/**< used by SCardConnect() */
		SCARD_RECONNECT = 0x05,			/**< used by SCardReconnect() */
		SCARD_DISCONNECT = 0x06,		/**< used by SCardDisconnect() */
		SCARD_BEGIN_TRANSACTION = 0x07,	/**< used by SCardBeginTransaction() */
		SCARD_END_TRANSACTION = 0x08,	/**< used by SCardEndTransaction() */
		SCARD_TRANSMIT = 0x09,			/**< used by SCardTransmit() */
		SCARD_CONTROL = 0x0A,			/**< used by SCardControl() */
		SCARD_STATUS = 0x0B,			/**< used by SCardStatus() */
		SCARD_GET_STATUS_CHANGE = 0x0C,	/**< used by SCardGetStatusChange() */
		SCARD_CANCEL = 0x0D,			/**< used by SCardCancel() */
		SCARD_CANCEL_TRANSACTION = 0x0E,
		SCARD_GET_ATTRIB = 0x0F,		/**< used by SCardGetAttrib() */
		SCARD_SET_ATTRIB = 0x10,		/**< used by SCardSetAttrib() */
		SCARD_TRANSMIT_EXTENDED = 0x11,	/**< used by SCardTransmit() */
		SCARD_CONTROL_EXTENDED = 0x12	/**< used by SCardControl() */
	};

	/**
	 * @brief Information transmitted in \ref CMD_VERSION Messages.
	 */
	struct version_struct
	{
		int32_t major;	/**< IPC major \ref PROTOCOL_VERSION_MAJOR */
		int32_t minor;	/**< IPC minor \ref PROTOCOL_VERSION_MINOR */
		uint32_t rv;
	};
	typedef struct version_struct version_struct;

	struct client_struct
	{
		uint32_t hContext;
	};
	typedef struct client_struct client_struct;

	/**
	 * @brief Information contained in \ref SCARD_ESTABLISH_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct establish_struct
	{
		uint32_t dwScope;
		uint32_t phContext;
		uint32_t rv;
	};
	typedef struct establish_struct establish_struct;

	/**
	 * @brief Information contained in \ref SCARD_RELEASE_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct release_struct
	{
		uint32_t hContext;
		uint32_t rv;
	};
	typedef struct release_struct release_struct;

	/**
	 * @brief contained in \ref SCARD_CONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct connect_struct
	{
		uint32_t hContext;
		char szReader[MAX_READERNAME];
		uint32_t dwShareMode;
		uint32_t dwPreferredProtocols;
		int32_t phCard;
		uint32_t pdwActiveProtocol;
		uint32_t rv;
	};
	typedef struct connect_struct connect_struct;

	/**
	 * @brief contained in \ref SCARD_RECONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct reconnect_struct
	{
		int32_t hCard;
		uint32_t dwShareMode;
		uint32_t dwPreferredProtocols;
		uint32_t dwInitialization;
		uint32_t pdwActiveProtocol;
		uint32_t rv;
	};
	typedef struct reconnect_struct reconnect_struct;

	/**
	 * @brief contained in \ref SCARD_DISCONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct disconnect_struct
	{
		int32_t hCard;
		uint32_t dwDisposition;
		uint32_t rv;
	};
	typedef struct disconnect_struct disconnect_struct;

	/**
	 * @brief contained in \ref SCARD_BEGIN_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct begin_struct
	{
		int32_t hCard;
		uint32_t rv;
	};
	typedef struct begin_struct begin_struct;

	/**
	 * @brief contained in \ref SCARD_END_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct end_struct
	{
		int32_t hCard;
		uint32_t dwDisposition;
		uint32_t rv;
	};
	typedef struct end_struct end_struct;

	/**
	 * @brief contained in \ref SCARD_CANCEL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct cancel_struct
	{
		int32_t hCard;
		uint32_t rv;
	};
	typedef struct cancel_struct cancel_struct;

	/**
	 * @brief contained in \ref SCARD_STATUS Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct status_struct
	{
		int32_t hCard;
		char mszReaderNames[MAX_READERNAME];
		uint32_t pcchReaderLen;
		uint32_t pdwState;
		uint32_t pdwProtocol;
		uint8_t pbAtr[MAX_ATR_SIZE];
		uint32_t pcbAtrLen;
		uint32_t rv;
	};
	typedef struct status_struct status_struct;

	/**
	 * @brief contained in \ref SCARD_TRANSMIT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct
	{
		int32_t hCard;
		uint32_t pioSendPciProtocol;
		uint32_t pioSendPciLength;
		uint8_t pbSendBuffer[MAX_BUFFER_SIZE];
		uint32_t cbSendLength;
		uint32_t pioRecvPciProtocol;
		uint32_t pioRecvPciLength;
		uint8_t pbRecvBuffer[MAX_BUFFER_SIZE];
		uint32_t pcbRecvLength;
		uint32_t rv;
	};
	typedef struct transmit_struct transmit_struct;

	/**
	 * @brief contained in \ref SCARD_TRANSMIT_EXTENDED Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct_extended
	{
		int32_t hCard;
		uint32_t pioSendPciProtocol;
		uint32_t pioSendPciLength;
		uint32_t cbSendLength;
		uint32_t pioRecvPciProtocol;
		uint32_t pioRecvPciLength;
		uint32_t pcbRecvLength;
		uint32_t rv;
		uint64_t size;
		uint8_t data[1];
	};
	typedef struct transmit_struct_extended transmit_struct_extended;

	/**
	 * @brief contained in \ref SCARD_CONTROL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct control_struct
	{
		int32_t hCard;
		uint32_t dwControlCode;
		uint8_t pbSendBuffer[MAX_BUFFER_SIZE];
		uint32_t cbSendLength;
		uint8_t pbRecvBuffer[MAX_BUFFER_SIZE];
		uint32_t cbRecvLength;
		uint32_t dwBytesReturned;
		uint32_t rv;
	};
	typedef struct control_struct control_struct;

	/**
	 * @brief contained in \ref SCARD_CONTROL_EXTENDED Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct control_struct_extended
	{
		int32_t hCard;
		uint32_t dwControlCode;
		uint32_t cbSendLength;
		uint32_t cbRecvLength;
		uint32_t pdwBytesReturned;
		uint32_t rv;
		uint64_t size;
		uint8_t data[1];
	};
	typedef struct control_struct_extended control_struct_extended;

	/**
	 * @brief contained in \ref SCARD_GET_ATTRIB and \c  Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct getset_struct
	{
		int32_t hCard;
		uint32_t dwAttrId;
		uint8_t pbAttr[MAX_BUFFER_SIZE];
		uint32_t cbAttrLen;
		uint32_t rv;
	};
	typedef struct getset_struct getset_struct;

	/*
	 * Now some function definitions
	 */

	int32_t SHMClientReadMessage(psharedSegmentMsg msgStruct, uint32_t dwClientID, size_t dataSize, int blockamount);
	
	int32_t SHMClientRead(psharedSegmentMsg, uint32_t, int32_t);
	int32_t SHMClientSetupSession(uint32_t *);
	int32_t SHMClientCloseSession(uint32_t);
	int32_t SHMInitializeCommonSegment(void);
	int32_t SHMProcessEventsContext(uint32_t, psharedSegmentMsg, int32_t);
	int32_t SHMProcessEventsServer(uint32_t *, int32_t);
	int32_t SHMMessageSend(void *buffer, uint64_t buffer_size, int32_t filedes,
		int32_t blockAmount);
	int32_t SHMMessageReceive(void *buffer, uint64_t buffer_size,
		int32_t filedes, int32_t blockAmount);
	int32_t WrapSHMWrite(uint32_t command, uint32_t dwClientID, uint64_t dataSize,
		uint32_t blockAmount, void *data);
	void SHMCleanupSharedSegment(int32_t, const char *);

	void SHSharedSegmentMsgToNetworkOrder(psharedSegmentMsg msg);
	void SHSharedSegmentMsgToHostOrder(psharedSegmentMsg msg);
	size_t SHMCalculateMessageSize(size_t dataSize);
	int32_t SHMCommunicationTimeout();

	// Fix up byte ordering
	INTERNAL void htonlControlStructExtended(control_struct_extended *cs);
	INTERNAL void ntohlControlStructExtended(control_struct_extended *cs);
	INTERNAL void htonlTransmitStructExtended(transmit_struct_extended *ts);
	INTERNAL void ntohlTransmitStructExtended(transmit_struct_extended *ts);
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
