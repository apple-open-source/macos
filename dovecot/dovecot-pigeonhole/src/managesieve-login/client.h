/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __CLIENT_H
#define __CLIENT_H

#include "network.h"
#include "client-common.h"

/* maximum length for managesieve command line. */
#define MAX_MANAGESIEVE_LINE 8192

struct managesieve_client {
	struct client common;

	const struct managesieve_login_settings *set;
	struct managesieve_parser *parser;

	unsigned int proxy_state;	

	const char *cmd_name;

	unsigned int cmd_finished:1;
	unsigned int skip_line:1;

	unsigned int proxy_starttls:1;
	unsigned int proxy_sasl_plain:1;
};

bool client_skip_line(struct managesieve_client *client);

void _client_send_response(struct client *client,
  const char *oknobye, const char *resp_code, const char *msg);

#define client_send_ok(client, msg) \
	_client_send_response(client, "OK", NULL, msg)
#define client_send_no(client, msg) \
  _client_send_response(client, "NO", NULL, msg)
#define client_send_bye(client, msg) \
  _client_send_response(client, "BYE", NULL, msg)

#define client_send_okresp(client, resp_code, msg) \
  _client_send_response(client, "OK", resp_code, msg)
#define client_send_noresp(client, resp_code, msg) \
  _client_send_response(client, "NO", resp_code, msg)
#define client_send_byeresp(client, resp_code, msg) \
  _client_send_response(client, "BYE", resp_code, msg)

#endif /* __CLIENT_H */
