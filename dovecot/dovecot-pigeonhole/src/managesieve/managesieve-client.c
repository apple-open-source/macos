/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ioloop.h"
#include "llist.h"
#include "str.h"
#include "hostpid.h"
#include "network.h"
#include "istream.h"
#include "ostream.h"
#include "var-expand.h"
#include "master-service.h"
#include "mail-storage-service.h"
#include "mail-namespace.h"

#include "sieve-storage.h"

#include "managesieve-quote.h"
#include "managesieve-common.h"
#include "managesieve-commands.h"
#include "managesieve-client.h"

#include <stdlib.h>
#include <unistd.h>

#define CRITICAL_MSG \
  "Internal error occured. Refer to server log for more information."
#define CRITICAL_MSG_STAMP CRITICAL_MSG " [%Y-%m-%d %H:%M:%S]"

extern struct mail_storage_callbacks mail_storage_callbacks;
struct managesieve_module_register managesieve_module_register = { 0 };

struct client *managesieve_clients = NULL;
unsigned int managesieve_client_count = 0;

static const char *managesieve_sieve_get_homedir
(void *context)
{
    struct mail_user *mail_user = (struct mail_user *) context;
    const char *home = NULL;

    if ( mail_user == NULL )
        return NULL;

    if ( mail_user_get_home(mail_user, &home) <= 0 )
        return NULL;

    return home;
}

static const char *managesieve_sieve_get_setting
(void *context, const char *identifier)
{
    struct mail_user *mail_user = (struct mail_user *) context;

    if ( mail_user == NULL )
        return NULL;

    return mail_user_plugin_getenv(mail_user, identifier);
}

static const struct sieve_environment managesieve_sieve_env = {
	managesieve_sieve_get_homedir,
	managesieve_sieve_get_setting
};

static void client_idle_timeout(struct client *client)
{
	if (client->cmd.func != NULL) {
		client_destroy(client,
			"Disconnected for inactivity in reading our output");
	} else {
		client_send_bye(client, "Disconnected for inactivity");
		client_destroy(client, "Disconnected for inactivity");
	}
}

static struct sieve_storage *client_get_storage
(struct sieve_instance *svinst, struct mail_user *user, 
	const struct managesieve_settings *set)
{
	struct sieve_storage *storage;
	const char *home;

	if ( mail_user_get_home(user, &home) <= 0 )
	home = NULL;

	storage = sieve_storage_create
		(svinst, user->username, home, set->mail_debug);

	if (storage == NULL) {
		struct tm *tm;
		char str[256];

		tm = localtime(&ioloop_time);

		printf("BYE \"%s\"\n",
			strftime(str, sizeof(str), CRITICAL_MSG_STAMP, tm) > 0 ?
				i_strdup(str) : i_strdup(CRITICAL_MSG));

		i_fatal("Failed to open Sieve storage.");
    }

	return storage;
}

struct client *client_create
(int fd_in, int fd_out, struct mail_user *user,
	struct mail_storage_service_user *service_user,
	const struct managesieve_settings *set)
{
	struct client *client;
	const char *ident;
	struct sieve_instance *svinst;
	struct sieve_storage *storage;

	/* Always use nonblocking I/O */

	net_set_nonblock(fd_in, TRUE);
	net_set_nonblock(fd_out, TRUE);

	/* Initialize Sieve instance */

	svinst = sieve_init(&managesieve_sieve_env, (void *) user, set->mail_debug);

	/* Get Sieve storage */

	storage = client_get_storage(svinst, user, set);	

	/* always use nonblocking I/O */
	net_set_nonblock(fd_in, TRUE);
	net_set_nonblock(fd_out, TRUE);

	client = i_new(struct client, 1);
	client->set = set;
	client->service_user = service_user;
	client->fd_in = fd_in;
	client->fd_out = fd_out;
	client->input = i_stream_create_fd
		(fd_in, set->managesieve_max_line_length, FALSE);
	client->output = o_stream_create_fd(fd_out, (size_t)-1, FALSE);

	o_stream_set_flush_callback(client->output, client_output, client);

	client->io = io_add(fd_in, IO_READ, client_input, client);
	client->last_input = ioloop_time;
	client->parser = managesieve_parser_create
		(client->input, client->output, set->managesieve_max_line_length);
	client->to_idle = timeout_add
		(CLIENT_IDLE_TIMEOUT_MSECS, client_idle_timeout, client);

	client->cmd.pool =
		pool_alloconly_create(MEMPOOL_GROWING"client command", 1024*12);
	client->cmd.client = client;
	client->user = user;

	client->svinst = svinst;
	client->storage = storage; 

	ident = mail_user_get_anvil_userip_ident(client->user);
	if (ident != NULL) {
		master_service_anvil_send(master_service, t_strconcat(
			"CONNECT\t", my_pid, "\tsieve/", ident, "\n", NULL));
		client->anvil_sent = TRUE;
	}

	managesieve_client_count++;
	DLLIST_PREPEND(&managesieve_clients, client);
	if (hook_client_created != NULL)
		hook_client_created(&client);

	managesieve_refresh_proctitle();
	return client;
}

static const char *client_stats(struct client *client)
{	
	static struct var_expand_table static_tab[] = {
		{ 'i', NULL, "input" },
		{ 'o', NULL, "output" },
		{ '\0', NULL, NULL }
	};
	struct var_expand_table *tab;
	string_t *str;

	tab = t_malloc(sizeof(static_tab));
	memcpy(tab, static_tab, sizeof(static_tab));

	tab[0].value = dec2str(client->input->v_offset);
	tab[1].value = dec2str(client->output->offset);

	str = t_str_new(128);
	var_expand(str, client->set->managesieve_logout_format, tab);
	return str_c(str);
}

static const char *client_get_disconnect_reason(struct client *client)
{
	errno = client->input->stream_errno != 0 ?
		client->input->stream_errno :
		client->output->stream_errno;
	return errno == 0 || errno == EPIPE ? "Connection closed" :
		t_strdup_printf("Connection closed: %m");
}

void client_destroy(struct client *client, const char *reason)
{
	int ret;

 	i_assert(!client->handling_input);
	i_assert(!client->destroyed);
	client->destroyed = TRUE;

	if (!client->disconnected) {
		client->disconnected = TRUE;
		if (reason == NULL)
			reason = client_get_disconnect_reason(client);
		i_info("%s %s", reason, client_stats(client));	
	}

	managesieve_client_count--;
	DLLIST_REMOVE(&managesieve_clients, client);

	if (client->command_pending) {
		/* try to deinitialize the command */
		i_assert(client->cmd.func != NULL);

		i_stream_close(client->input);
		o_stream_close(client->output);

		client->input_pending = FALSE;

		ret = client->cmd.func(&client->cmd);
		i_assert(ret);
	}

	if (client->anvil_sent) {
		master_service_anvil_send(master_service, t_strconcat(
			"DISCONNECT\t", my_pid, "\tsieve/",
			mail_user_get_anvil_userip_ident(client->user),
			"\n", NULL));
	}
	mail_user_unref(&client->user);

	managesieve_parser_destroy(&client->parser);
	if (client->io != NULL)
		io_remove(&client->io);
	if (client->to_idle_output != NULL)
		timeout_remove(&client->to_idle_output);
	timeout_remove(&client->to_idle);

	i_stream_destroy(&client->input);
	o_stream_destroy(&client->output);

	if (close(client->fd_in) < 0)
		i_error("close(client in) failed: %m");
	if (client->fd_in != client->fd_out) {
		if (close(client->fd_out) < 0)
			i_error("close(client out) failed: %m");
	}

	sieve_storage_free(client->storage);
	sieve_deinit(&client->svinst);

	pool_unref(&client->cmd.pool);
	i_free(client);

	master_service_client_connection_destroyed(master_service);
	managesieve_refresh_proctitle();
}

void client_disconnect(struct client *client, const char *reason)
{
	i_assert(reason != NULL);

	if (client->disconnected)
		return;

	i_info("Disconnected: %s %s", reason, client_stats(client));
	client->disconnected = TRUE;
	(void)o_stream_flush(client->output);

	i_stream_close(client->input);
	o_stream_close(client->output);
}

void client_disconnect_with_error(struct client *client, const char *msg)
{
	client_send_bye(client, msg);
	client_disconnect(client, msg);
}

int client_send_line(struct client *client, const char *data) 
{
	struct const_iovec iov[2];

	if (client->output->closed)
		return -1;

	iov[0].iov_base = data;
	iov[0].iov_len = strlen(data);
	iov[1].iov_base = "\r\n";
	iov[1].iov_len = 2;

	if (o_stream_sendv(client->output, iov, 2) < 0)
		return -1;
	client->last_output = ioloop_time;

	if (o_stream_get_buffer_used_size(client->output) >=
	    CLIENT_OUTPUT_OPTIMAL_SIZE) {
		/* buffer full, try flushing */
		return o_stream_flush(client->output);
	}
	return 1;
}

void client_send_response
(struct client *client, const char *oknobye, const char *resp_code, const char *msg)
{
	string_t *str;
	
	str = t_str_new(128);
	str_append(str, oknobye);

	if ( resp_code != NULL ) {
		str_append(str, " (");
		str_append(str, resp_code);
		str_append_c(str, ')');
	}

	if ( msg != NULL ) {
		str_append_c(str, ' ');
		managesieve_quote_append_string(str, msg, TRUE);
	}

	client_send_line(client, str_c(str));
}

void client_send_command_error
(struct client_command_context *cmd, const char *msg)
{
	struct client *client = cmd->client;
	const char *error, *cmd_name;
	bool fatal;

	if (msg == NULL) {
		msg = managesieve_parser_get_error(client->parser, &fatal);
		if (fatal) {
			client_disconnect_with_error(client, msg);
			return;
		}
	}

	if (cmd->name == NULL)
		error = t_strconcat
			("Error in MANAGESIEVE command: ", msg, NULL);
	else {
		cmd_name = t_str_ucase(cmd->name);
		error = t_strconcat
			("Error in MANAGESIEVE command ", cmd_name, ": ", msg, NULL);
	}

	client_send_no(client, error);

	if (++client->bad_counter >= CLIENT_MAX_BAD_COMMANDS) {
		client_disconnect_with_error(client,
			"Too many invalid MANAGESIEVE commands.");
	}

	/* client_read_args() failures rely on this being set, so that the
	   command processing is stopped even while command function returns
	   FALSE. */
	cmd->param_error = TRUE;
}

void client_send_storage_error
(struct client *client, struct sieve_storage *storage)
{
	enum sieve_error error_code;
	const char *error;

	error = sieve_storage_get_last_error(storage, &error_code);

	switch ( error_code ) {
	case SIEVE_ERROR_TEMP_FAIL:
		client_send_noresp(client, "TRYLATER", error);
		break;

	case SIEVE_ERROR_NO_QUOTA:
	case SIEVE_ERROR_NO_SPACE: /* Not sure if this is appropriate */
		client_send_noresp(client, "QUOTA", error);
		break;

	case SIEVE_ERROR_NOT_FOUND:
		client_send_noresp(client, "NONEXISTENT", error);
		break;

	case SIEVE_ERROR_ACTIVE:
		client_send_noresp(client, "ACTIVE", error);
		break;

	case SIEVE_ERROR_EXISTS:
		client_send_noresp(client, "ALREADYEXISTS", error);
		break;

	case SIEVE_ERROR_NOT_POSSIBLE:
	default:
		client_send_no(client, error);
		break;
	}
}

bool client_read_args(struct client_command_context *cmd, unsigned int count,
	unsigned int flags, bool no_more, struct managesieve_arg **args_r)
{
	struct managesieve_arg *dummy_args_r = NULL;
	int ret;

	if ( args_r == NULL ) args_r = &dummy_args_r; 

	i_assert(count <= INT_MAX);

	ret = managesieve_parser_read_args
		(cmd->client->parser, ( no_more ? 0 : count ), flags, args_r);
	if ( ret >= 0 ) {
		if ( count > 0 || no_more ) {
			if ( ret < (int)count ) {
				client_send_command_error(cmd, "Missing arguments.");
				return FALSE;
			} else if ( no_more && ret > (int)count ) {
				client_send_command_error(cmd, "Too many arguments.");
				return FALSE;
			}
		}
	
		/* all parameters read successfully */
		return TRUE;
	} else if (ret == -2) {
		/* need more data */
		if (cmd->client->input->closed) {
			/* disconnected */
 			cmd->param_error = TRUE;
		}
		return FALSE;
	} else {
		/* error */
		client_send_command_error(cmd, NULL);
		return FALSE;
	}
}

bool client_read_string_args(struct client_command_context *cmd,
			     unsigned int count, bool no_more, ...)
{
	struct managesieve_arg *managesieve_args;
	va_list va;
	const char *str;
	unsigned int i;
	bool result = TRUE;

	if (!client_read_args(cmd, count, 0, no_more, &managesieve_args))
		return FALSE;

	va_start(va, no_more);
	for (i = 0; i < count; i++) {
		const char **ret = va_arg(va, const char **);

		if (managesieve_args[i].type == MANAGESIEVE_ARG_EOL) {
			client_send_command_error(cmd, "Missing arguments.");
			result = FALSE;
			break;
		}

		str = managesieve_arg_string(&managesieve_args[i]);
		if (str == NULL) {
			client_send_command_error(cmd, "Invalid arguments.");
			result = FALSE;
			break;
		}

		if (ret != NULL)
			*ret = str;
	}
	va_end(va);

	return result;
}

void _client_reset_command(struct client *client)
{
	pool_t pool;
	size_t size;

	/* reset input idle time because command output might have taken a
	   long time and we don't want to disconnect client immediately then */
	client->last_input = ioloop_time;
	timeout_reset(client->to_idle);

	client->command_pending = FALSE;
    if (client->io == NULL && !client->disconnected) {
        i_assert(i_stream_get_fd(client->input) >= 0);
        client->io = io_add(i_stream_get_fd(client->input),
                    IO_READ, client_input, client);
    }
    o_stream_set_flush_callback(client->output, client_output, client);

	pool = client->cmd.pool;
	memset(&client->cmd, 0, sizeof(client->cmd));

	p_clear(pool);
	client->cmd.pool = pool;
	client->cmd.client = client;

	managesieve_parser_reset(client->parser);

	/* if there's unread data in buffer, remember that there's input
	   pending and we should get around to calling client_input() soon.
	   This is mostly for APPEND/IDLE. */
	(void)i_stream_get_data(client->input, &size);
	if (size > 0 && !client->destroyed)
		client->input_pending = TRUE;
}

/* Skip incoming data until newline is found,
   returns TRUE if newline was found. */
static bool client_skip_line(struct client *client)
{
	const unsigned char *data;
	size_t i, data_size;

	data = i_stream_get_data(client->input, &data_size);

	for (i = 0; i < data_size; i++) {
		if (data[i] == '\n') {
			client->input_skip_line = FALSE;
			i++;
			break;
		}
	}

	i_stream_skip(client->input, i);
	return !client->input_skip_line;
}

static bool client_handle_input(struct client_command_context *cmd)
{
	struct client *client = cmd->client;

	if (cmd->func != NULL) {
		/* command is being executed - continue it */
		if (cmd->func(cmd) || cmd->param_error) {
			/* command execution was finished */
			if (!cmd->param_error)
				client->bad_counter = 0;
			_client_reset_command(client);
			return TRUE;
		}

		/* unfinished */
    if (client->command_pending)
			o_stream_set_flush_pending(client->output, TRUE);
		return FALSE;
	}

	if (client->input_skip_line) {
		/* we're just waiting for new line.. */
		if (!client_skip_line(client))
			return FALSE;

		/* got the newline */
		_client_reset_command(client);

		/* pass through to parse next command */
	}

	if (cmd->name == NULL) {
		cmd->name = managesieve_parser_read_word(client->parser);
		if (cmd->name == NULL)
			return FALSE; /* need more data */
		cmd->name = p_strdup(cmd->pool, cmd->name);
		managesieve_refresh_proctitle();
	}

	if (cmd->name == '\0') {
		/* command not given - cmd_func is already NULL. */
	} else {
		/* find the command function */
		struct command *command = command_find(cmd->name);

		if (command != NULL) {
			cmd->func = command->func;
		}
	}

	client->input_skip_line = TRUE;
	if (cmd->func == NULL) {
		/* unknown command */
		client_send_command_error(cmd, "Unknown command.");
		_client_reset_command(client);
	} else {
		i_assert(!client->disconnected);

		client_handle_input(cmd);
	}

	return TRUE;
}

void client_input(struct client *client)
{
	struct client_command_context *cmd = &client->cmd;
	int ret;

	if (client->command_pending) {
		/* already processing one command. wait. */
		io_remove(&client->io);
		return;
	}

	client->input_pending = FALSE;
	client->last_input = ioloop_time;
	timeout_reset(client->to_idle);

	switch (i_stream_read(client->input)) {
	case -1:
		/* disconnected */
		client_destroy(client, NULL);
		return;
	case -2:
		/* parameter word is longer than max. input buffer size.
		   this is most likely an error, so skip the new data
		   until newline is found. */
		client->input_skip_line = TRUE;

		client_send_command_error(cmd, "Too long argument.");
		_client_reset_command(client);
		break;
	}

	client->handling_input = TRUE;
	o_stream_cork(client->output);
	do {
		T_BEGIN {
			ret = client_handle_input(cmd);
		} T_END;
	} while (ret && !client->disconnected);
    o_stream_uncork(client->output);
    client->handling_input = FALSE;

	if (client->command_pending)
		client->input_pending = TRUE;

	if (client->output->closed)
		client_destroy(client, NULL);
}

int client_output(struct client *client)
{
	struct client_command_context *cmd = &client->cmd;
	int ret;
	bool finished;

	client->last_output = ioloop_time;
    timeout_reset(client->to_idle);
    if (client->to_idle_output != NULL)
        timeout_reset(client->to_idle_output);

	if ((ret = o_stream_flush(client->output)) < 0) {
		client_destroy(client, NULL);
		return 1;
	}

	if (!client->command_pending)
		return 1;

	/* continue processing command */
	o_stream_cork(client->output);
	client->output_pending = TRUE;
	finished = cmd->func(cmd) || cmd->param_error;

	/* a bit kludgy check. normally we would want to get back to this
	   output handler, but IDLE is a special case which has command
	   pending but without necessarily anything to write. */
	if (!finished && client->output_pending)
		o_stream_set_flush_pending(client->output, TRUE);

	o_stream_uncork(client->output);

	if (finished) {
		/* command execution was finished */
		client->bad_counter = 0;
		_client_reset_command(client);

		if (client->input_pending)
			client_input(client);
	}
	return ret;
}

void clients_destroy_all(void)
{
	while (managesieve_clients != NULL) {
		client_send_bye(managesieve_clients, "Server shutting down.");
		client_destroy(managesieve_clients, "Server shutting down.");
	}
}
