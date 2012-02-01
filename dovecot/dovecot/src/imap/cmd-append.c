/* Copyright (c) 2002-2011 Dovecot authors, see the included COPYING file */

#include "imap-common.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "str.h"
#include "imap-url.h"					/* APPLE - catenate */
#include "imap-fetch.h"					/* APPLE - catenate */
#include "imap-search-args.h"				/* APPLE - catenate */
#include "fd-set-nonblock.h"				/* APPLE - catenate */
#include "imap-parser.h"
#include "imap-date.h"
#include "imap-util.h"
#include "imap-commands.h"

#include <sys/time.h>
#include <stdlib.h>					/* APPLE - catenate */

/* Don't allow internaldates to be too far in the future. At least with Maildir
   they can cause problems with incremental backups since internaldate is
   stored in file's mtime. But perhaps there are also some other reasons why
   it might not be wanted. */
#define INTERNALDATE_MAX_FUTURE_SECS (2*3600)

struct cmd_append_context {
	struct client *client;
        struct client_command_context *cmd;
	struct mail_storage *storage;
	struct mailbox *box;
        struct mailbox_transaction_context *t;
	time_t started;

	struct istream *input;
	uoff_t msg_size;

	struct imap_parser *save_parser;
	struct mail_save_context *save_ctx;
	unsigned int count;

	/* APPLE - catenate */
	struct {
		struct ostream *output;	/* to pipe; input is from pipe */
		struct istream *literal_input;
		uoff_t literal_size;
		string_t *literal_url;
		unsigned int parts;

		/* also,
			msg_size is the cumulative size of the message
			message_input is used as "reading any literal"
		 */
	} cat;
	const struct imap_arg *args;

	unsigned int message_input:1;
	unsigned int failed:1;
};

static void cmd_append_finish(struct cmd_append_context *ctx);
static bool cmd_append_continue_message(struct client_command_context *cmd);
static bool cmd_append_continue_parsing(struct client_command_context *cmd);

/* APPLE - catenate */
static bool args_indicate_catenate(const struct imap_arg *args);
static bool catenate_begin_parsing(struct client_command_context *cmd,
				   const struct imap_arg *args);
static bool catenate_begin_cancel(struct client_command_context *cmd,
				  const struct imap_arg *args);

static const char *get_disconnect_reason(struct cmd_append_context *ctx)
{
	string_t *str = t_str_new(128);
	unsigned int secs = ioloop_time - ctx->started;

	str_printfa(str, "Disconnected in APPEND (%u msgs, %u secs",
		    ctx->count, secs);
	if (ctx->input != NULL) {
		str_printfa(str, ", %"PRIuUOFF_T"/%"PRIuUOFF_T" bytes",
			    ctx->input->v_offset, ctx->msg_size);
	}
	str_append_c(str, ')');
	return str_c(str);
}

static void client_input_append(struct client_command_context *cmd)
{
	struct cmd_append_context *ctx = cmd->context;
	struct client *client = cmd->client;
	const char *reason;
	bool finished;

	i_assert(!client->destroyed);

	client->last_input = ioloop_time;
	timeout_reset(client->to_idle);

	switch (i_stream_read(client->input)) {
	case -1:
		/* disconnected */
		reason = get_disconnect_reason(ctx);
		cmd_append_finish(cmd->context);
		/* Reset command so that client_destroy() doesn't try to call
		   cmd_append_continue_message() anymore. */
		client_command_free(&cmd);
		client_destroy(client, reason);
		return;
	case -2:
		if (ctx->message_input) {
			/* message data, this is handled internally by
			   mailbox_save_continue() */
			break;
		}
		cmd_append_finish(cmd->context);

		/* parameter word is longer than max. input buffer size.
		   this is most likely an error, so skip the new data
		   until newline is found. */
		client->input_skip_line = TRUE;

		client_send_command_error(cmd, "Too long argument.");
		cmd->param_error = TRUE;
		client_command_free(&cmd);
		return;
	}

	o_stream_cork(client->output);
	finished = cmd->func(cmd);
	if (!finished && cmd->state != CLIENT_COMMAND_STATE_DONE)
		(void)client_handle_unfinished_cmd(cmd);
	else
		client_command_free(&cmd);
	(void)cmd_sync_delayed(client);
	o_stream_uncork(client->output);

	if (client->disconnected)
		client_destroy(client, NULL);
	else
		client_continue_pending_input(client);
}

/* Returns -1 = error, 0 = need more data, 1 = successful. flags and
   internal_date may be NULL as a result, but mailbox and msg_size are always
   set when successful. */
static int validate_args(const struct imap_arg *args,
			 const struct imap_arg **flags_r,
			 const char **internal_date_r, uoff_t *msg_size_r,
			 bool *nonsync_r)
{
	/* [<flags>] */
	if (!imap_arg_get_list(args, flags_r))
		*flags_r = NULL;
	else
		args++;

	/* [<internal date>] */
	if (args->type != IMAP_ARG_STRING)
		*internal_date_r = NULL;
	else {
		*internal_date_r = imap_arg_as_astring(args);
		args++;
	}

	if (!imap_arg_get_literal_size(args, msg_size_r)) {
		*nonsync_r = FALSE;
		return FALSE;
	}

	*nonsync_r = args->type == IMAP_ARG_LITERAL_SIZE_NONSYNC;
	return TRUE;
}

static void cmd_append_finish(struct cmd_append_context *ctx)
{
	imap_parser_destroy(&ctx->save_parser);

	i_assert(ctx->client->input_lock == ctx->cmd);

	io_remove(&ctx->client->io);
	/* we must put back the original flush callback before beginning to
	   sync (the command is still unfinished at that point) */
	o_stream_set_flush_callback(ctx->client->output,
				    client_output, ctx->client);

	if (ctx->input != NULL)
		i_stream_unref(&ctx->input);
	if (ctx->save_ctx != NULL)
		mailbox_save_cancel(&ctx->save_ctx);
	if (ctx->t != NULL)
		mailbox_transaction_rollback(&ctx->t);
	if (ctx->box != ctx->cmd->client->mailbox && ctx->box != NULL)
		mailbox_free(&ctx->box);
}

static bool cmd_append_continue_cancel(struct client_command_context *cmd)
{
	struct cmd_append_context *ctx = cmd->context;
	size_t size;

	if (cmd->cancel) {
		cmd_append_finish(ctx);
		return TRUE;
	}

	(void)i_stream_read(ctx->input);
	(void)i_stream_get_data(ctx->input, &size);
	i_stream_skip(ctx->input, size);

	if (cmd->client->input->closed) {
		cmd_append_finish(ctx);
		return TRUE;
	}

	if (ctx->input->v_offset == ctx->msg_size) {
		/* finished, but with MULTIAPPEND and LITERAL+ we may get
		   more messages. */
		i_stream_unref(&ctx->input);
		ctx->input = NULL;

		ctx->message_input = FALSE;
		imap_parser_reset(ctx->save_parser);
		cmd->func = cmd_append_continue_parsing;
		return cmd_append_continue_parsing(cmd);
	}

	return FALSE;
}

static bool cmd_append_cancel(struct cmd_append_context *ctx, bool nonsync)
{
	ctx->failed = TRUE;

	if (!nonsync) {
		cmd_append_finish(ctx);
		return TRUE;
	}

	/* we have to read the nonsynced literal so we don't treat the message
	   data as commands. */
	ctx->input = i_stream_create_limit(ctx->client->input, ctx->msg_size);

	ctx->message_input = TRUE;
	ctx->cmd->func = cmd_append_continue_cancel;
	ctx->cmd->context = ctx;
	return cmd_append_continue_cancel(ctx->cmd);
}

static bool cmd_append_continue_parsing(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	const struct imap_arg *args;
	const struct imap_arg *flags_list;
	enum mail_flags flags;
	const char *const *keywords_list;
	struct mail_keywords *keywords;
	const char *internal_date_str;
	time_t internal_date;
	int ret, timezone_offset;
	unsigned int save_count;
	bool nonsync;

	if (cmd->cancel) {
		cmd_append_finish(ctx);
		return TRUE;
	}

	/* if error occurs, the CRLF is already read. */
	client->input_skip_line = FALSE;

	/* APPLE - catenate */
	if (ctx->args != NULL) {
		args = ctx->args;
		ctx->args = NULL;
	} else {	/* reduce code deltas */
	/* [<flags>] [<internal date>] <message literal> */
	ret = imap_parser_read_args(ctx->save_parser, 0,
				    IMAP_PARSE_FLAG_LITERAL_SIZE, &args);
	if (ret == -1) {
		if (!ctx->failed)
			client_send_command_error(cmd, NULL);
		cmd_append_finish(ctx);
		return TRUE;
	}
	if (ret < 0) {
		/* need more data */
		return FALSE;
	}
	} /* APPLE - catenate */

	if (IMAP_ARG_IS_EOL(args)) {
		/* last message */
		enum mailbox_sync_flags sync_flags;
		enum imap_sync_flags imap_flags;
		struct mail_transaction_commit_changes changes;
		string_t *msg;

		/* eat away the trailing CRLF */
		client->input_skip_line = TRUE;

		if (ctx->failed) {
			/* we failed earlier, error message is sent */
			cmd_append_finish(ctx);
			return TRUE;
		}
		if (ctx->count == 0) {
			client_send_tagline(cmd, "BAD Missing message size.");
			cmd_append_finish(ctx);
			return TRUE;
		}

		ret = mailbox_transaction_commit_get_changes(&ctx->t, &changes);
		if (ret < 0) {
			client_send_storage_error(cmd, ctx->storage);
			cmd_append_finish(ctx);
			return TRUE;
		}

		msg = t_str_new(256);
		save_count = seq_range_count(&changes.saved_uids);
		if (save_count == 0) {
			/* not supported by backend (virtual) */
			str_append(msg, "OK Append completed.");
		} else {
			i_assert(ctx->count == save_count);
			str_printfa(msg, "OK [APPENDUID %u ",
				    changes.uid_validity);
			imap_write_seq_range(msg, &changes.saved_uids);
			str_append(msg, "] Append completed.");
		}
		pool_unref(&changes.pool);

		if (ctx->box == cmd->client->mailbox) {
			sync_flags = 0;
			imap_flags = IMAP_SYNC_FLAG_SAFE;
		} else {
			sync_flags = MAILBOX_SYNC_FLAG_FAST;
			imap_flags = 0;
		}

		cmd_append_finish(ctx);
		return cmd_sync(cmd, sync_flags, imap_flags, str_c(msg));
	}

	/* APPLE - catenate */
	if (args_indicate_catenate(args))
		return ctx->failed ? catenate_begin_cancel(cmd, args) :
				     catenate_begin_parsing(cmd, args);

	if (!validate_args(args, &flags_list, &internal_date_str,
			   &ctx->msg_size, &nonsync)) {
		client_send_command_error(cmd, "Invalid arguments.");
		return cmd_append_cancel(ctx, nonsync);
	}

	if (ctx->failed) {
		/* we failed earlier, make sure we just eat nonsync-literal
		   if it's given. */
		return cmd_append_cancel(ctx, nonsync);
	}

	if (flags_list != NULL) {
		if (!client_parse_mail_flags(cmd, flags_list,
					     &flags, &keywords_list))
			return cmd_append_cancel(ctx, nonsync);
		if (keywords_list == NULL)
			keywords = NULL;
		else if (mailbox_keywords_create(ctx->box, keywords_list,
						 &keywords) < 0) {
			client_send_storage_error(cmd, ctx->storage);
			return cmd_append_cancel(ctx, nonsync);
		}
	} else {
		flags = 0;
		keywords = NULL;
	}

	if (internal_date_str == NULL) {
		/* no time given, default to now. */
		internal_date = (time_t)-1;
		timezone_offset = 0;
	} else if (!imap_parse_datetime(internal_date_str,
					&internal_date, &timezone_offset)) {
		client_send_tagline(cmd, "BAD Invalid internal date.");

		/* APPLE */
		if (keywords != NULL)
			mailbox_keywords_unref(ctx->box, &keywords);

		return cmd_append_cancel(ctx, nonsync);
	}

	if (internal_date != (time_t)-1 &&
	    internal_date > ioloop_time + INTERNALDATE_MAX_FUTURE_SECS) {
		/* the client specified a time in the future, set it to now. */
		internal_date = (time_t)-1;
		timezone_offset = 0;
	}

	if (ctx->msg_size == 0) {
		/* no message data, abort */
		client_send_tagline(cmd, "NO Can't save a zero byte message.");

		/* APPLE */
		if (keywords != NULL)
			mailbox_keywords_unref(ctx->box, &keywords);

		return cmd_append_cancel(ctx, nonsync);
	}

	/* save the mail */
	ctx->input = i_stream_create_limit(client->input, ctx->msg_size);
	ctx->save_ctx = mailbox_save_alloc(ctx->t);
	mailbox_save_set_flags(ctx->save_ctx, flags, keywords);
	mailbox_save_set_received_date(ctx->save_ctx,
				       internal_date, timezone_offset);
	ret = mailbox_save_begin(&ctx->save_ctx, ctx->input);

	if (keywords != NULL)
		mailbox_keywords_unref(ctx->box, &keywords);

	if (ret < 0) {
		/* save initialization failed */
		client_send_storage_error(cmd, ctx->storage);
		return cmd_append_cancel(ctx, nonsync);
	}

	/* after literal comes CRLF, if we fail make sure we eat it away */
	client->input_skip_line = TRUE;

	if (!nonsync) {
		o_stream_send(client->output, "+ OK\r\n", 6);
		o_stream_flush(client->output);
		o_stream_uncork(client->output);
		o_stream_cork(client->output);
	}

	ctx->count++;
	ctx->message_input = TRUE;
	cmd->func = cmd_append_continue_message;
	return cmd_append_continue_message(cmd);
}

static bool cmd_append_continue_message(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	size_t size;
	int ret;

	if (cmd->cancel) {
		cmd_append_finish(ctx);
		return TRUE;
	}

	if (ctx->save_ctx != NULL) {
		while (ctx->input->v_offset != ctx->msg_size) {
			ret = i_stream_read(ctx->input);
			if (mailbox_save_continue(ctx->save_ctx) < 0) {
				/* we still have to finish reading the message
				   from client */
				mailbox_save_cancel(&ctx->save_ctx);
				break;
			}
			if (ret == -1 || ret == 0)
				break;
		}
	}

	if (ctx->save_ctx == NULL) {
		(void)i_stream_read(ctx->input);
		(void)i_stream_get_data(ctx->input, &size);
		i_stream_skip(ctx->input, size);
	}

	if (ctx->input->eof || client->input->closed) {
		bool all_written = ctx->input->v_offset == ctx->msg_size;

		/* finished */
		i_stream_unref(&ctx->input);
		ctx->input = NULL;

		if (ctx->save_ctx == NULL) {
			/* failed above */
			client_send_storage_error(cmd, ctx->storage);
			ctx->failed = TRUE;
		} else if (!all_written) {
			/* client disconnected before it finished sending the
			   whole message. */
			ctx->failed = TRUE;
			mailbox_save_cancel(&ctx->save_ctx);
			client_disconnect(client, "EOF while appending");
		} else if (mailbox_save_finish(&ctx->save_ctx) < 0) {
			ctx->failed = TRUE;
			client_send_storage_error(cmd, ctx->storage);
		}
		ctx->save_ctx = NULL;

		if (client->input->closed) {
			cmd_append_finish(ctx);
			return TRUE;
		}

		/* prepare for next message */
		ctx->message_input = FALSE;
		imap_parser_reset(ctx->save_parser);
		cmd->func = cmd_append_continue_parsing;
		return cmd_append_continue_parsing(cmd);
	}

	return FALSE;
}

static struct mailbox *
get_mailbox(struct client_command_context *cmd, const char *name)
{
	struct mail_namespace *ns;
	struct mailbox *box;
	enum mailbox_name_status status;
	const char *storage_name;

	ns = client_find_namespace(cmd, name, &storage_name, &status);
	if (ns == NULL)
		return NULL;

	switch (status) {
	case MAILBOX_NAME_EXISTS_MAILBOX:
		break;
	case MAILBOX_NAME_EXISTS_DIR:
		status = MAILBOX_NAME_VALID;
		/* fall through */
	case MAILBOX_NAME_VALID:
	case MAILBOX_NAME_INVALID:
	case MAILBOX_NAME_NOINFERIORS:
		client_fail_mailbox_name_status(cmd, name, "TRYCREATE", status);
		return NULL;
	}

	if (cmd->client->mailbox != NULL &&
	    mailbox_equals(cmd->client->mailbox, ns, storage_name))
		return cmd->client->mailbox;

	box = mailbox_alloc(ns->list, storage_name, MAILBOX_FLAG_SAVEONLY |
			    MAILBOX_FLAG_KEEP_RECENT);
	if (mailbox_open(box) < 0) {
		client_send_storage_error(cmd, mailbox_get_storage(box));
		mailbox_free(&box);
		return NULL;
	}
	if (cmd->client->enabled_features != 0)
		mailbox_enable(box, cmd->client->enabled_features);
	return box;
}

bool cmd_append(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
        struct cmd_append_context *ctx;
	const char *mailbox;

	if (client->syncing) {
		/* if transaction is created while its view is synced,
		   appends aren't allowed for it. */
		cmd->state = CLIENT_COMMAND_STATE_WAIT_UNAMBIGUITY;
		return FALSE;
	}

	/* <mailbox> */
	if (!client_read_string_args(cmd, 1, &mailbox))
		return FALSE;

	/* we keep the input locked all the time */
	client->input_lock = cmd;

	ctx = p_new(cmd->pool, struct cmd_append_context, 1);
	ctx->cmd = cmd;
	ctx->client = client;
	ctx->box = get_mailbox(cmd, mailbox);
	ctx->started = ioloop_time;
	if (ctx->box == NULL)
		ctx->failed = TRUE;
	else {
		ctx->storage = mailbox_get_storage(ctx->box);

		ctx->t = mailbox_transaction_begin(ctx->box,
					MAILBOX_TRANSACTION_FLAG_EXTERNAL |
					MAILBOX_TRANSACTION_FLAG_ASSIGN_UIDS);
	}

	io_remove(&client->io);
	client->io = io_add(i_stream_get_fd(client->input), IO_READ,
			    client_input_append, cmd);
	/* append is special because we're only waiting on client input, not
	   client output, so disable the standard output handler until we're
	   finished */
	o_stream_unset_flush_callback(client->output);

	ctx->save_parser = imap_parser_create(client->input, client->output,
					      client->set->imap_max_line_length);

	cmd->func = cmd_append_continue_parsing;
	cmd->context = ctx;
	return cmd_append_continue_parsing(cmd);
}

/* APPLE - rest of file - catenate
   Implements RFC 4469.

   Tried to weave CATENATE support into the existing code but was too messy.
   Chose to keep a separate state machine rather than rewrite it all.

   Enter at catenate_begin_parsing().  That iterates through all the
   in-line URLs.  If it encounters a URL literal, it continues at
   catenate_continue_url().  If it encounters a TEXT literal, it
   continues at catenate_continue_text().  Both of these flow to
   catenate_continue_parsing().  At the end, catenate_finish() passes
   control back to cmd_append_continue_parsing(). */

#define MAX_URL_LITERAL_SIZE	8000
#define MAX_CATENATE_MSG_SIZE	((uint32_t) -1)
#define MAX_CATENATE_PARTS	50
#ifndef PIPE_MAX
# define PIPE_MAX	5120
#endif

static bool catenate_process_args(struct client_command_context *cmd,
				  const struct imap_arg *args, bool nonsync);
static bool catenate_cancel_args(struct client_command_context *cmd,
				 const struct imap_arg *args, bool nonsync);
static bool catenate_cancel(struct cmd_append_context *ctx, bool nonsync);
static bool catenate_finish(struct cmd_append_context *ctx, bool cancel);
static bool catenate_finish_literal(struct client_command_context *cmd,
				    bool cancel);

static bool args_indicate_catenate(const struct imap_arg *args)
{
	while (!IMAP_ARG_IS_EOL(args)) {
		if (imap_arg_atom_equals(args, "CATENATE"))
			return TRUE;
		args++;
	}
	return FALSE;
}

static void catenate_solicit(struct cmd_append_context *ctx, bool nonsync)
{
	if (!nonsync) {
		o_stream_send(ctx->client->output, "+ OK\r\n", 6);
		o_stream_flush(ctx->client->output);
		o_stream_uncork(ctx->client->output);
		o_stream_cork(ctx->client->output);
	}
}

static bool catenate_url_validate(const struct imap_url_parts *parts,
				  const char **error)
{
	// user: absent; RFC 3501 "userid"
	if (parts->user != NULL) {
		*error = "user ID present; need relative URL";
		return FALSE;
	}

	// auth_type: absent; RFC 3501 "auth-type"
	if (parts->auth_type != NULL) {
		*error = "auth type present; need relative URL";
		return FALSE;
	}
		    
	// hostport: absent; RFC 1738 "hostport"
	if (parts->hostport != NULL) {
		*error = "server present; need relative URL";
		return FALSE;
	}

	// mailbox: optional; RFC 3501 "mailbox"
	if (parts->mailbox != NULL &&
	    !imap_url_mailbox_validate(parts->mailbox)) {
		*error = "invalid mailbox";
		return FALSE;
	}

	// uidvalidity: optional; RFC 3501 "nz-number"
	if (parts->uidvalidity != NULL &&
	    !imap_url_nz_number_validate(parts->uidvalidity)) {
		*error = "invalid uidvalidity";
		return FALSE;
	}

	// uid: mandatory; RFC 3501 "nz-number"
	if (parts->uid == NULL ||
	    !imap_url_nz_number_validate(parts->uid)) {
		*error = "missing or invalid uid";
		return FALSE;
	}

	// section: optional; RFC 2192 "section"
	if (parts->section != NULL &&
	    !imap_url_section_validate(parts->section)) {
		*error = "invalid section";
		return FALSE;
	}

	// expiration: absent; RFC 3339 "date-time"
	if (parts->expiration != NULL) {
		*error = "expiration present";
		return FALSE;
	}

	// access: absent; RFC 4467 "access"
	if (parts->access != NULL) {
		*error = "missing or invalid access ID";
		return FALSE;
	}

	// mechanism: absent
	if (parts->mechanism != NULL) {
		*error = "mechanism present";
		return FALSE;
	}

	// urlauth: absent
	if (parts->urlauth != NULL) {
		*error = "access token present";
		return FALSE;
	}

	return TRUE;
}

struct catenate_fetch_context {
	struct cmd_append_context *ctx;
	struct ostream *client_output;
	const char *url;
	const char *mailbox;
	struct mailbox *box;
	struct istream *input;
	struct imap_parser *parser;
	struct mail_search_args *search_args;
	struct imap_fetch_context *fetch_ctx;
};

static bool catenate_fetch_cleanup(struct catenate_fetch_context *cfctx,
				   bool toobig, const char *error)
{
	if (cfctx->fetch_ctx != NULL) {
		if (!cfctx->fetch_ctx->urlfetched) {
			if (error == NULL && !toobig)
				error = "message not found";
		}
		if (imap_fetch_deinit(cfctx->fetch_ctx) < 0)
			cfctx->fetch_ctx->failed = TRUE;
		if (cfctx->fetch_ctx->failed) {
			if (error == NULL && !toobig)
				error = "message fetch failed";
		}
		cfctx->fetch_ctx->urlfetch = FALSE;
	}
	if (cfctx->search_args != NULL)
		mail_search_args_unref(&cfctx->search_args);
	if (cfctx->parser != NULL)
		imap_parser_destroy(&cfctx->parser);
	if (cfctx->input != NULL)
		i_stream_unref(&cfctx->input);
	if (cfctx->box != NULL)
		mailbox_free(&cfctx->box);
	if (cfctx->client_output != NULL)
		cfctx->ctx->client->output = cfctx->client_output;
	cfctx->ctx->client->output_squelch = FALSE;

	if (toobig)
		client_send_tagline(cfctx->ctx->cmd,
				    "NO [TOOBIG] Resulting message too large");
	else if (error != NULL)
		client_send_tagline(cfctx->ctx->cmd,
				    t_strconcat("NO [BADURL ",
						imap_url_sanitize(cfctx->url),
						"] ", error, NULL));
	return error == NULL && !toobig;
}

static bool catenate_url(struct cmd_append_context *ctx, const char *url)
{
	struct imap_url_parts enc_parts, dec_parts;
	const char *error = NULL;
	struct catenate_fetch_context cfctx;
	struct mail_namespace *ns;
	struct mailbox_status status;
	string_t *fetch_text;
	const struct imap_arg *fetch_args = NULL, *next_arg = NULL;
	int fetch_ret, flush_ret, ret;
	bool header, toobig;
	uoff_t size;

	/* parse, decode, and validate the url */
	memset(&enc_parts, 0, sizeof enc_parts);
	memset(&dec_parts, 0, sizeof dec_parts);
	imap_url_parse(url, &enc_parts);
	if (!imap_url_decode(&enc_parts, &dec_parts, &error) ||
	    !catenate_url_validate(&dec_parts, &error)) {
		client_send_tagline(ctx->cmd,
				    t_strconcat("NO [BADURL ",
						imap_url_sanitize(url),
						"] ", error, NULL));
		return FALSE;
	}

	memset(&cfctx, 0, sizeof cfctx);
	cfctx.ctx = ctx;
	cfctx.url = url;

	ctx->cmd->client->output_squelch = TRUE;

	/* get the mailbox; if none in URL use the selected one */
	if (dec_parts.mailbox) {
		ns = client_find_namespace(ctx->cmd, dec_parts.mailbox,
					   &cfctx.mailbox, NULL);
		if (ns == NULL)
			return catenate_fetch_cleanup(&cfctx, FALSE,
						      "can't find storage");
	} else if (ctx->cmd->client->mailbox != NULL) {
		cfctx.mailbox = mailbox_get_name(ctx->cmd->client->mailbox);
		ns = mailbox_get_namespace(ctx->cmd->client->mailbox);
	} else
		return catenate_fetch_cleanup(&cfctx, FALSE,
					 "no mailbox specified or selected");
	cfctx.box = mailbox_alloc(ns->list, cfctx.mailbox,
			   MAILBOX_FLAG_READONLY | MAILBOX_FLAG_KEEP_RECENT);
	if (mailbox_open(cfctx.box) < 0) {
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "can't open mailbox");
	}

	/* verify uidvalidity */
	if (mailbox_sync(cfctx.box, MAILBOX_SYNC_FLAG_FULL_READ |
			 MAILBOX_SYNC_FLAG_FAST) < 0) {
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "can't sync mailbox");
	}

	mailbox_get_status(cfctx.box, STATUS_UIDVALIDITY, &status);
	if (dec_parts.uidvalidity != NULL &&
	    strtoul(dec_parts.uidvalidity, NULL, 10) != status.uidvalidity)
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "uidvalidity mismatch");

	/* fetch the data as in
		UID FETCH dec_parts.uid BODY.PEEK[dec_parts.section]
	   but with less output syntax */
	fetch_text = t_str_new(11 + (dec_parts.section ?
				     strlen(dec_parts.section) : 0));
	str_append(fetch_text, "BODY.PEEK[");
	if (dec_parts.section)
		str_append(fetch_text, dec_parts.section);
	str_append(fetch_text, "]");

	cfctx.input = i_stream_create_from_data(str_data(fetch_text),
						str_len(fetch_text));
	(void) i_stream_read(cfctx.input);

	cfctx.parser = imap_parser_create(cfctx.input, NULL, (size_t) -1);
	if (imap_parser_finish_line(cfctx.parser, 0, 0, &fetch_args) < 0)
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "can't parse fetch string");

	ctx->cmd->uid = TRUE;
	if (imap_search_get_uidset_arg(dec_parts.uid,
				       &cfctx.search_args, &error) < 0)
		return catenate_fetch_cleanup(&cfctx, FALSE, error);

	cfctx.fetch_ctx = imap_fetch_init(ctx->cmd, cfctx.box);
	if (cfctx.fetch_ctx == NULL)
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "can't init fetch");
	cfctx.fetch_ctx->search_args = cfctx.search_args;
	mail_search_args_ref(cfctx.search_args);
	cfctx.fetch_ctx->urlfetch = TRUE;

	if (!fetch_parse_args(cfctx.fetch_ctx, fetch_args, &next_arg))
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "can't parse fetch args");

	if (imap_fetch_begin(cfctx.fetch_ctx) != 0)
		return catenate_fetch_cleanup(&cfctx, FALSE,
					      "can't begin fetch");

	/* fix the plumbing:  imap_fetch_more() writes to
	   client->output, so set that to the pipe to mailbox_save()'s
	   input */
	cfctx.client_output = ctx->client->output;
	ctx->client->output = ctx->cat.output;

	fetch_ret = 0;
	flush_ret = 0;
	error = NULL;
	header = TRUE;
	toobig = FALSE;
	do {
		if (fetch_ret == 0)
			fetch_ret = imap_fetch_more(cfctx.fetch_ctx);
		else
			flush_ret = o_stream_flush(ctx->client->output);
		while ((ret = i_stream_read(ctx->input)) != 0 && ret != -1) {
			/* parse the header line that imap_fetch produces */
			if (header) {
				char *line = i_stream_next_line(ctx->input);
				if (line == NULL)
					continue;
				header = FALSE;

				size = 0;
				line = strchr(line, '{');
				if (line != NULL)
					size = strtoul(line + 1, NULL, 10);
				ctx->msg_size += size;
				if (ctx->msg_size > MAX_CATENATE_MSG_SIZE) {
					toobig = TRUE;
					ret = -1;
					break;
				}
			}

			/* feed the message data into the new message */
			if (mailbox_save_continue(ctx->save_ctx) < 0) {
				mailbox_save_cancel(&ctx->save_ctx);
				ret = -1;
				break;
			}
		}
		if (ret == -1) {
			if (!toobig)
				error = "fetch/save failed";
			break;
		}
	} while (fetch_ret == 0 || flush_ret == 0);

	return catenate_fetch_cleanup(&cfctx, toobig, error);
}

static bool catenate_continue_cancel_literal(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	size_t size;

	if (cmd->cancel)
		return catenate_finish(ctx, TRUE);

	(void) i_stream_read(ctx->cat.literal_input);
	(void) i_stream_get_data(ctx->cat.literal_input, &size);
	i_stream_skip(ctx->cat.literal_input, size);

	if (ctx->cat.literal_input->eof || client->input->closed) {
		/* finished, but there may be more URL or TEXT nonsync
		   literals in a continued CATENATE command */
		i_stream_unref(&ctx->cat.literal_input);
		ctx->cat.literal_input = NULL;

		if (cmd->client->input->closed || ctx->args != NULL)
			return catenate_finish(ctx, TRUE);

		return catenate_finish_literal(cmd, TRUE);
	}

	return FALSE;
}

static bool catenate_cancel(struct cmd_append_context *ctx, bool nonsync)
{
	ctx->failed = TRUE;

	if (!nonsync || ctx->args != NULL)
		return catenate_finish(ctx, TRUE);

	/* we have to read this -- and any following -- nonsynced
	   literals so we don't treat the literal data as commands. */
	i_assert(ctx->cat.literal_input == NULL);
	ctx->cat.literal_input = i_stream_create_limit(ctx->client->input,
						       ctx->cat.literal_size);

	ctx->message_input = TRUE;
	ctx->cmd->func = catenate_continue_cancel_literal;
	ctx->cmd->context = ctx;
	return catenate_continue_cancel_literal(ctx->cmd);
}

static bool catenate_finish(struct cmd_append_context *ctx, bool cancel)
{
	if (!cancel) {
		/* close the pipe and save any lingering output, which also
		   crucially passes EOF down through the mailbox_save stack */
		o_stream_close(ctx->cat.output);
		if (mailbox_save_continue(ctx->save_ctx) < 0 ||
		    mailbox_save_finish(&ctx->save_ctx) < 0) {
			ctx->failed = TRUE;
			client_send_storage_error(ctx->cmd, ctx->storage);
			cancel = TRUE;
		} else {
			ctx->save_ctx = NULL;
			++ctx->count;
		}

		if (ctx->args == NULL) {
			// eat CRLF after close of CATENATE list
			ctx->cmd->client->input_skip_line = TRUE;
		}
	}

	if (ctx->cat.output != NULL)
		o_stream_unref(&ctx->cat.output);
	if (ctx->cat.literal_input != NULL)
		i_stream_unref(&ctx->cat.literal_input);
	if (ctx->cat.literal_url != NULL)
		str_free(&ctx->cat.literal_url);
	if (ctx->input != NULL)
		i_stream_unref(&ctx->input);
	ctx->cat.parts = 0;

	if (ctx->msg_size == 0 || ctx->count == 0) {
		if (!ctx->failed)
			client_send_tagline(ctx->cmd,
					"NO Can't save a zero byte message.");
		ctx->failed = TRUE;
		cancel = TRUE;
	}

	ctx->message_input = FALSE;
	if (cancel) {
		if (ctx->args != NULL) {
			ctx->cmd->func = cmd_append_continue_parsing;
			return cmd_append_continue_parsing(ctx->cmd);
		}
		cmd_append_finish(ctx);
		return TRUE;
	} else {
		if (ctx->args == NULL)
			imap_parser_reset(ctx->save_parser);
		ctx->cmd->func = cmd_append_continue_parsing;
		return cmd_append_continue_parsing(ctx->cmd);
	}
}

static bool catenate_continue_parsing(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	const struct imap_arg *args, *listargs;
	int ret;
	bool literal, nonsync;

	if (cmd->cancel)
		return catenate_finish(ctx, TRUE);

	if (client->input_skip_line && !client_skip_line(client))
		return FALSE;
	ret = imap_parser_read_args(ctx->save_parser, 0,
				    IMAP_PARSE_FLAG_LITERAL_SIZE, &args);
	if (ret == -1) {
		if (!ctx->failed)
			client_send_command_error(cmd, NULL);
		ctx->failed = TRUE;
		return catenate_finish(ctx, TRUE);
	}
	if (ret < 0)
		return FALSE;

	// continuing the CATENATE list
	if (!imap_arg_get_list(args, &listargs))
		i_unreached();
	if (!IMAP_ARG_IS_EOL(&args[1])) {
		i_assert(ctx->args == NULL);
		ctx->args = &args[1];		// MULTIAPPEND
	}
	args = listargs;

	if (IMAP_ARG_IS_EOL(args)) {
		/* end of CATENATE list.  finish the message and return
		   to regular MULTIAPPEND processing */
		client->input_skip_line = ctx->args == NULL;
		return catenate_finish(ctx, client->input->closed ||
					    ctx->failed);
	}

	literal = imap_parser_get_literal_size(ctx->save_parser,
					       &ctx->cat.literal_size);
	client->input_skip_line = !literal;
	nonsync = imap_parser_has_nonsync_literal(ctx->save_parser);
	return ctx->failed ? catenate_cancel_args(cmd, args, nonsync) :
			     catenate_process_args(cmd, args, nonsync);
}

static bool catenate_finish_literal(struct client_command_context *cmd,
				    bool cancel)
{
	struct cmd_append_context *ctx = cmd->context;

	ctx->message_input = FALSE;

	// continue in the CATENATE list
	i_assert(ctx->args == NULL);
	imap_parser_reset(ctx->save_parser);
	imap_parser_open_list(ctx->save_parser);

	i_assert(!cancel || ctx->failed);
	cmd->func = catenate_continue_parsing;
	return catenate_continue_parsing(cmd);
}

static bool catenate_continue_url(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	int ret;
	const unsigned char *data;
	size_t size;

	if (cmd->cancel)
		return catenate_finish(ctx, TRUE);

	if (ctx->cat.literal_input == NULL) {
		ctx->cat.literal_input =
			i_stream_create_limit(client->input,
					      ctx->cat.literal_size);
		i_assert(ctx->cat.literal_url == NULL);
		ctx->cat.literal_url = str_new(cmd->pool,
					       ctx->cat.literal_size + 1);
		ctx->message_input = TRUE;
	}

	if (ctx->save_ctx != NULL) {
		while (ctx->cat.literal_input->v_offset !=
		       ctx->cat.literal_size) {
			ret = i_stream_read(ctx->cat.literal_input);
			data = i_stream_get_data(ctx->cat.literal_input,
						 &size);
			buffer_append(ctx->cat.literal_url, data, size);
			i_stream_skip(ctx->cat.literal_input, size);
			if (ret == -1 || ret == 0)
				break;
		}
	}

	if (ctx->save_ctx == NULL) {
		(void) i_stream_read(ctx->cat.literal_input);
		(void) i_stream_get_data(ctx->cat.literal_input, &size);
		i_stream_skip(ctx->cat.literal_input, size);
	}

	if (ctx->cat.literal_input->eof || client->input->closed) {
		bool all_written = ctx->cat.literal_input->v_offset ==
				   ctx->cat.literal_size;

		/* finished */
		i_stream_unref(&ctx->cat.literal_input);
		ctx->cat.literal_input = NULL;

		if (ctx->save_ctx == NULL) {
			/* failed above */
			ctx->failed = TRUE;
		} else if (!all_written) {
			/* client disconnected before it finished sending the
			   whole literal. */
			ctx->failed = TRUE;
			mailbox_save_cancel(&ctx->save_ctx);
			client_disconnect(client,
					  "EOF while sending URL literal");
		} else if (!catenate_url(ctx, str_c(ctx->cat.literal_url)))
			ctx->failed = TRUE;

		str_free(&ctx->cat.literal_url);
		ctx->cat.literal_url = NULL;

		if (client->input->closed)
			return catenate_finish(ctx, TRUE);

		/* done with this literal, resume the CATENATE list */
		return catenate_finish_literal(cmd, ctx->failed);
	}

	return FALSE;
}

static bool catenate_continue_text(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	int ret;
	size_t size;

	if (cmd->cancel)
		return catenate_finish(ctx, TRUE);

	if (ctx->cat.literal_input == NULL) {
		ctx->cat.literal_input =
			i_stream_create_limit(client->input,
					      ctx->cat.literal_size);
		ctx->message_input = TRUE;
	}

	if (ctx->save_ctx != NULL) {
		off_t send_ret = 1;
		int flush_ret = 0;

		do {
			if (send_ret > 0)
				send_ret = o_stream_send_istream(
						ctx->cat.output,
						ctx->cat.literal_input);
			else
				flush_ret = o_stream_flush(ctx->cat.output);
			while ((ret = i_stream_read(ctx->input)) != 0 &&
			       ret != -1) {
				if (mailbox_save_continue(ctx->save_ctx) < 0) {
					mailbox_save_cancel(&ctx->save_ctx);
					ret = -1;
					break;
				}
			}
			if (ret == -1)
				break;
		} while (send_ret > 0 || flush_ret == 0);
	}

	if (ctx->save_ctx == NULL) {
		(void) i_stream_read(ctx->cat.literal_input);
		(void) i_stream_get_data(ctx->cat.literal_input, &size);
		i_stream_skip(ctx->cat.literal_input, size);
	}

	if (ctx->cat.literal_input->eof || client->input->closed) {
		bool all_written = ctx->cat.literal_input->v_offset ==
				   ctx->cat.literal_size;

		/* finished */
		i_stream_unref(&ctx->cat.literal_input);
		ctx->cat.literal_input = NULL;

		if (ctx->save_ctx == NULL) {
			/* failed above */
			client_send_storage_error(cmd, ctx->storage);
			ctx->failed = TRUE;
		} else if (!all_written) {
			/* client disconnected before it finished sending the
			   whole literal. */
			ctx->failed = TRUE;
			mailbox_save_cancel(&ctx->save_ctx);
			client_disconnect(client, "EOF while appending");
		}

		if (client->input->closed)
			return catenate_finish(ctx, TRUE);

		/* done with this literal, resume the CATENATE list */
		return catenate_finish_literal(cmd, ctx->failed);
	}

	return FALSE;
}

static bool catenate_begin_parsing(struct client_command_context *cmd,
				   const struct imap_arg *args)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	const struct imap_arg *flags_list, *listargs;
	bool literal, nonsync;
	enum mail_flags flags = 0;
	const char *str;
	struct mail_keywords *keywords = NULL;
	time_t internal_date = (time_t) -1;
	int ret, timezone_offset = 0;
	int fds[2];

	literal = imap_parser_get_literal_size(ctx->save_parser,
					       &ctx->cat.literal_size);
	client->input_skip_line = !literal;
	nonsync = imap_parser_has_nonsync_literal(ctx->save_parser);

	if (ctx->failed)
		return catenate_cancel(ctx, nonsync);

	/* [<flags>] */
	if (imap_arg_get_list(args, &flags_list)) {
		const char *const *keywords_list = NULL;
		++args;

		if (!client_parse_mail_flags(cmd, flags_list,
					     &flags, &keywords_list))
			return catenate_cancel(ctx, nonsync);
		if (keywords_list != NULL &&
		    mailbox_keywords_create(ctx->box, keywords_list,
					    &keywords) < 0) {
			client_send_storage_error(cmd, ctx->storage);
			return catenate_cancel(ctx, nonsync);
		}
	}

	/* [<internal date>]
	   MUST be a STRING only, no atom or literal */
	if (args->type == IMAP_ARG_STRING && imap_arg_get_string(args, &str)) {
		++args;

		if (!imap_parse_datetime(str, &internal_date,
					 &timezone_offset)) {
			client_send_tagline(cmd, "BAD Invalid internal date.");
			if (keywords != NULL)
				mailbox_keywords_unref(ctx->box, &keywords);
			return catenate_cancel(ctx, nonsync);
		}

		if (internal_date != (time_t)-1 &&
		    internal_date > ioloop_time + INTERNALDATE_MAX_FUTURE_SECS) {
			internal_date = (time_t)-1;
			timezone_offset = 0;
		}
	}

	if (!imap_arg_atom_equals(args, "CATENATE") ||
	    !imap_arg_get_list(&args[1], &listargs)) {
		// do not use client_send_command_error()
		client_send_tagline(cmd, "BAD Invalid arguments.");
		if (keywords != NULL)
			mailbox_keywords_unref(ctx->box, &keywords);
		return catenate_cancel(ctx, nonsync);
	}

	/* MULTIAPPEND syntax on a single line, e.g.:
	   APPEND <mailbox> CATENATE (URL <url>) CATENATE (... */
	if (!IMAP_ARG_IS_EOL(&args[2]))
		ctx->args = &args[2];

	// remainder of CATENATE parsing cares only about the items in the list
	args = listargs;
	if (IMAP_ARG_IS_EOL(args)) {	// empty list
		client_send_tagline(cmd, "BAD Invalid arguments.");
		if (keywords != NULL)
			mailbox_keywords_unref(ctx->box, &keywords);
		return catenate_cancel(ctx, nonsync);
	}

	/* The mailbox_save() API pulls data from an istream, and the
	   imap_fetch() API pushes data into an ostream.  Dovecot does
	   not offer an ostream which feeds an istream, so use a UNIX
	   pipe.  That's not the most efficient way to move the data
	   around, but dovecot's ostreams and istreams are sufficiently
	   complex (read: under-documented) that it's easier to use a
	   pipe than to write an ostream-to-istream module.  Somebody
	   call a plumber. */
	if (pipe(fds) < 0) {
		i_error("catenate_begin_parsing: pipe: %m");
		client_send_tagline(cmd, "NO "MAIL_ERRSTR_CRITICAL_MSG);
		if (keywords != NULL)
			mailbox_keywords_unref(ctx->box, &keywords);
		return catenate_cancel(ctx, nonsync);
	}
	fd_set_nonblock(fds[0], TRUE);
	fd_set_nonblock(fds[1], TRUE);
	ctx->cat.output = o_stream_create_fd(fds[1], PIPE_MAX, TRUE);
	ctx->input = i_stream_create_fd(fds[0], PIPE_MAX, TRUE);

	/* start assembling the message */
	ctx->save_ctx = mailbox_save_alloc(ctx->t);
	mailbox_save_set_flags(ctx->save_ctx, flags, keywords);
	mailbox_save_set_received_date(ctx->save_ctx,
				       internal_date, timezone_offset);
	ret = mailbox_save_begin(&ctx->save_ctx, ctx->input);

	if (keywords != NULL)
		mailbox_keywords_unref(ctx->box, &keywords);

	if (ret < 0) {
		/* save initialization failed */
		client_send_storage_error(cmd, ctx->storage);
		return catenate_cancel(ctx, nonsync);
	}
	ctx->msg_size = 0;

	return catenate_process_args(cmd, args, nonsync);
}

static bool catenate_begin_cancel(struct client_command_context *cmd,
				  const struct imap_arg *args)
{
	struct client *client = cmd->client;
	struct cmd_append_context *ctx = cmd->context;
	const struct imap_arg *listargs;
	bool literal, nonsync;

	literal = imap_parser_get_literal_size(ctx->save_parser,
					       &ctx->cat.literal_size);
	client->input_skip_line = !literal;
	nonsync = imap_parser_has_nonsync_literal(ctx->save_parser);

	/* [<flags>] */
	if (args->type == IMAP_ARG_LIST)
		++args;

	/* [<internal date>] */
	if (args->type == IMAP_ARG_STRING)
		++args;

	if (!imap_arg_atom_equals(args, "CATENATE") ||
	    !imap_arg_get_list(&args[1], &listargs))
		return catenate_cancel(ctx, nonsync);

	/* MULTIAPPEND syntax on a single line, e.g.:
	   APPEND <mailbox> CATENATE (URL <url>) CATENATE (... */
	if (!IMAP_ARG_IS_EOL(&args[2]))
		ctx->args = &args[2];

	// remainder of CATENATE parsing cares only about the items in the list
	args = listargs;
	if (IMAP_ARG_IS_EOL(args))		// empty list
		return catenate_cancel(ctx, nonsync);

	return catenate_cancel_args(cmd, args, nonsync);
}

static bool catenate_process_args(struct client_command_context *cmd,
				  const struct imap_arg *args, bool nonsync)
{
	struct cmd_append_context *ctx = cmd->context;

	/* [URL <url>] ... [URL <literal>] */
	while (imap_arg_atom_equals(args, "URL")) {
		if (++ctx->cat.parts > MAX_CATENATE_PARTS) {
			client_send_tagline(cmd,
					    "BAD Too many message parts.");
			return catenate_cancel(ctx, nonsync);
		}

		++args;
		if (args->type == IMAP_ARG_ATOM ||
		    args->type == IMAP_ARG_STRING) {
			if (!catenate_url(ctx, args->_data.str))
				return catenate_cancel(ctx, nonsync);
			++args;
		} else if (args->type == IMAP_ARG_LITERAL_SIZE ||
			   args->type == IMAP_ARG_LITERAL_SIZE_NONSYNC) {
			if (ctx->cat.literal_size > MAX_URL_LITERAL_SIZE) {
				client_send_tagline(cmd,
						"BAD URL literal too large");
				return catenate_cancel(ctx, nonsync);
			}

			catenate_solicit(ctx, nonsync);
			cmd->func = catenate_continue_url;
			return catenate_continue_url(cmd);
		} else {
			client_send_tagline(cmd, "BAD Invalid arguments.");
			return catenate_cancel(ctx, nonsync);
		}
	}

	/* [TEXT <literal>] */
	if (imap_arg_atom_equals(args, "TEXT") &&
	    (args[1].type == IMAP_ARG_LITERAL_SIZE ||
	     args[1].type == IMAP_ARG_LITERAL_SIZE_NONSYNC)) {
		if (++ctx->cat.parts > MAX_CATENATE_PARTS) {
			client_send_tagline(cmd,
					    "BAD Too many message parts.");
			return catenate_cancel(ctx, nonsync);
		}

		ctx->msg_size += ctx->cat.literal_size;
		if (ctx->msg_size > MAX_CATENATE_MSG_SIZE) {
			client_send_tagline(cmd,
				"NO [TOOBIG] Resulting message too large");
			return catenate_cancel(ctx, nonsync);
		}

		catenate_solicit(ctx, nonsync);
		cmd->func = catenate_continue_text;
		return catenate_continue_text(cmd);
	} else if (!IMAP_ARG_IS_EOL(args)) {
		client_send_tagline(cmd, "BAD Invalid arguments.");
		return catenate_cancel(ctx, nonsync);
	}

	return catenate_finish(ctx, FALSE);
}

static bool catenate_cancel_args(struct client_command_context *cmd,
				 const struct imap_arg *args, bool nonsync)
{
	struct cmd_append_context *ctx = cmd->context;

	/* [URL <url>] ... [URL <literal>] */
	while (imap_arg_atom_equals(args, "URL")) {
		++args;
		if (args->type == IMAP_ARG_ATOM ||
		    args->type == IMAP_ARG_STRING)
			++args;
		else if (args->type == IMAP_ARG_LITERAL_SIZE ||
			 args->type == IMAP_ARG_LITERAL_SIZE_NONSYNC)
			return catenate_cancel(ctx, nonsync);
	}

	/* [TEXT <literal>] */
	if (imap_arg_atom_equals(args, "TEXT") &&
	    (args[1].type == IMAP_ARG_LITERAL_SIZE ||
	     args[1].type == IMAP_ARG_LITERAL_SIZE_NONSYNC))
		return catenate_cancel(ctx, nonsync);
	else if (!IMAP_ARG_IS_EOL(args))
		return catenate_cancel(ctx, nonsync);

	return catenate_finish(ctx, TRUE);
}
