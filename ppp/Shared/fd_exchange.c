/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */
#include <sys/queue.h>
#include <sys/socket.h>
#include <dispatch/dispatch.h>
#include <SystemConfiguration/SCPrivate.h>

#include "fd_exchange.h"

#define FD_EXCHANGE_MAX_DATA_SIZE	4096

typedef void (^FDGenericHandler)(void);
typedef void (^FDExchangeAckHandler)(void);

enum {
	kFDExchangeMessageTypeRequest,
	kFDExchangeMessageTypeResponse,
	kFDExchangeMessageTypeAck,
};

typedef int FDExchangeMessageType;

struct fd_msg_handler {
	FDExchangeMessageType			msg_type;
	int								fd_type;
	FDGenericHandler				cb;
	STAILQ_ENTRY(fd_msg_handler)	link;
};

struct fd_exchange {
	dispatch_queue_t				queue;
	dispatch_source_t				socket_source;
	CFRunLoopRef					callback_rl;
	STAILQ_HEAD(, fd_msg_handler)	handler_queue;
};

static void
fd_exchange_dealloc(void *context)
{
	CFAllocatorDeallocate(kCFAllocatorDefault, context);
}

static void
fd_exchange_add_handler(struct fd_exchange *exchange, int fd_type, FDExchangeMessageType msg_type, FDGenericHandler handler)
{
	struct fd_msg_handler *new_handler = (struct fd_msg_handler *)CFAllocatorAllocate(kCFAllocatorDefault, sizeof(*new_handler), 0);
	new_handler->fd_type = fd_type;
	new_handler->msg_type = msg_type;
	new_handler->cb = Block_copy(handler);
	STAILQ_INSERT_TAIL(&exchange->handler_queue, new_handler, link);
}

static void
fd_exchange_remove_handler(struct fd_exchange *exchange, struct fd_msg_handler *handler)
{
	STAILQ_REMOVE(&exchange->handler_queue, handler, fd_msg_handler, link);
	Block_release(handler->cb);
	CFAllocatorDeallocate(kCFAllocatorDefault, handler);
}

static bool
fd_exchange_send_msg(int socket, int fd_type, FDExchangeMessageType msg_type, int fd, const void *data, size_t data_size)
{
	struct msghdr   msg;
	struct iovec    iov[3];
	int				iov_idx = 0;

	if (data_size > FD_EXCHANGE_MAX_DATA_SIZE) {
		SCLog(TRUE, LOG_ERR, CFSTR("fd_exchange_send_msg: data is to large (%u)"), data_size);
		return false;
	}

	memset(&msg, 0, sizeof(msg));

	iov[iov_idx].iov_base = (char *)&msg_type;
	iov[iov_idx].iov_len = sizeof(msg_type);
	iov_idx++;

	iov[iov_idx].iov_base = (char *)&fd_type;
	iov[iov_idx].iov_len = sizeof(fd_type);
	iov_idx++;

	if (data != NULL && data_size > 0) {
		iov[iov_idx].iov_base = (char *)data;
		iov[iov_idx].iov_len = data_size;
		iov_idx++;
	}

	msg.msg_iov = iov;
	msg.msg_iovlen = iov_idx;

	if (fd >= 0) {
		struct cmsghdr	*control_msg;
		uint8_t         control_buffer[sizeof(*control_msg) + sizeof(fd)];
		uint32_t        control_msg_len = sizeof(control_buffer);

		memset(control_buffer, 0, control_msg_len);
		control_msg = (struct cmsghdr *)((void *)control_buffer);
		control_msg->cmsg_len = control_msg_len;
		control_msg->cmsg_level = SOL_SOCKET;
		control_msg->cmsg_type = SCM_RIGHTS;
		memcpy(CMSG_DATA(control_msg), (void *)&fd, sizeof(fd));

		msg.msg_control = control_msg;
		msg.msg_controllen = control_msg_len;
	}

	if (sendmsg(socket, &msg, 0) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("sendmsg failed while sending an fd: %s"), strerror(errno));
		return false;
	}

	return true;
}

static void
fd_exchange_handle_message(struct fd_exchange *exchange, int msg_type, int fd_type, int received_fd, void *ancilliary_data, size_t ancilliary_data_size)
{
	__block FDGenericHandler callback = NULL;
	dispatch_sync(exchange->queue, ^{
		struct fd_msg_handler	*handler = NULL;
		/* Find the next handler for this file descriptor type and message type */
		STAILQ_FOREACH(handler, &exchange->handler_queue, link) {
			if (handler->fd_type == fd_type && handler->msg_type == msg_type) {
				break;
			}
		}

		if (handler != NULL) {
			callback = Block_copy(handler->cb);
			/* Remove response and ack handlers. Request handlers are re-used */
			if (msg_type == kFDExchangeMessageTypeResponse || msg_type == kFDExchangeMessageTypeAck) {
				fd_exchange_remove_handler(exchange, handler);
			}
		}
	});

	if (callback != NULL) {
		if (msg_type == kFDExchangeMessageTypeRequest || msg_type == kFDExchangeMessageTypeResponse) {
			((FDExchangeMsgHandler)callback)(received_fd, ancilliary_data, ancilliary_data_size);
		} else if (msg_type == kFDExchangeMessageTypeAck) {
			((FDExchangeAckHandler)callback)();
		}
		Block_release(callback);
	} else if (received_fd >= 0) {
		SCLog(TRUE, LOG_WARNING, CFSTR("No handler available for message type %d and fd %d and fd type %d"),
		      msg_type, received_fd, fd_type);
		/* No handler for this message, close the received file descriptor */
		close(received_fd);
	}
}

struct fd_exchange *
fd_exchange_create(int exchange_socket, CFRunLoopRef cb_runloop)
{
	struct fd_exchange *new_exchange = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(*new_exchange), 0);

	memset(new_exchange, 0, sizeof(*new_exchange));

	new_exchange->callback_rl = (CFRunLoopRef)CFRetain(cb_runloop);
	STAILQ_INIT(&new_exchange->handler_queue);

	new_exchange->queue = dispatch_queue_create("fd_exchange_queue", NULL);
	dispatch_set_context(new_exchange->queue, new_exchange);
	dispatch_set_finalizer_f(new_exchange->queue, fd_exchange_dealloc);

	new_exchange->socket_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, exchange_socket, 0, new_exchange->queue);
	dispatch_source_set_cancel_handler(new_exchange->socket_source, ^{
		close(exchange_socket);
	});

	dispatch_source_set_event_handler(new_exchange->socket_source, ^{
		struct msghdr			msg;
		struct cmsghdr			*control;
		uint8_t					control_buffer[sizeof(*control) + sizeof(int)];
		unsigned int			control_len = sizeof(control_buffer);
		int						result;
		char					data_buffer[FD_EXCHANGE_MAX_DATA_SIZE];
		int						fd_type;
		FDExchangeMessageType	msg_type;
		struct iovec        	iov[3];
		int						iov_idx = 0;

		control = (struct cmsghdr *)((void *)control_buffer);

		memset(&msg, 0, sizeof(msg));
		memset(control_buffer, 0, control_len);

		iov[iov_idx].iov_base = (char *)&msg_type;
		iov[iov_idx].iov_len = sizeof(msg_type);
		iov_idx++;

		iov[iov_idx].iov_base = (char *)&fd_type;
		iov[iov_idx].iov_len = sizeof(fd_type);
		iov_idx++;

		iov[iov_idx].iov_base = data_buffer;
		iov[iov_idx].iov_len = sizeof(data_buffer);
		iov_idx++;

		msg.msg_iov = iov;
		msg.msg_iovlen = iov_idx;

		msg.msg_control = control;
		msg.msg_controllen = control_len;

		result = recvmsg(exchange_socket, &msg, 0);
		if (result > 0) {
			if (result >= sizeof(fd_type) + sizeof(msg_type)) {
				size_t	data_size = result - sizeof(fd_type) - sizeof(msg_type);
				int		received_fd = -1;
				void	*ancilliary_data = NULL;

				if (data_size > 0) {
					ancilliary_data = CFAllocatorAllocate(kCFAllocatorDefault, data_size, 0);
					memcpy(ancilliary_data, data_buffer, data_size);
				}

				if (control->cmsg_len == control_len &&
				    control->cmsg_level == SOL_SOCKET &&
				    control->cmsg_type == SCM_RIGHTS)
				{
					/* A file descriptor was received, copy it out */
					memcpy(&received_fd, CMSG_DATA(control), sizeof(received_fd));
				}

				dispatch_retain(new_exchange->queue);
				CFRunLoopPerformBlock(new_exchange->callback_rl, kCFRunLoopDefaultMode, ^{
					fd_exchange_handle_message(new_exchange, msg_type, fd_type, received_fd, ancilliary_data, data_size);
					dispatch_release(new_exchange->queue);
					if (ancilliary_data != NULL) {
						CFAllocatorDeallocate(kCFAllocatorDefault, ancilliary_data);
					}
				});
				CFRunLoopWakeUp(new_exchange->callback_rl);

				/* If a file descriptor was received in a response, send an ack */
				if (msg_type == kFDExchangeMessageTypeResponse && received_fd >= 0) {
					if (!fd_exchange_send_msg((int)dispatch_source_get_handle(new_exchange->socket_source),
					                          fd_type, kFDExchangeMessageTypeAck, -1, NULL, 0))
					{
						SCLog(TRUE, LOG_WARNING, CFSTR("Failed to send an ACK for fd %d of type %d"), received_fd, fd_type);
					}
				}
			} else {
				SCLog(TRUE, LOG_ERR, CFSTR("Got a fd exchange message that is too short (%d)"), result);
			}
		} else if (result == 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("recvmsg returned EOF"));
			dispatch_source_cancel(new_exchange->socket_source);
		} else {
			SCLog(TRUE, LOG_ERR, CFSTR("recvmsg returned an error: %s"), strerror(errno));
			dispatch_source_cancel(new_exchange->socket_source);
		}
	});

	dispatch_resume(new_exchange->socket_source);

	return new_exchange;
}

void
fd_exchange_destroy(struct fd_exchange *exchange)
{
	if (exchange != NULL) {
		dispatch_queue_t queue = exchange->queue;

		dispatch_retain(queue);
		dispatch_sync(queue, ^{
			/* Clear out all handlers. This will prevent any more callbacks from being called */
			while (!STAILQ_EMPTY(&exchange->handler_queue)) {
				struct fd_msg_handler *handler = STAILQ_FIRST(&exchange->handler_queue);
				/* Call all ACK handlers to prevent file descriptor leaks */
				if (handler->msg_type == kFDExchangeMessageTypeAck) {
					((FDExchangeAckHandler)handler->cb)();
				}
				fd_exchange_remove_handler(exchange, handler);
			}
			CFRelease(exchange->callback_rl);
			exchange->callback_rl = NULL;

			dispatch_source_cancel(exchange->socket_source);
			dispatch_release(exchange->socket_source);
			exchange->socket_source = NULL;

			/* The finalizer function for the queue will take care of de-allocating the exchange */
			dispatch_release(exchange->queue);
		});
		dispatch_release(queue);
	}
}

void 
fd_exchange_set_request_handler(struct fd_exchange *exchange, int type, FDExchangeMsgHandler handler)
{
	dispatch_sync(exchange->queue, ^{
		fd_exchange_add_handler(exchange, type, kFDExchangeMessageTypeRequest, (FDGenericHandler)handler);
	});
}

bool
fd_exchange_send_response(struct fd_exchange *exchange, int type, int fd, const void *data, size_t data_size)
{
	__block bool success = true;

	dispatch_sync(exchange->queue, ^{
		if (fd_exchange_send_msg((int)dispatch_source_get_handle(exchange->socket_source), type, kFDExchangeMessageTypeResponse, fd, data, data_size)) {
			if (fd >= 0) {
				/* If a file descriptor was sent, close it when we receive the acknowledgment */
				fd_exchange_add_handler(exchange, type, kFDExchangeMessageTypeAck, ^{
					close(fd);
				});
			}
		} else {
			success = false;
		}
	});

	return success;
}

bool
fd_exchange_send_request(struct fd_exchange *exchange, int type, int fd, const void *data, size_t data_size, FDExchangeMsgHandler handler)
{
	__block bool success = true;

	dispatch_sync(exchange->queue, ^{
		if (fd_exchange_send_msg((int)dispatch_source_get_handle(exchange->socket_source), type, kFDExchangeMessageTypeRequest, fd, data, data_size))
		{
			/* The request was sent successfully, set up the response handler if one was specified */
			if (handler != NULL) {
				fd_exchange_add_handler(exchange, type, kFDExchangeMessageTypeResponse, (FDGenericHandler)handler);
			}
		} else {
			success = false;
		}
	});

	return success;
}

