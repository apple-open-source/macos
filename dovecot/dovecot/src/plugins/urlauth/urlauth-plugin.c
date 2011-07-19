/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

/* Implements RFC 4467 */

#include "lib.h"
#include "str.h"
#include "imap-common.h"
#include "imap-client.h"
#include "hostpid.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "mail-user.h"
#include "mail-search.h"
#include "mail-storage-private.h"
#include "imap-fetch.h"
#include "imap-search-args.h"
#include "urlauth-plugin.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* context for processing nested loops asynchronously:
   for (each url in the urlfetch command)
	while (the imap fetch is unfinished)
		print the info, but non-blocking */
struct urlfetch_context {
	/* pertaining to the urlfetch command as a whole */
	pool_t pool;
	struct client_command_context *cmd;
	const struct imap_arg *urlfetch_args;
	string_t *response;
	const char *hostport;
	unsigned int debug:1;
	unsigned int failed:1;

	/* pertaining to the individual url being processed */
	const char *url;
	struct mailbox *box;
	struct istream *input;
	struct imap_parser *parser;
	struct mail_search_args *search_args;
	struct imap_fetch_context *fetch_ctx;
};

enum urlfetch_status {
	URLFETCH_DONE,
	URLFETCH_MORE,
	URLFETCH_UNFINISHED
};

const char *urlauth_plugin_version = DOVECOT_VERSION;

static struct module *urlauth_module;
static void (*next_hook_client_created)(struct client **client);

extern void (*hook_select_send_urlmech)(struct client *client);
extern void (*hook_delete_mailbox)(struct mailbox *box);

// GENURLAUTH command
static bool cmd_genurlauth(struct client_command_context *cmd)
{
	const struct imap_arg *args;
	string_t *auth_urls;
	const char *hostport, *rump, *mech;

	/* url mechanism [url mechanism ...] */
	if (!client_read_args(cmd, 0, 0, &args))
		return FALSE;

	auth_urls = t_str_new(256);

	// resolve this each time in case my_hostname changed
	hostport = mail_user_plugin_getenv(cmd->client->user, "urlauth_hostport");
	if (hostport == NULL || *hostport == '\0')
		hostport = my_hostname;

	while (imap_arg_get_astring(&args[0], &rump) &&
	       imap_arg_get_astring(&args[1], &mech)) {
		struct imap_url_parts enc_parts, dec_parts;
		const char *error = NULL, *mailbox;
		struct mail_namespace *ns;
		struct mailbox *box;
		buffer_t *key;
		enum mailbox_name_status status;

		if (strcasecmp(mech, "INTERNAL") != 0) {
			client_send_command_error(cmd,
						  "Unsupported mechanism.");
			return TRUE;
		}

		memset(&enc_parts, 0, sizeof enc_parts);
		memset(&dec_parts, 0, sizeof dec_parts);
		imap_url_parse(rump, &enc_parts);
		if (!imap_url_decode(&enc_parts, &dec_parts, &error) ||
		    !urlauth_url_validate(&dec_parts, FALSE, &error)) {
			if (error)
				client_send_command_error(cmd,
					t_strconcat("Invalid arguments: ",
						    error, NULL));
			else
				client_send_command_error(cmd,
							  "Invalid arguments.");
			return TRUE;
		}

		if (cmd->client->user->mail_debug) {
			string_t *url = t_str_new(strlen(rump) + 1);
			imap_url_construct(&enc_parts, url);
			if (strcasecmp(rump, str_c(url)) != 0)
				i_warning("GENURLAUTH: reconstructed URL (%s) does not match original (%s)",
					  str_c(url), rump);
		}

		// username and servername must match
		if (strcmp(dec_parts.user, cmd->client->user->username) != 0 ||
		    strcmp(dec_parts.hostport, hostport) != 0) {
			client_send_command_error(cmd, "User/server mismatch.");
			return TRUE;
		}

		// mailbox must exist
		ns = client_find_namespace(cmd, dec_parts.mailbox,
					   &mailbox, &status);
		if (ns == NULL)
			return TRUE;
		switch (status) {
		case MAILBOX_NAME_EXISTS_MAILBOX:
			break;
		case MAILBOX_NAME_EXISTS_DIR:
			status = MAILBOX_NAME_VALID;
			/* fall through */
		case MAILBOX_NAME_VALID:
		case MAILBOX_NAME_INVALID:
		case MAILBOX_NAME_NOINFERIORS:
			client_fail_mailbox_name_status(cmd, dec_parts.mailbox, NULL, status);
			return TRUE;
		}

		// get the access key
		key = buffer_create_dynamic(pool_datastack_create(),
					    URLAUTH_KEY_BYTES);
		box = mailbox_alloc(ns->list, mailbox, 0);
		if (!urlauth_keys_get(box, key)) {
			mailbox_free(&box);
			client_send_tagline(cmd, "NO "MAIL_ERRSTR_CRITICAL_MSG);
			return TRUE;
		}
		mailbox_free(&box);

		// generate the reply
		if (str_len(auth_urls) > 0)
			str_append_c(auth_urls, ' ');
		str_append_c(auth_urls, '"');
		str_append(auth_urls, rump);
		str_append(auth_urls, ":INTERNAL:");
		urlauth_urlauth_generate_internal(rump, key, auth_urls);
		str_append_c(auth_urls, '"');

		args += 2;
	}
	if (args[0].type == IMAP_ARG_EOL) {
		if (str_len(auth_urls) > 0) {
			client_send_line(cmd->client,
					 t_strconcat("* GENURLAUTH ",
						     str_c(auth_urls), NULL));
			client_send_tagline(cmd, "OK Genurlauth completed.");
		} else
			client_send_command_error(cmd, "Missing arguments.");
	} else if (args[1].type == IMAP_ARG_EOL)
		client_send_command_error(cmd, "Missing arguments.");
	else
		client_send_command_error(cmd, "Invalid arguments.");

	return TRUE;
}

// initialize context for traversing the urlauth maze
static struct urlfetch_context *
urlfetch_init(struct client_command_context *cmd, const struct imap_arg *args)
{
	struct urlfetch_context *ufctx;

	pool_ref(cmd->pool);
	ufctx = p_new(cmd->pool, struct urlfetch_context, 1);
	ufctx->pool = cmd->pool;
	ufctx->cmd = cmd;
	ufctx->urlfetch_args = args;

	ufctx->response = str_new(ufctx->pool, 256);
	str_append(ufctx->response, "* URLFETCH");

	// resolve this each time in case my_hostname changed
	ufctx->hostport = mail_user_plugin_getenv(cmd->client->user, "urlauth_hostport");
	if (ufctx->hostport == NULL || *ufctx->hostport == '\0')
		ufctx->hostport = my_hostname;

	ufctx->debug = cmd->client->user->mail_debug;
	return ufctx;
}

// reset the context for the next url
static enum urlfetch_status urlfetch_reset(struct urlfetch_context *ufctx)
{
	if (ufctx->fetch_ctx != NULL) {
		if (!ufctx->fetch_ctx->urlfetched) {
			// found no mail with that uid
			// HERE - probably should send BAD response instead
			if (ufctx->debug)
				i_info("URLFETCH: \"%s\" failed: UID not found",
				       ufctx->url);
			i_assert(str_len(ufctx->response) == 0);
			str_append_str(ufctx->response,
				       ufctx->fetch_ctx->cur_str);
			str_append(ufctx->response, " NIL");
		}
		if (imap_fetch_deinit(ufctx->fetch_ctx) < 0)
			ufctx->fetch_ctx->failed = TRUE;
		if (ufctx->fetch_ctx->failed)
			ufctx->failed = TRUE;
		ufctx->fetch_ctx = NULL;
	}
	if (ufctx->search_args != NULL)
		mail_search_args_unref(&ufctx->search_args);
	if (ufctx->parser != NULL)
		imap_parser_destroy(&ufctx->parser);
	if (ufctx->input != NULL)
		i_stream_destroy(&ufctx->input);
	if (ufctx->box != NULL)
		mailbox_free(&ufctx->box);
	ufctx->url = NULL;

	return ufctx->failed ? URLFETCH_DONE : URLFETCH_MORE;
}

// destroy the context
static void urlfetch_deinit(struct urlfetch_context **_ufctx)
{
	struct urlfetch_context *ufctx = *_ufctx;
	*_ufctx = NULL;

	urlfetch_reset(ufctx);

	str_free(&ufctx->response);
	pool_unref(&ufctx->pool);
}

// urlfetch for one url
static int urlfetch_url(struct urlfetch_context *ufctx, const char **error)
{
	struct imap_url_parts enc_parts, dec_parts;
	bool authorized = FALSE, foil = FALSE;
	buffer_t *key;
	string_t *urlauth, *fetch_text;
	struct mail_namespace *ns;
	struct mailbox_status status;
	const struct imap_arg *fetch_args = NULL, *next_arg = NULL;
	const char *mailbox, *foilbox;

	memset(&enc_parts, 0, sizeof enc_parts);
	memset(&dec_parts, 0, sizeof dec_parts);
	imap_url_parse(ufctx->url, &enc_parts);
	if (!imap_url_decode(&enc_parts, &dec_parts, error) ||
	    !urlauth_url_validate(&dec_parts, TRUE, error))
		return -1;
	/* comparing usernames here violates RFC 4467 section 4 but
	   dovecot can't reach into another user's mail storage */
	if (strcmp(dec_parts.user, ufctx->cmd->client->user->username) != 0 ||
	    strcmp(dec_parts.hostport, ufctx->hostport) != 0) {
		*error = "user/server mismatch";
		return -1;
	}

	// expired?
	if (dec_parts.expiration_time != 0 &&
	    dec_parts.expiration_time < ioloop_time) {
		*error = "expired";
		return -1;
	}

	// authorized?
	if (strcasecmp(dec_parts.access, "anonymous") == 0) {
		/* any user */
		authorized = TRUE;
	} else if (strcasecmp(dec_parts.access, "authuser") == 0) {
		/* any non-anonymous user */
		/* As of Dovecot-2.0.9 there is an "anonymous" flag in
		   the plugin_env but it is set not only for SASL ANONYMOUS
		   but also when the anonymous user logs in another way so
		   it breaks "authuser" for the anonymous user. */
		const char *anonymous =
			mail_user_plugin_getenv(ufctx->cmd->client->user,
						"anonymous_username");
		authorized = anonymous == NULL ||
			strcmp(ufctx->cmd->client->user->username,
			       anonymous) != 0;
	} else if (strncasecmp(dec_parts.access, "user+", 5) == 0) {
		/* logged-in user */
		authorized = strcmp(dec_parts.access + 5,
				    ufctx->cmd->client->user->username) == 0;
	} else if (strncasecmp(dec_parts.access, "submit+", 7) == 0) {
		/* logged-in user via submit-user login */
		const char *submit =
			mail_user_plugin_getenv(ufctx->cmd->client->user,
						"submit_user");
		authorized = submit != NULL &&
			strcmp(dec_parts.access + 7,
			       ufctx->cmd->client->user->username) == 0;
	}
	if (!authorized) {
		*error = "not authorized";
		return -1;
	}

	// get the access key
	key = buffer_create_dynamic(pool_datastack_create(),
				    URLAUTH_KEY_BYTES);
	ns = client_find_namespace(ufctx->cmd, dec_parts.mailbox,
				   &mailbox, NULL);
	if (ns == NULL) {
		// invent a key to foil timing attacks
		ns = client_find_namespace(ufctx->cmd, "INBOX", &mailbox, NULL);
		foil = TRUE;
	}
	ufctx->box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_READONLY |
				   MAILBOX_FLAG_KEEP_RECENT);
	foilbox = ufctx->box->name;
	if (foil)
		ufctx->box->name = "";
	if (!urlauth_keys_get(ufctx->box, key)) {
		*error = "can't get mailbox key";
		ufctx->box->name = foilbox;
		return -1;
	}
	ufctx->box->name = foilbox;

	// generate an access token and compare it to the one given
	urlauth = t_str_new(strlen(dec_parts.urlauth));
	urlauth_urlauth_generate_internal(enc_parts.rump, key, urlauth);
	if (strcasecmp(dec_parts.urlauth, str_c(urlauth)) != 0 || foil) {
		*error = "access token mismatch";
		return -1;
	}

	if (mailbox_sync(ufctx->box, MAILBOX_SYNC_FLAG_FULL_READ |
			 MAILBOX_SYNC_FLAG_FAST) < 0) {
		*error = "can't sync mailbox";
		return -1;
	}
	mailbox_get_status(ufctx->box, STATUS_UIDVALIDITY, &status);

	// verify uidvalidity
	if (dec_parts.uidvalidity != NULL &&
	    strtoul(dec_parts.uidvalidity, NULL, 10) != status.uidvalidity) {
		*error = "uidvalidity mismatch";
		return -1;
	}

	/* passed all tests.  go get the data as in
		UID FETCH dec_parts.uid BODY.PEEK[dec_parts.section]
	   but with less output syntax */
	fetch_text = str_new(ufctx->pool,
			     11 + (dec_parts.section ?
				   strlen(dec_parts.section) : 0));
	str_append(fetch_text, "BODY.PEEK[");
	if (dec_parts.section)
		str_append(fetch_text, dec_parts.section);
	str_append(fetch_text, "]");

	ufctx->input = i_stream_create_from_data(str_data(fetch_text),
						 str_len(fetch_text));
	(void) i_stream_read(ufctx->input);

	ufctx->parser = imap_parser_create(ufctx->input, NULL, (size_t) -1);
	if (imap_parser_finish_line(ufctx->parser, 0, 0, &fetch_args) < 0) {
		*error = "can't parse fetch string";
		return -1;
	}

	ufctx->cmd->uid = TRUE;
	if (imap_search_get_uidset_arg(dec_parts.uid,
				       &ufctx->search_args, error) < 0)
		return -1;

	ufctx->fetch_ctx = imap_fetch_init(ufctx->cmd, ufctx->box);
	if (ufctx->fetch_ctx == NULL) {
		*error = "can't init fetch";
		return -1;
	}
	ufctx->fetch_ctx->search_args = ufctx->search_args;
	mail_search_args_ref(ufctx->search_args);
	str_append_str(ufctx->fetch_ctx->cur_str, ufctx->response);
	str_truncate(ufctx->response, 0);
	ufctx->fetch_ctx->urlfetch = TRUE;

	if (!fetch_parse_args(ufctx->fetch_ctx, fetch_args, &next_arg)) {
		*error = "can't parse fetch args";
		return -1;
	}

	if (imap_fetch_begin(ufctx->fetch_ctx) != 0) {
		*error = "can't begin fetch";
		return -1;
	}

	return imap_fetch_more(ufctx->fetch_ctx);
}

// execute imap fetch for one urlfetch url
static enum urlfetch_status urlfetch_more(struct urlfetch_context *ufctx)
{
	if (ufctx->url == NULL) {
		int ret;
		const char *error = NULL;

		if (!imap_arg_get_astring(ufctx->urlfetch_args, &ufctx->url))
			return URLFETCH_DONE;

		++ufctx->urlfetch_args;

		str_printfa(ufctx->response, " \"%s\"", ufctx->url);
		ret = urlfetch_url(ufctx, &error);
		if (ret < 0) {
			if (ufctx->debug)
				i_info("URLFETCH: \"%s\" failed: %s",
				       ufctx->url,
				       error ? error : "(unspecified)");
			str_append(ufctx->response, " NIL");
		} else if (ret == 0)
			return URLFETCH_UNFINISHED;
	} else {
		if (imap_fetch_more(ufctx->fetch_ctx) == 0)
			return URLFETCH_UNFINISHED;
	}

	return urlfetch_reset(ufctx);
}

// finish off an urlfetch command
static int urlfetch_finish(struct urlfetch_context *ufctx)
{
	ufctx->cmd->client->output_squelch = FALSE;

	if (ufctx->failed) {
		if (ufctx->cmd->client->output->closed)
			client_disconnect(ufctx->cmd->client, "Disconnected");
		else {
			const char *error_string;
			enum mail_error error;

			error_string = mail_storage_get_last_error(
				mailbox_get_storage(ufctx->box), &error);

			/* We never want to reply NO to FETCH requests,
			BYE is preferrable (see imap-ml for reasons). */
			client_disconnect_with_error(ufctx->cmd->client,
						     error_string);
		}
	} else {
		client_send_line(ufctx->cmd->client, str_c(ufctx->response));
		client_send_tagline(ufctx->cmd, "OK Urlfetch completed.");
	}

	urlfetch_deinit(&ufctx);
	return TRUE;
}

// client is ready for more output, continue urlfetch where we left off
static bool urlfetch_continue(struct client_command_context *cmd)
{
	struct urlfetch_context *ufctx = cmd->context;
	enum urlfetch_status status;

	while ((status = urlfetch_more(ufctx)) == URLFETCH_MORE)
		;
	if (status == URLFETCH_UNFINISHED)
		return FALSE;
	return urlfetch_finish(ufctx);
}

// URLFETCH command
static bool cmd_urlfetch(struct client_command_context *cmd)
{
	const struct imap_arg *args, *argp;
	struct urlfetch_context *ufctx;
	enum urlfetch_status status;

	/* url [url ...] */
	if (!client_read_args(cmd, 0, 0, &args))
		return FALSE;

	for (argp = args; argp->type != IMAP_ARG_EOL; argp++) {
		if (!IMAP_ARG_TYPE_IS_ASTRING(argp->type)) {
			client_send_command_error(cmd, "Invalid arguments.");
			return TRUE;
		}
	}
	if (argp == args) {
		client_send_command_error(cmd, "Missing arguments.");
		return TRUE;
	}

	/* Squelch all errors and output other than the fetched parts;
	   we are allowed only NIL or "".  Dovecot does not cleanly
	   separate the parsing, execution, and response phases of
	   command handling, but instead interleaves them.  To re-use
	   the imap-fetch code for urlfetch, output_squelch keeps
	   dovecot quiet. */
	cmd->client->output_squelch = TRUE;

	ufctx = urlfetch_init(cmd, args);
	while ((status = urlfetch_more(ufctx)) == URLFETCH_MORE)
		;
	if (status == URLFETCH_UNFINISHED) {
		cmd->state = CLIENT_COMMAND_STATE_WAIT_OUTPUT;
		cmd->func = urlfetch_continue;
		cmd->context = ufctx;
		return FALSE;
	}
	return urlfetch_finish(ufctx);
}

// RESETKEY command
static bool cmd_resetkey(struct client_command_context *cmd)
{
	const struct imap_arg *args;
	struct mail_namespace *ns;
	struct mailbox *box;
	enum mailbox_name_status status;
	const char *mailbox = NULL, *storage_name;
	const char *mechanism = NULL;
	bool reset_all = FALSE;

	/* [mailbox [mechanism]] */
	if (!client_read_args(cmd, 0, 0, &args))
		return FALSE;

	if (imap_arg_get_astring(args, &mailbox)) {
		/* mechanism name may be absent, empty, or "INTERNAL" */
		if (imap_arg_get_astring(&args[1], &mechanism)) {
			if (*mechanism != '\0' &&
			    strcasecmp(mechanism, "INTERNAL") != 0) {
				client_send_command_error(cmd,
						"Unsupported mechanism.");
				return TRUE;
			}
		}
	} else {
		reset_all = args[0].type == IMAP_ARG_EOL;
		mailbox = "INBOX";
	}

	ns = client_find_namespace(cmd, mailbox, &storage_name, &status);
	if (ns == NULL)
		return TRUE;
	switch (status) {
	case MAILBOX_NAME_EXISTS_MAILBOX:
		break;
	case MAILBOX_NAME_EXISTS_DIR:
		status = MAILBOX_NAME_VALID;
		/* fall through */
	case MAILBOX_NAME_VALID:
	case MAILBOX_NAME_INVALID:
	case MAILBOX_NAME_NOINFERIORS:
		client_fail_mailbox_name_status(cmd, mailbox, NULL, status);
		return TRUE;
	}

	box = mailbox_alloc(ns->list, storage_name, 0);
	if (reset_all) {
		if (urlauth_keys_reset(ns->list))
			client_send_tagline(cmd, "OK All keys removed.");
		else
			client_send_tagline(cmd, "NO "MAIL_ERRSTR_CRITICAL_MSG);
	} else {
		if (urlauth_keys_set(box))
			client_send_tagline(cmd, "OK [URLMECH INTERNAL] Key changed.");
		else
			client_send_tagline(cmd, "NO "MAIL_ERRSTR_CRITICAL_MSG);
	}
	mailbox_free(&box);
	return TRUE;
}

// report urlauth mechanisms
static void urlauth_send_urlmech(struct client *client)
{
	client_send_line(client,
			 "* OK [URLMECH INTERNAL] Mechanisms supported");
}

// a mailbox was deleted (or renamed away) so delete its urlauth key
static void urlauth_delete_mailbox(struct mailbox *box)
{
	urlauth_keys_delete(box);
}

static void urlauth_client_created(struct client **client)
{
	if (mail_user_is_plugin_loaded((*client)->user, urlauth_module))
		str_append((*client)->capability_string, " URLAUTH");

	if (next_hook_client_created != NULL)
		next_hook_client_created(client);
}

void urlauth_plugin_init(struct module *module)
{
	command_register("GENURLAUTH", cmd_genurlauth, 0);
	command_register("URLFETCH", cmd_urlfetch,
			 COMMAND_FLAG_OK_FOR_SUBMIT_USER);
	command_register("RESETKEY", cmd_resetkey, 0);
	urlauth_keys_init();

	urlauth_module = module;
	next_hook_client_created =
		imap_client_created_hook_set(urlauth_client_created);

	hook_select_send_urlmech = urlauth_send_urlmech;
	hook_delete_mailbox = urlauth_delete_mailbox;
}

void urlauth_plugin_deinit(void)
{
	command_unregister("GENURLAUTH");
	command_unregister("URLFETCH");
	command_unregister("RESETKEY");
	urlauth_keys_deinit();

	imap_client_created_hook_set(next_hook_client_created);
	hook_select_send_urlmech = NULL;
	hook_delete_mailbox = NULL;
}

const char urlauth_plugin_binary_dependency[] = "imap";
