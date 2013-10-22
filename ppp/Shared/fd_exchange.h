/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */
#ifndef __VPN_FD_EXCHANGE_H__
#define __VPN_FD_EXCHANGE_H__

struct fd_exchange;

typedef void (^FDExchangeMsgHandler)(int fd, const void *data, size_t data_len);

struct fd_exchange *fd_exchange_create(int exchange_socket, CFRunLoopRef cb_runloop);
void fd_exchange_destroy(struct fd_exchange *exchange);
void fd_exchange_set_request_handler(struct fd_exchange *exchange, int type, FDExchangeMsgHandler handler);
bool fd_exchange_send_response(struct fd_exchange *exchange, int type, int fd, const void *data, size_t data_len);
bool fd_exchange_send_request(struct fd_exchange *exchange, int type, int fd, const void *data, size_t data_len, FDExchangeMsgHandler handler);

#endif /* __VPN_FD_EXCHANGE_H__ */
