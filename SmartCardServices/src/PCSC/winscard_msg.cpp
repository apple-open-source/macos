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
 *  Ludoic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: winscard_msg.c 2377 2007-02-05 13:13:56Z rousseau $
 */

/**
 * @file
 * @brief This is responsible for client/server communication.
 *
 * A file based socket (\c commonSocket) is used to send/receive only messages
 * among clients and server.\n
 * The messages' data are passed throw a memory mapped file: \c sharedSegmentMsg.
 */

#include "config.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#include "wintypes.h"
#include "pcsclite.h"
#include "pcscexport.h"
#include "winscard.h"
#include "debug.h"
#include "winscard_msg.h"
#include "sys_generic.h"

#include <security_utilities/debugging.h>

/**
 * @brief Wrapper for the SHMMessageReceive() function.
 *
 * Called by clients to read the server responses.
 *
 * @param[out] msgStruct Message read.
 * @param[in] dwClientID Client socket handle.
 * @param[in] blockamount Timeout in milliseconds.
 *
 * @return Same error codes as SHMMessageReceive().
 */
INTERNAL int SHMClientRead(psharedSegmentMsg msgStruct, DWORD dwClientID, int blockamount)
{
	int rv = SHMMessageReceive(msgStruct, sizeof(*msgStruct), dwClientID, blockamount);
	SHSharedSegmentMsgToHostOrder(msgStruct);
	return rv;
}

/**
 * @brief Wrapper for the SHMMessageReceive() function.
 *
 * Called by clients to read the server responses. This reads the exact number of bytes expected for the struct
 *
 * @param[out] msgStruct Message read.
 * @param[in] dwClientID Client socket handle.
 * @param[in] dataSize Size of the data at msgStruct->data
 * @param[in] blockamount Timeout in milliseconds.
 *
 * @return Same error codes as SHMMessageReceive().
 */
INTERNAL int SHMClientReadMessage(psharedSegmentMsg msgStruct, DWORD dwClientID, size_t dataSize, int blockamount)
{
	// Read the basic header first so we know the size of the rest
	// The special case of "dataSize == 0" means that we should deduce the size of the
	// data from the header
	size_t headerSize = sizeof(sharedSegmentMsg) - sizeof(msgStruct->data);
	Log2(PCSC_LOG_DEBUG, "SHMClientReadMessage: Issuing read for %d bytes (header)", headerSize);
	secdebug("pcscd", "SHMClientReadMessage: Issuing read for %ld bytes (header)", headerSize);
	int rv = SHMMessageReceive(msgStruct, headerSize, dwClientID, blockamount);
	Log3(rv?PCSC_LOG_CRITICAL:PCSC_LOG_DEBUG, "SHMClientReadMessage: read message header error: 0x%08X [0x%08X]", rv, rv);
	secdebug("pcscd", "SHMClientReadMessage: read message header error: 0x%08X [0x%08X]", rv, rv);
	if (rv)
		return rv;
	SHSharedSegmentMsgToHostOrder(msgStruct);
	// Integrity check
	if (msgStruct->headerTag != WINSCARD_MSG_HEADER_TAG)
	{
		Log3(PCSC_LOG_CRITICAL, "Error: read message header tag of: 0x%08X for possible command 0x%08X", 
			msgStruct->headerTag, msgStruct->command);
		secdebug("pcscd", "Error: read message header tag of: 0x%08X for possible command 0x%08X", 
			msgStruct->headerTag, msgStruct->command);
		return SCARD_F_INTERNAL_ERROR;
	}
	if (dataSize == 0)
		dataSize = msgStruct->msgSize - headerSize;		// message size includes header
	else
	if (msgStruct->msgSize != (headerSize + dataSize))
	{
		Log2(PCSC_LOG_CRITICAL, "Error: create on client socket: %s", strerror(errno));
		secdebug("pcscd", "Error: create on client socket: %s", strerror(errno));
		return SCARD_F_INTERNAL_ERROR;
	}
	Log2(PCSC_LOG_DEBUG, "SHMClientReadMessage: Issuing read for %d bytes", dataSize);
	secdebug("pcscd", "SHMClientReadMessage: Issuing read for %ld bytes", dataSize);
	if (blockamount == 0)
		blockamount = PCSCLITE_SERVER_ATTEMPTS;
	rv = SHMMessageReceive(msgStruct->data, dataSize, dwClientID, blockamount);
	Log3(rv?PCSC_LOG_CRITICAL:PCSC_LOG_DEBUG, "SHMClientReadMessage: read message body error: 0x%08X [0x%08X]", rv, rv);
	secdebug("pcscd", "SHMClientReadMessage: read message body error: 0x%08X [0x%08X]", rv, rv);
	return rv;
}

/**
 * @brief Prepares a communication channel for the client to talk to the server.
 *
 * This is called by the application to create a socket for local IPC with the
 * server. The socket is associated to the file \c PCSCLITE_CSOCK_NAME.
 *
 * @param[out] pdwClientID Client Connection ID.
 *
 * @retval 0 Success.
 * @retval -1 Can not create the socket.
 * @retval -1 The socket can not open a connection.
 * @retval -1 Can not set the socket to non-blocking.
 */
INTERNAL int SHMClientSetupSession(PDWORD pdwClientID)
{
	struct sockaddr_un svc_addr;
	int one;
	int ret;

	ret = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}
	*pdwClientID = ret;

	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(svc_addr.sun_path));

	if (connect(*pdwClientID, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: connect to client socket: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		return -1;
	}

	one = 1;
	if (ioctl(*pdwClientID, FIONBIO, &one) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: cannot set socket nonblocking: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		return -1;
	}

	return 0;
}

/**
 * @brief Closes the socket used by the client to communicate with the server.
 *
 * @param[in] dwClientID Client socket handle to be closed.
 *
 * @retval 0 Success.
 */
INTERNAL int SHMClientCloseSession(DWORD dwClientID)
{
	SYS_CloseFile(dwClientID);
	return 0;
}

/**
 * @brief CalculateMessageSize
 *
 * @param[in] dataSize Size of the additional data to send in the message.
 *
 * @retval total message size.
 */
INTERNAL size_t SHMCalculateMessageSize(size_t dataSize)
{
	// PCSCLITE_MAX_MESSAGE_SIZE == sizeof(sharedSegmentMsg.data)
	return sizeof(sharedSegmentMsg) - PCSCLITE_MAX_MESSAGE_SIZE + dataSize;;
}


/**
 * @brief Sends a menssage from client to server or vice-versa.
 *
 * Writes the message in the shared file \c filedes.
 *
 * @param[in] buffer_void Message to be sent.
 * @param[in] buffer_size Size of the message to send
 * @param[in] filedes Socket handle.
 * @param[in] blockAmount Timeout in milliseconds.
 *
 * @retval 0 Success
 * @retval -1 Timeout.
 * @retval -1 Socket is closed.
 * @retval -1 A signal was received.
 */
INTERNAL int SHMMessageSend(void *buffer_void, size_t buffer_size,
	int filedes, int blockAmount)
{
	char *buffer = (char *)buffer_void;

	/*
	 * default is success
	 */
	int retval = 0;
	/*
	 * record the time when we started
	 */
	time_t start = time(0);
	/*
	 * how many bytes remains to be written
	 */
	size_t remaining = buffer_size;

	LogXxd(PCSC_LOG_DEBUG, "==> SHMMessageSend:\n", (const unsigned char *)buffer, buffer_size);

	/*
	 * repeat until all data is written
	 */
	while (remaining > 0)
	{
		fd_set write_fd;
		struct timeval timeout;
		int selret;

		FD_ZERO(&write_fd);
		FD_SET(filedes, &write_fd);

		timeout.tv_usec = 0;
		if ((timeout.tv_sec = start + blockAmount - time(0)) < 0)
		{
			/*
			 * we already timed out
			 */
			Log1(PCSC_LOG_ERROR, "SHMMessageReceive: we already timed out");
			retval = -1;
			break;
		}

		selret = select(filedes + 1, NULL, &write_fd, NULL, &timeout);

		/*
		 * try to write only when the file descriptor is writable
		 */
		if (selret > 0)
		{
			int written;

			if (!FD_ISSET(filedes, &write_fd))
			{
				/*
				 * very strange situation. it should be an assert really
				 */
				Log1(PCSC_LOG_ERROR, "SHMMessageReceive: very strange situation: !FD_ISSET");
				retval = -1;
				break;
			}
			written = write(filedes, buffer, remaining);

			if (written > 0)
			{
				/*
				 * we wrote something
				 */
				buffer += written;
				remaining -= written;
			} else if (written == 0)
			{
				/*
				 * peer closed the socket
				 */
				Log1(PCSC_LOG_ERROR, "SHMMessageReceive: peer closed the socket");
				retval = -1;
				break;
			} else
			{
				/*
				 * we ignore the signals and socket full situations, all
				 * other errors are fatal
				 */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/*
			 * timeout
			 */
			Log1(PCSC_LOG_ERROR, "SHMMessageReceive: selret == 0 [timeout]");
			retval = -1;
			break;
		} else
		{
			/*
			 * ignore signals
			 */
			if (errno != EINTR)
			{
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	if (remaining > 0)
		Log3(PCSC_LOG_ERROR, "failure to write all bytes, remaining: %d, err: ", remaining, strerror(errno));
		
	return retval;
}

/**
 * @brief Called by the Client to get the reponse from the server or vice-versa.
 *
 * Reads the message from the file \c filedes.
 *
 * @param[out] buffer_void Message read.
 * @param[in] buffer_size Size to read
 * @param[in] filedes Socket handle.
 * @param[in] blockAmount Timeout in milliseconds.
 *
 * @retval 0 Success.
 * @retval -1 Timeout.
 * @retval -1 Socket is closed.
 * @retval -1 A signal was received.
 */
INTERNAL int SHMMessageReceive(void *buffer_void, size_t buffer_size,
	int filedes, int blockAmount)
{
	char *buffer = (char *)buffer_void;

	/*
	 * default is success
	 */
	int retval = 0;
	/*
	 * record the time when we started
	 */
	time_t start = time(0);
	/*
	 * how many bytes we must read
	 */
	size_t remaining = buffer_size;

	/*
	 * repeat until we get the whole message
	 */
	while (remaining > 0)
	{
		fd_set read_fd;
		struct timeval timeout;
		int selret;

		FD_ZERO(&read_fd);
		FD_SET(filedes, &read_fd);

		timeout.tv_usec = 0;
		if ((timeout.tv_sec = start + blockAmount - time(0)) < 0)
		{
			/*
			 * we already timed out
			 */
			Log1(PCSC_LOG_ERROR, "SHMMessageReceive: we already timed out");
			retval = -1;
			break;
		}

		selret = select(filedes + 1, &read_fd, NULL, NULL, &timeout);

		/*
		 * try to read only when socket is readable
		 */
		if (selret > 0)
		{
			int readed;

			if (!FD_ISSET(filedes, &read_fd))
			{
				/*
				 * very strange situation. it should be an assert really
				 */
				Log1(PCSC_LOG_ERROR, "SHMMessageReceive: very strange situation: !FD_ISSET");
				retval = -1;
				break;
			}
			readed = read(filedes, buffer, remaining);

			if (readed > 0)
			{
				/*
				 * we got something
				 */
				buffer += readed;
				remaining -= readed;
			} else if (readed == 0)
			{
				/*
				 * peer closed the socket
				 */
				Log1(PCSC_LOG_ERROR, "SHMMessageReceive: peer closed the socket");
				retval = -1;
				break;
			} else
			{
				/*
				 * we ignore the signals and empty socket situations, all
				 * other errors are fatal
				 */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/*
			 * timeout
			 */
			Log1(PCSC_LOG_ERROR, "SHMMessageReceive: selret == 0 [timeout]");
			retval = -1;
			break;
		} else
		{
			/*
			 * we ignore signals, all other errors are fatal
			 */
			if (errno != EINTR)
			{
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	size_t bytesRead = (buffer_size - remaining);
	Log3(PCSC_LOG_DEBUG, "SHMMessageReceive errno: 0x%08X: %s", errno, errno?strerror(errno):"no error");
	Log3(retval?PCSC_LOG_ERROR:PCSC_LOG_DEBUG, "SHMMessageReceive retval: 0x%08X, bytes read: %d", retval, bytesRead);
	LogXxd(PCSC_LOG_DEBUG, "<== SHMMessageReceive:\n", (const unsigned char *)buffer_void, bytesRead);
	return retval;
}

/**
 * @brief Wrapper for the SHMMessageSend() function.
 *
 * Called by clients to send messages to the server.
 * The parameters \p command and \p data are set in the \c sharedSegmentMsg
 * struct in order to be sent.
 *
 * @param[in] command Command to be sent.
 * @param[in] dwClientID Client socket handle.
 * @param[in] size Size of the message (\p data).
 * @param[in] blockAmount Timeout to the operation in ms.
 * @param[in] data_void Data to be sent.
 *
 * @return Same error codes as SHMMessageSend().
 */
INTERNAL int WrapSHMWrite(unsigned int command, DWORD dwClientID,
	unsigned int dataSize, unsigned int blockAmount, void *data_void)
{
	char *data = (char *)data_void;

	sharedSegmentMsg msgStruct;
	int ret;

	/*
	 * Set the appropriate packet parameters
	 */

	memset(&msgStruct, 0, sizeof(msgStruct));
	msgStruct.headerTag = WINSCARD_MSG_HEADER_TAG;
	msgStruct.msgSize = sizeof(sharedSegmentMsg) - sizeof(msgStruct.data) + dataSize;
	msgStruct.mtype = (command == CMD_VERSION)?CMD_VERSION:CMD_FUNCTION;
	msgStruct.user_id = SYS_GetUID();
	msgStruct.group_id = SYS_GetGID();
	msgStruct.command = command;
	msgStruct.date = time(NULL);

	if ((SCARD_TRANSMIT_EXTENDED == command)
		|| (SCARD_CONTROL_EXTENDED == command))
	{
		/* first block */
		size_t sizeToSend = (msgStruct.msgSize <= PCSCLITE_MAX_MESSAGE_SIZE)?msgStruct.msgSize:PCSCLITE_MAX_MESSAGE_SIZE;
		size_t sizeRemaining = (msgStruct.msgSize <= PCSCLITE_MAX_MESSAGE_SIZE)?0:
			(msgStruct.msgSize - PCSCLITE_MAX_MESSAGE_SIZE);
		memcpy(msgStruct.data, data, sizeToSend);
		SHSharedSegmentMsgToNetworkOrder(&msgStruct);
		ret = SHMMessageSend(&msgStruct, sizeToSend, dwClientID,
			blockAmount);
		if (ret)
			return ret;

		// Warning: this code only works for sizes of 2 blocks or less
		if (sizeRemaining > PCSCLITE_MAX_MESSAGE_SIZE)
		{
			Log2(PCSC_LOG_ERROR, "WrapSHMWrite: cannot send message of size %d", msgStruct.msgSize);
			return -1;
		}
		
		/* do not send an empty second block */
		if (sizeRemaining > 0)
		{
			/* second block */
			ret = SHMMessageSend(data+PCSCLITE_MAX_MESSAGE_SIZE,
				sizeRemaining, dwClientID, blockAmount);
			if (ret)
				return ret;
		}
	}
	else
	if (msgStruct.msgSize > PCSCLITE_MAX_MESSAGE_SIZE)
	{
		Log3(PCSC_LOG_ERROR, "WrapSHMWrite: cannot send message of size %d with this command: %d", msgStruct.msgSize, command);
		return -1;
	}
	else
	{
		size_t savedSize = msgStruct.msgSize;
		
		memcpy(msgStruct.data, data, dataSize);
		SHSharedSegmentMsgToNetworkOrder(&msgStruct);
		ret = SHMMessageSend(&msgStruct, savedSize, dwClientID, blockAmount);
	}
	return ret;
}

/**
 * @brief Closes the communications channel used by the server to talk to the
 * clients.
 *
 * The socket used is closed and the file it is bound to is removed.
 *
 * @param[in] sockValue Socket to be closed.
 * @param[in] pcFilePath File used by the socket.
 */
INTERNAL void SHMCleanupSharedSegment(int sockValue, const char *pcFilePath)
{
	SYS_CloseFile(sockValue);
	SYS_Unlink((char *)pcFilePath);
}

#pragma mark -------------------- Byte ordering functions --------------------

/**
 * @brief Convert fields in the psharedSegmentMsg struct to network byte order for sending
 *
 * Call this before each call to SHMMessageSend. Note: the data fields are not processed
 * and need to be done individually. Also have to look for WrapSHMWrite.
 *
 * @param[in/out] msgStruct Message read.
 *
 */
INTERNAL void SHSharedSegmentMsgToNetworkOrder(psharedSegmentMsg msg)
{
	if (msg)
	{
		msg->headerTag = htonl(msg->headerTag);
		msg->msgSize = htonl(msg->msgSize);
		msg->mtype = htonl(msg->mtype);
		msg->user_id = htonl(msg->user_id);
		msg->group_id = htonl(msg->group_id);
		msg->command = htonl(msg->command);
		msg->dummy = htonl(msg->dummy);
		msg->date = htonl(msg->date);
	}
}

/**
 * @brief Convert fields in the psharedSegmentMsg struct to host byte order on receive
 *
 * Call this after each call to SHMMessageReceive. Note: the data fields are not processed
 * and need to be done individually, e.g. in MSGFunctionDemarshall
 *
 * @param[in/out] msgStruct Message read.
 *
 */
INTERNAL void SHSharedSegmentMsgToHostOrder(psharedSegmentMsg msg)
{
	if (msg)
	{
		msg->headerTag = ntohl(msg->headerTag);
		msg->msgSize = ntohl(msg->msgSize);
		msg->mtype = ntohl(msg->mtype);
		msg->user_id = ntohl(msg->user_id);
		msg->group_id = ntohl(msg->group_id);
		msg->command = ntohl(msg->command);
		msg->dummy = ntohl(msg->dummy);
		msg->date = ntohl(msg->date);
	}
}

INTERNAL void htonlControlStructExtended(control_struct_extended *cs)
{
	if (cs)
	{
		cs->hCard = htonl(cs->hCard);
		cs->dwControlCode = htonl(cs->dwControlCode);
		cs->cbSendLength = htonl(cs->cbSendLength);
		cs->cbRecvLength = htonl(cs->cbRecvLength);
		cs->size = htonl(cs->size);
		cs->rv = htonl(cs->rv);			// so we don't forget about it
	}
}

INTERNAL void ntohlControlStructExtended(control_struct_extended *cs)
{
	if (cs)
	{
		cs->hCard = ntohl(cs->hCard);
		cs->dwControlCode = ntohl(cs->dwControlCode);
		cs->cbSendLength = ntohl(cs->cbSendLength);
		cs->cbRecvLength = ntohl(cs->cbRecvLength);
		cs->size = ntohl(cs->size);
		cs->rv = ntohl(cs->rv);
	}
}

INTERNAL void htonlTransmitStruct(transmit_struct *ts)
{
	if (ts)
	{
		ts->hCard = htonl(ts->hCard);
		htonlSCARD_IO_REQUEST(&ts->pioSendPci);
		ts->cbSendLength = htonl(ts->cbSendLength);
		htonlSCARD_IO_REQUEST(&ts->pioRecvPci);
		ts->pcbRecvLength = htonl(ts->pcbRecvLength);
		ts->rv = htonl(ts->rv);			// so we don't forget about it
	}
}

INTERNAL void ntohlTransmitStruct(transmit_struct *ts)
{
	if (ts)
	{
		ts->hCard = ntohl(ts->hCard);
		ntohlSCARD_IO_REQUEST(&ts->pioSendPci);
		ts->cbSendLength = ntohl(ts->cbSendLength);
		ntohlSCARD_IO_REQUEST(&ts->pioRecvPci);
		ts->pcbRecvLength = ntohl(ts->pcbRecvLength);
		ts->rv = ntohl(ts->rv);
	}
}

INTERNAL void htonlTransmitStructExtended(transmit_struct_extended *ts)
{
	if (ts)
	{
		ts->hCard = htonl(ts->hCard);
		htonlSCARD_IO_REQUEST(&ts->pioSendPci);
		ts->cbSendLength = htonl(ts->cbSendLength);
		htonlSCARD_IO_REQUEST(&ts->pioRecvPci);
		ts->pcbRecvLength = htonl(ts->pcbRecvLength);
		ts->size = htonl(ts->size);
		ts->rv = htonl(ts->rv);			// so we don't forget about it
	}
}

INTERNAL void ntohlTransmitStructExtended(transmit_struct_extended *ts)
{
	if (ts)
	{
		ts->hCard = ntohl(ts->hCard);
		ntohlSCARD_IO_REQUEST(&ts->pioSendPci);
		ts->cbSendLength = ntohl(ts->cbSendLength);
		ntohlSCARD_IO_REQUEST(&ts->pioRecvPci);
		ts->pcbRecvLength = ntohl(ts->pcbRecvLength);
		ts->size = ntohl(ts->size);
		ts->rv = ntohl(ts->rv);
	}
}

INTERNAL void htonlSCARD_IO_REQUEST(SCARD_IO_REQUEST *req)
{
	if (req)
	{
		req->dwProtocol = htonl(req->dwProtocol);
		req->cbPciLength = htonl(req->cbPciLength);
	}
}

INTERNAL void ntohlSCARD_IO_REQUEST(SCARD_IO_REQUEST *req)
{
	if (req)
	{
		req->dwProtocol = ntohl(req->dwProtocol);
		req->cbPciLength = ntohl(req->cbPciLength);
	}
}

INTERNAL void htonlEstablishStruct(establish_struct *es)
{
	if (es)
	{
		es->dwScope = htonl(es->dwScope);
		es->phContext = htonl(es->phContext);
		es->rv = htonl(es->rv);
	}
}

INTERNAL void ntohlEstablishStruct(establish_struct *es)
{
	if (es)
	{
		es->dwScope = ntohl(es->dwScope);
		es->phContext = ntohl(es->phContext);
		es->rv = ntohl(es->rv);
	}
}

INTERNAL void htonlReleaseStruct(release_struct *rs)
{
	if (rs)
	{
		rs->hContext = htonl(rs->hContext);
		rs->rv = htonl(rs->rv);
	}
}

INTERNAL void ntohlReleaseStruct(release_struct *rs)
{
	if (rs)
	{
		rs->hContext = ntohl(rs->hContext);
		rs->rv = ntohl(rs->rv);
	}
}

INTERNAL void htonlConnectStruct(connect_struct *cs)
{
	if (cs)
	{
		cs->hContext = htonl(cs->hContext);
		cs->dwShareMode = htonl(cs->dwShareMode);
		cs->dwPreferredProtocols = htonl(cs->dwPreferredProtocols);
		cs->phCard = htonl(cs->phCard);
		cs->pdwActiveProtocol = htonl(cs->pdwActiveProtocol);
		cs->rv = htonl(cs->rv);
	}
}

INTERNAL void ntohlConnectStruct(connect_struct *cs)
{
	if (cs)
	{
		cs->hContext = ntohl(cs->hContext);
		cs->dwShareMode = ntohl(cs->dwShareMode);
		cs->dwPreferredProtocols = ntohl(cs->dwPreferredProtocols);
		cs->phCard = ntohl(cs->phCard);
		cs->pdwActiveProtocol = ntohl(cs->pdwActiveProtocol);
		cs->rv = ntohl(cs->rv);
	}
}

INTERNAL void htonlReconnectStruct(reconnect_struct *rc)
{
	if (rc)
	{
		rc->hCard = htonl(rc->hCard);
		rc->dwShareMode = htonl(rc->dwShareMode);
		rc->dwPreferredProtocols = htonl(rc->dwPreferredProtocols);
		rc->dwInitialization = htonl(rc->dwInitialization);
		rc->pdwActiveProtocol = htonl(rc->pdwActiveProtocol);
		rc->rv = htonl(rc->rv);
	}
}

INTERNAL void ntohlReconnectStruct(reconnect_struct *rc)
{
	if (rc)
	{
		rc->hCard = ntohl(rc->hCard);
		rc->dwShareMode = ntohl(rc->dwShareMode);
		rc->dwPreferredProtocols = ntohl(rc->dwPreferredProtocols);
		rc->dwInitialization = ntohl(rc->dwInitialization);
		rc->pdwActiveProtocol = ntohl(rc->pdwActiveProtocol);
		rc->rv = ntohl(rc->rv);
	}
}

INTERNAL void htonlDisconnectStruct(disconnect_struct *dc)
{
	if (dc)
	{
		dc->hCard = htonl(dc->hCard);
		dc->dwDisposition = htonl(dc->dwDisposition);
		dc->rv = htonl(dc->rv);
	}
}

INTERNAL void ntohlDisconnectStruct(disconnect_struct *dc)
{
	if (dc)
	{
		dc->hCard = ntohl(dc->hCard);
		dc->dwDisposition = ntohl(dc->dwDisposition);
		dc->rv = ntohl(dc->rv);
	}
}

INTERNAL void htonlBeginStruct(begin_struct *bs)
{
	if (bs)
	{
		bs->hCard = htonl(bs->hCard);
		bs->rv = htonl(bs->rv);
	}
}

INTERNAL void ntohlBeginStruct(begin_struct *bs)
{
	if (bs)
	{
		bs->hCard = ntohl(bs->hCard);
		bs->rv = ntohl(bs->rv);
	}
}

INTERNAL void htonlCancelStruct(cancel_struct *cs)
{
	if (cs)
	{
		cs->hCard = htonl(cs->hCard);
		cs->rv = htonl(cs->rv);
	}
}

INTERNAL void ntohlCancelStruct(cancel_struct *cs)
{
	if (cs)
	{
		cs->hCard = ntohl(cs->hCard);
		cs->rv = ntohl(cs->rv);
	}
}

INTERNAL void htonlEndStruct(end_struct *es)
{
	if (es)
	{
		es->hCard = htonl(es->hCard);
		es->dwDisposition = htonl(es->dwDisposition);
		es->rv = htonl(es->rv);
	}
}

INTERNAL void ntohlEndStruct(end_struct *es)
{
	if (es)
	{
		es->hCard = ntohl(es->hCard);
		es->dwDisposition = ntohl(es->dwDisposition);
		es->rv = ntohl(es->rv);
	}
}

INTERNAL void htonlStatusStruct(status_struct *ss)
{
	if (ss)
	{
		ss->hCard = htonl(ss->hCard);
		ss->pcchReaderLen = htonl(ss->pcchReaderLen);
		ss->pdwState = htonl(ss->pdwState);
		ss->pdwProtocol = htonl(ss->pdwProtocol);
		ss->pcbAtrLen = htonl(ss->pcbAtrLen);
		ss->rv = htonl(ss->rv);
	}
}

INTERNAL void ntohlStatusStruct(status_struct *ss)
{
	if (ss)
	{
		ss->hCard = ntohl(ss->hCard);
		ss->pcchReaderLen = ntohl(ss->pcchReaderLen);
		ss->pdwState = ntohl(ss->pdwState);
		ss->pdwProtocol = ntohl(ss->pdwProtocol);
		ss->pcbAtrLen = ntohl(ss->pcbAtrLen);
		ss->rv = ntohl(ss->rv);
	}
}

INTERNAL void htonlControlStruct(control_struct *cs)
{
	if (cs)
	{
		cs->hCard = htonl(cs->hCard);
		cs->dwControlCode = htonl(cs->dwControlCode);
		cs->cbSendLength = htonl(cs->cbSendLength);
		cs->cbRecvLength = htonl(cs->cbRecvLength);
		cs->dwBytesReturned = htonl(cs->dwBytesReturned);
		cs->rv = htonl(cs->rv);
	}
}

INTERNAL void ntohlControlStruct(control_struct *cs)
{
	if (cs)
	{
		cs->hCard = ntohl(cs->hCard);
		cs->dwControlCode = ntohl(cs->dwControlCode);
		cs->cbSendLength = ntohl(cs->cbSendLength);
		cs->cbRecvLength = ntohl(cs->cbRecvLength);
		cs->dwBytesReturned = ntohl(cs->dwBytesReturned);
		cs->rv = ntohl(cs->rv);
	}
}

INTERNAL void htonlGetSetStruct(getset_struct *gs)
{
	if (gs)
	{
		gs->hCard = htonl(gs->hCard);
		gs->dwAttrId = htonl(gs->dwAttrId);
		gs->cbAttrLen = htonl(gs->cbAttrLen);
		gs->rv = htonl(gs->rv);
	}
}

INTERNAL void ntohlGetSetStruct(getset_struct *gs)
{
	if (gs)
	{
		gs->hCard = ntohl(gs->hCard);
		gs->dwAttrId = ntohl(gs->dwAttrId);
		gs->cbAttrLen = ntohl(gs->cbAttrLen);
		gs->rv = ntohl(gs->rv);
	}
}

INTERNAL void htonlVersionStruct(version_struct *vs)
{
	if (vs)
	{
		vs->major = htonl(vs->major);
		vs->minor = htonl(vs->minor);
		vs->rv = htonl(vs->rv);
	}
}

INTERNAL void ntohlVersionStruct(version_struct *vs)
{
	if (vs)
	{
		vs->major = ntohl(vs->major);
		vs->minor = ntohl(vs->minor);
		vs->rv = ntohl(vs->rv);
	}
}

