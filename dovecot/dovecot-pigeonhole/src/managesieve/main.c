/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "buffer.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "abspath.h"
#include "str.h"
#include "base64.h"
#include "process-title.h"
#include "restrict-access.h"
#include "fd-close-on-exec.h"
#include "master-interface.h"
#include "master-service.h"
#include "master-login.h"
#include "mail-user.h"
#include "mail-storage-service.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"
#include "managesieve-capabilities.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define IS_STANDALONE() \
        (getenv(MASTER_IS_PARENT_ENV) == NULL)

#define MANAGESIEVE_DIE_IDLE_SECS 10

static bool verbose_proctitle = FALSE;
static struct mail_storage_service_ctx *storage_service;
static struct master_login *master_login = NULL;

void (*hook_client_created)(struct client **client) = NULL;

void managesieve_refresh_proctitle(void)
{
#define MANAGESIEVE_PROCTITLE_PREFERRED_LEN 80
	struct client *client;
	string_t *title = t_str_new(128);

	if (!verbose_proctitle)
		return;

	str_append_c(title, '[');
	switch (managesieve_client_count) {
	case 0:
		str_append(title, "idling");
		break;
	case 1:
		client = managesieve_clients;
		str_append(title, client->user->username);
		if (client->user->remote_ip != NULL) {
			str_append_c(title, ' ');
			str_append(title, net_ip2addr(client->user->remote_ip));
		}

		if ( client->cmd.name != NULL &&
			str_len(title) <= MANAGESIEVE_PROCTITLE_PREFERRED_LEN ) {
			str_append_c(title, ' ');
			str_append(title, client->cmd.name);
		}
		break;
	default:
		str_printfa(title, "%u connections", managesieve_client_count);
		break;
	}
	str_append_c(title, ']');
	process_title_set(str_c(title));
}

static void client_kill_idle(struct client *client)
{
	client_send_bye(client, "Server shutting down.");
	client_destroy(client, "Server shutting down.");
}

static void managesieve_die(void)
{
	struct client *client, *next;
	time_t last_io, now = time(NULL);
	time_t stop_timestamp = now - MANAGESIEVE_DIE_IDLE_SECS;
	unsigned int stop_msecs;

	for (client = managesieve_clients; client != NULL; client = next) {
		next = client->next;

		last_io = I_MAX(client->last_input, client->last_output);
		if (last_io <= stop_timestamp)
			client_kill_idle(client);
		else {
			timeout_remove(&client->to_idle);
			stop_msecs = (last_io - stop_timestamp) * 1000;
			client->to_idle = timeout_add(stop_msecs,
						      client_kill_idle, client);
		}
	}
}

static void client_add_input(struct client *client, const buffer_t *buf)
{
	struct ostream *output;

	if (buf != NULL && buf->used > 0) {
		if (!i_stream_add_data(client->input, buf->data, buf->used))
			i_panic("Couldn't add client input to stream");
	}

	output = client->output;
	o_stream_ref(output);
	o_stream_cork(output);
	if (!IS_STANDALONE())
		client_send_ok(client, "Logged in.");
  (void)client_input(client);
	o_stream_uncork(output);
	o_stream_unref(&output);
}

static int
client_create_from_input(const struct mail_storage_service_input *input,
			 int fd_in, int fd_out, const buffer_t *input_buf,
			 const char **error_r)
{
	struct mail_storage_service_user *user;
	struct mail_user *mail_user;
	struct client *client;
	const struct managesieve_settings *set;

	if (mail_storage_service_lookup_next(storage_service, input,
					     &user, &mail_user, error_r) <= 0)
		return -1;
	restrict_access_allow_coredumps(TRUE);

	set = mail_storage_service_user_get_set(user)[1];
	if (set->verbose_proctitle)
		verbose_proctitle = TRUE;

	client = client_create(fd_in, fd_out, mail_user, user, set);
	T_BEGIN {
		client_add_input(client, input_buf);
	} T_END;

	return 0;
}

static void main_stdio_run(const char *username)
{
	struct mail_storage_service_input input;
	const char *value, *error, *input_base64;
	buffer_t *input_buf;

	memset(&input, 0, sizeof(input));
	input.module = input.service = "managesieve";
	input.username =  username != NULL ? username : getenv("USER");
	if (input.username == NULL && IS_STANDALONE())
		input.username = getlogin();
	if (input.username == NULL)
		i_fatal("USER environment missing");
	if ((value = getenv("IP")) != NULL)
		net_addr2ip(value, &input.remote_ip);
	if ((value = getenv("LOCAL_IP")) != NULL)
		net_addr2ip(value, &input.local_ip);

	input_base64 = getenv("CLIENT_INPUT");
	input_buf = input_base64 == NULL ? NULL :
		t_base64_decode_str(input_base64);

	if (client_create_from_input(&input, STDIN_FILENO, STDOUT_FILENO,
				     input_buf, &error) < 0)
		i_fatal("%s", error);
}

static void
login_client_connected(const struct master_login_client *client,
		       const char *username, const char *const *extra_fields)
{
	struct mail_storage_service_input input;
	const char *error;
	buffer_t input_buf;

	memset(&input, 0, sizeof(input));
	input.module = input.service = "managesieve";
	input.local_ip = client->auth_req.local_ip;
	input.remote_ip = client->auth_req.remote_ip;
	input.username = username;
	input.userdb_fields = extra_fields;

	buffer_create_const_data(&input_buf, client->data,
				 client->auth_req.data_size);
	if (client_create_from_input(&input, client->fd, client->fd,
				     &input_buf, &error) < 0) {
		i_error("%s", error);
		(void)close(client->fd);
		master_service_client_connection_destroyed(master_service);
	}
}

static void login_client_failed(const struct master_login_client *client,
				const char *errormsg)
{
	const char *msg;

	msg = t_strdup_printf("NO \"%s\"\r\n", errormsg);
	if (write(client->fd, msg, strlen(msg)) < 0) {
		/* ignored */
	}
}

static void client_connected(struct master_service_connection *conn)
{
	/* when running standalone, we shouldn't even get here */
	i_assert(master_login != NULL);

	master_service_client_connection_accept(conn);
	master_login_add(master_login, conn->fd);
}

int main(int argc, char *argv[])
{
	static const struct setting_parser_info *set_roots[] = {
		&managesieve_setting_parser_info,
		NULL
	};
	enum master_service_flags service_flags = 0;
	enum mail_storage_service_flags storage_service_flags = 0;
	const char *postlogin_socket_path, *username = NULL;
	int c;

	if (IS_STANDALONE() && getuid() == 0 &&
	    net_getpeername(1, NULL, NULL) == 0) {
		printf("NO \"managesieve binary must not be started from "
		       "inetd, use managesieve-login instead.\"\n");
		return 1;
	}

	if (IS_STANDALONE() || getenv("DUMP_CAPABILITY") != NULL) {
		service_flags |= MASTER_SERVICE_FLAG_STANDALONE |
			MASTER_SERVICE_FLAG_STD_CLIENT;
	} else {
		service_flags |= MASTER_SERVICE_FLAG_KEEP_CONFIG_OPEN;
		storage_service_flags |=
			MAIL_STORAGE_SERVICE_FLAG_DISALLOW_ROOT;
	}

	master_service = master_service_init("managesieve", service_flags,
					     &argc, &argv, "u:");
	while ((c = master_getopt(master_service)) > 0) {
		switch (c) {
		case 'u':
			storage_service_flags |=
				MAIL_STORAGE_SERVICE_FLAG_USERDB_LOOKUP;
			username = optarg;
			break;
		default:
			return FATAL_DEFAULT;
		}
	}
	postlogin_socket_path = argv[optind] == NULL ? NULL :
		t_abspath(argv[optind]);

	master_service_init_finish(master_service);
	master_service_set_die_callback(master_service, managesieve_die);

	/* plugins may want to add commands, so this needs to be called early */
	commands_init();

	/* Dump capabilities if requested */
	if ( getenv("DUMP_CAPABILITY") != NULL ) {
		managesieve_capabilities_dump();
		exit(0);
	}

	storage_service =
		mail_storage_service_init(master_service,
					  set_roots, storage_service_flags);

	/* fake that we're running, so we know if client was destroyed
		while handling its initial input */
	io_loop_set_running(current_ioloop);

	if (IS_STANDALONE()) {
		T_BEGIN {
			main_stdio_run(username);
		} T_END;
	} else {
		master_login = master_login_init(master_service,
						 t_abspath("auth-master"),
						 postlogin_socket_path,
						 login_client_connected,
						 login_client_failed);
		io_loop_set_running(current_ioloop);
	}

	if (io_loop_is_running(current_ioloop))
		master_service_run(master_service, client_connected);
	clients_destroy_all();

	if (master_login != NULL)
		master_login_deinit(&master_login);
	mail_storage_service_deinit(&storage_service);

	commands_deinit();

	master_service_deinit(&master_service);
	return 0;
}
