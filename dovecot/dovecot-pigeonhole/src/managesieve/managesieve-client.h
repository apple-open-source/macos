/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __MANAGESIEVE_CLIENT_H
#define __MANAGESIEVE_CLIENT_H

#include "managesieve-commands.h"

struct client;
struct sieve_storage;
struct managesieve_parser;
struct managesieve_arg;

struct client_command_context {
	struct client *client;

	pool_t pool;
	const char *name;

	command_func_t *func;
	void *context;

	unsigned int param_error:1;
};

struct managesieve_module_register {
    unsigned int id;
};

union managesieve_module_context {
    struct managesieve_module_register *reg;
};
extern struct managesieve_module_register managesieve_module_register;

struct client {
	struct client *prev, *next;

	int fd_in, fd_out;
	struct io *io;
	struct istream *input;
	struct ostream *output;
	struct timeout *to_idle, *to_idle_output;

	struct mail_storage_service_user *service_user;
	const struct managesieve_settings *set;

	struct mail_user *user;

	struct sieve_instance *svinst;
	struct sieve_storage *storage;

	time_t last_input, last_output;
	unsigned int bad_counter;

	struct managesieve_parser *parser;
	struct client_command_context cmd;

	unsigned int disconnected:1;
	unsigned int destroyed:1;
	unsigned int command_pending:1;
	unsigned int input_pending:1;
	unsigned int output_pending:1;
	unsigned int handling_input:1;
	unsigned int anvil_sent:1;
	unsigned int input_skip_line:1; /* skip all the data until we've
					   found a new line */
};

extern struct client *managesieve_clients;
extern unsigned int managesieve_client_count;

/* Create new client with specified input/output handles. socket specifies
   if the handle is a socket. */
struct client *client_create(int fd_in, int fd_out, struct mail_user *user,
			     struct mail_storage_service_user *service_user,
			     const struct managesieve_settings *set);
void client_destroy(struct client *client, const char *reason);

void client_dump_capability(struct client *client);

/* Disconnect client connection */
void client_disconnect(struct client *client, const char *reason);
void client_disconnect_with_error(struct client *client, const char *msg);

/* Send a line of data to client. Returns 1 if ok, 0 if buffer is getting full,
   -1 if error */
int client_send_line(struct client *client, const char *data);

void client_send_response(struct client *client,
  const char *oknobye, const char *resp_code, const char *msg);

#define client_send_ok(client, msg) \
  client_send_response(client, "OK", NULL, msg)
#define client_send_no(client, msg) \
  client_send_response(client, "NO", NULL, msg)
#define client_send_bye(client, msg) \
  client_send_response(client, "BYE", NULL, msg)

#define client_send_okresp(client, resp_code, msg) \
  client_send_response(client, "OK", resp_code, msg)
#define client_send_noresp(client, resp_code, msg) \
  client_send_response(client, "NO", resp_code, msg)
#define client_send_byeresp(cmd, resp_code, msg) \
  client_send_response(client, "BYE", resp_code, msg)

/* Send BAD command error to client. msg can be NULL. */
void client_send_command_error(struct client_command_context *cmd,
			       const char *msg);

/* Send storage or sieve related errors to the client */
void client_send_storage_error(struct client *client,
             struct sieve_storage *storage);

/* Read a number of arguments. Returns TRUE if everything was read or
   FALSE if either needs more data or error occurred. */
bool client_read_args(struct client_command_context *cmd, unsigned int count,
		      unsigned int flags, bool no_more, struct managesieve_arg **args_r);
/* Reads a number of string arguments. ... is a list of pointers where to
   store the arguments. */
bool client_read_string_args(struct client_command_context *cmd,
			     unsigned int count, bool no_more, ...);

static inline bool client_read_no_args
(struct client_command_context *cmd)
{
	return client_read_args(cmd, 0, 0, TRUE, NULL);
}

void _client_reset_command(struct client *client);
void client_input(struct client *client);
int client_output(struct client *client);

void clients_destroy_all(void);

#endif /* __MANAGESIEVE_CLIENT_H */
