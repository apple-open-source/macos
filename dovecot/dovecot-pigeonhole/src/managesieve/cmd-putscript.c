/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* NOTE: this file also contains the checkscript command due to its obvious
 * similarities.
 */

#include "lib.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "str.h"

#include "sieve.h"

#include "sieve-storage.h"
#include "sieve-storage-script.h"
#include "sieve-storage-save.h"

#include "managesieve-parser.h"

#include "managesieve-common.h"
#include "managesieve-commands.h"
#include "managesieve-quota.h"

#include <sys/time.h>

struct cmd_putscript_context {
	struct client *client;
	struct client_command_context *cmd;
	struct sieve_storage *storage;

	struct istream *input;

	const char *scriptname;
	uoff_t script_size;	

	struct managesieve_parser *save_parser;
	struct sieve_save_context *save_ctx;
};

static void cmd_putscript_finish(struct cmd_putscript_context *ctx);
static bool cmd_putscript_continue_script(struct client_command_context *cmd);

static void client_input_putscript(void *context)
{
	struct client *client = context;
	struct client_command_context *cmd = &client->cmd;

	i_assert(!client->destroyed);

	client->last_input = ioloop_time;
	timeout_reset(client->to_idle);

	switch (i_stream_read(client->input)) {
	case -1:
		/* disconnected */
		cmd_putscript_finish(cmd->context);
		/* Reset command so that client_destroy() doesn't try to call
		   cmd_putscript_continue_script() anymore. */
		_client_reset_command(client);
		client_destroy(client, "Disconnected in PUTSCRIPT/SCRIPT");
		return;
	case -2:
		cmd_putscript_finish(cmd->context);
		if (client->command_pending) {
			/* uploaded script data, this is handled internally by
			   mailbox_save_continue() */
			break;
		}

		/* parameter word is longer than max. input buffer size.
		   this is most likely an error, so skip the new data
		   until newline is found. */
		client->input_skip_line = TRUE;

		client_send_command_error(cmd, "Too long argument.");
		cmd->param_error = TRUE;
		_client_reset_command(client);
		return;
	}

	if (cmd->func(cmd)) {
		/* command execution was finished. Note that if cmd_sync()
		   didn't finish, we didn't get here but the input handler
		   has already been moved. So don't do anything important
		   here..

		   reset command once again to reset cmd_sync()'s changes. */
		_client_reset_command(client);

		if (client->input_pending)
			client_input(client);
	}
}

static void cmd_putscript_finish(struct cmd_putscript_context *ctx)
{
	managesieve_parser_destroy(&ctx->save_parser);
	
	io_remove(&ctx->client->io);
	o_stream_set_flush_callback(ctx->client->output,
				    client_output, ctx->client);

	if (ctx->input != NULL)
		i_stream_unref(&ctx->input);

	if (ctx->save_ctx != NULL)
	{
		ctx->client->input_skip_line = TRUE;
		sieve_storage_save_cancel(&ctx->save_ctx);
	}
}

static bool cmd_putscript_continue_cancel(struct client_command_context *cmd)
{
	struct cmd_putscript_context *ctx = cmd->context;
	size_t size;

	(void)i_stream_read(ctx->input);
	(void)i_stream_get_data(ctx->input, &size);
	i_stream_skip(ctx->input, size);

	if ( cmd->client->input->closed ||
		ctx->input->v_offset == ctx->script_size ) {
		cmd_putscript_finish(ctx);
		return TRUE;
	}
	return FALSE;
}

static bool cmd_putscript_cancel(struct cmd_putscript_context *ctx, bool nonsync)
{
	ctx->client->input_skip_line = TRUE;

	if (!nonsync) { /* Rediculous for managesieve */
		cmd_putscript_finish(ctx);
		return TRUE;
	}

	/* we have to read the nonsynced literal so we don't treat the uploaded script
	   as commands. */
	ctx->input = i_stream_create_limit(ctx->client->input, ctx->script_size);

	ctx->client->command_pending = TRUE;
	ctx->cmd->func = cmd_putscript_continue_cancel;
	ctx->cmd->context = ctx;
	return cmd_putscript_continue_cancel(ctx->cmd);
}

static bool cmd_putscript_finish_parsing(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_putscript_context *ctx = cmd->context;
	struct managesieve_arg *args;
	int ret;
	
	/* if error occurs, the CRLF is already read. */
	client->input_skip_line = FALSE;
	
	/* <script literal> */
	ret = managesieve_parser_read_args(ctx->save_parser, 0,
          MANAGESIEVE_PARSE_FLAG_LITERAL_SIZE, &args);
	if (ret == -1 || client->output->closed) {
		if (ctx->storage != NULL)
			client_send_command_error(cmd, NULL);
		cmd_putscript_finish(ctx);
		return TRUE;
	}
	if (ret < 0) {
		/* need more data */
		return FALSE;
	}

	if (args[0].type == MANAGESIEVE_ARG_EOL) {
		struct sieve_script *script;

		/* Last (and only) script */
		bool success = TRUE;

		/* Eat away the trailing CRLF */
		client->input_skip_line = TRUE;

		/* Obtain script object for uploaded script */
		script = sieve_storage_save_get_tempscript(ctx->save_ctx);

		/* Check result */
		if ( script == NULL ) {
			client_send_storage_error(client, ctx->storage);
			cmd_putscript_finish(ctx);
			return TRUE;
		}

		/* Try to compile script */
		T_BEGIN {
			struct sieve_error_handler *ehandler;
			struct sieve_binary *sbin;
			string_t *errors;

			/* Prepare error handler */
			errors = str_new(default_pool, 1024);
			ehandler = sieve_strbuf_ehandler_create(client->svinst, errors, TRUE, 
				client->set->managesieve_max_compile_errors);

			/* Compile */
			if ( (sbin=sieve_compile_script(script, ehandler, NULL)) == NULL ) {
				client_send_no(client, str_c(errors));
				success = FALSE;
			} else {
				sieve_close(&sbin);

				/* Commit to save only when this is a putscript command */
				if ( ctx->scriptname != NULL ) { 
					ret = sieve_storage_save_commit(&ctx->save_ctx);

					/* Check commit */			
					if (ret < 0) {
						client_send_storage_error(client, ctx->storage);
						success = FALSE;
					}
				} 
			}

			/* Finish up */
			cmd_putscript_finish(ctx);

			/* Report result to user */
			if ( success ) {
				if ( sieve_get_warnings(ehandler) > 0 ) 
					client_send_okresp(client, "WARNINGS", str_c(errors));
				else {
					if ( ctx->scriptname != NULL )
						client_send_ok(client, "PUTSCRIPT completed.");
					else
						client_send_ok(client, "Script checked successfully.");						
				}
			}

			sieve_error_handler_unref(&ehandler);
			str_free(&errors);
		} T_END;

		return TRUE;
	}

	client_send_command_error(cmd, "Too many command arguments.");
	cmd_putscript_finish(ctx);
	return TRUE;
}

static bool cmd_putscript_continue_parsing(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_putscript_context *ctx = cmd->context;
	struct managesieve_arg *args;
	bool nonsync = FALSE;
	int ret;

	/* if error occurs, the CRLF is already read. */
	client->input_skip_line = FALSE;

	/* <script literal> */
	ret = managesieve_parser_read_args(ctx->save_parser, 0,
				    MANAGESIEVE_PARSE_FLAG_LITERAL_SIZE, &args);
	if (ret == -1 || client->output->closed) {
		cmd_putscript_finish(ctx);
		client_send_command_error(cmd, "Invalid arguments.");
		client->input_skip_line = TRUE;
		return TRUE;
	}
	if (ret < 0) {
		/* need more data */
		return FALSE;
	}

	if (args->type != MANAGESIEVE_ARG_STRING) {
		/* Validate the script argument */
	  	if (args->type != MANAGESIEVE_ARG_LITERAL_SIZE ) {
			client_send_command_error(cmd, "Invalid arguments.");
			return cmd_putscript_cancel(ctx, FALSE);
		}

		ctx->script_size = MANAGESIEVE_ARG_LITERAL_SIZE(args);
		nonsync = TRUE;
	} else {
		/* FIXME: allow quoted strings */
		client_send_no(client, 
			"This MANAGESIEVE implementation currently does not allow "
			"quoted strings to be used for script contents.");
		return cmd_putscript_cancel(ctx, FALSE);		
	}

	if ( ctx->script_size == 0 ) {
		/* no script content, abort */
		if ( ctx->scriptname != NULL ) 
			client_send_no(client, "PUTSCRIPT aborted (empty script).");
		else
			client_send_no(client, "CHECKSCRIPT aborted (empty script).");

		cmd_putscript_finish(ctx);	
		return TRUE;	
	}

	/* save the script */
	ctx->input = i_stream_create_limit(client->input, ctx->script_size);
	ctx->save_ctx = sieve_storage_save_init
		(ctx->storage, ctx->scriptname, ctx->input);

	if ( ctx->save_ctx == NULL ) {
		/* save initialization failed */
		client_send_storage_error(client, ctx->storage);
		return cmd_putscript_cancel(ctx, nonsync);
	}

	/* Check quota */
	if ( ctx->scriptname == NULL ) {
		if ( !managesieve_quota_check_validsize(client, ctx->script_size) )
			return cmd_putscript_cancel(ctx, nonsync);
	} else {
		if ( !managesieve_quota_check_all
			(client, ctx->scriptname, ctx->script_size) )
			return cmd_putscript_cancel(ctx, nonsync);
	}

	/* after literal comes CRLF, if we fail make sure we eat it away */
	client->input_skip_line = TRUE;

	client->command_pending = TRUE;
	cmd->func = cmd_putscript_continue_script;
	return cmd_putscript_continue_script(cmd);
}

static bool cmd_putscript_continue_script(struct client_command_context *cmd)
{
	struct client *client = cmd->client;
	struct cmd_putscript_context *ctx = cmd->context;
	size_t size;
	bool failed;
	int ret;

	if (ctx->save_ctx != NULL) {
		while (ctx->input->v_offset != ctx->script_size) {
			ret = i_stream_read(ctx->input);
			if (sieve_storage_save_continue(ctx->save_ctx) < 0) {
				/* we still have to finish reading the script
			   	  from client */
				sieve_storage_save_cancel(&ctx->save_ctx);
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
		bool all_written = ctx->input->v_offset == ctx->script_size;

		/* finished */
		i_stream_unref(&ctx->input);
		ctx->input = NULL;

		if (ctx->save_ctx == NULL) {
			/* failed above */
			client_send_storage_error(client, ctx->storage);
			failed = TRUE;
		} else if (!all_written) {
			/* client disconnected before it finished sending the
			   whole script. */
			failed = TRUE;
			sieve_storage_save_cancel(&ctx->save_ctx);
			client_disconnect(client, "EOF while appending in PUTSCRIPT/CHECKSCRIPT");
		} else if (sieve_storage_save_finish(ctx->save_ctx) < 0) {
			failed = TRUE;
			client_send_storage_error(client, ctx->storage);
		} else {
			failed = client->input->closed;
		}

		if (failed) {
			cmd_putscript_finish(ctx);
			return TRUE;
		}

		/* finish */
		client->command_pending = FALSE;
		managesieve_parser_reset(ctx->save_parser);
		cmd->func = cmd_putscript_finish_parsing;
		return cmd_putscript_finish_parsing(cmd);
	}

	return FALSE;
}

static bool cmd_putscript_start
(struct client_command_context *cmd, const char *scriptname)
{
	struct cmd_putscript_context *ctx;
	struct client *client = cmd->client;

	ctx = p_new(cmd->pool, struct cmd_putscript_context, 1);
	ctx->cmd = cmd;
	ctx->client = client;
	ctx->storage = client->storage;
	ctx->scriptname = scriptname;

	io_remove(&client->io);
	client->io = io_add(i_stream_get_fd(client->input), IO_READ,
			    client_input_putscript, client);
	/* putscript is special because we're only waiting on client input, not
	   client output, so disable the standard output handler until we're
	   finished */
	o_stream_unset_flush_callback(client->output);

	ctx->save_parser = managesieve_parser_create(client->input, client->output,
					      client->set->managesieve_max_line_length);

	cmd->func = cmd_putscript_continue_parsing;
	cmd->context = ctx;
	return cmd_putscript_continue_parsing(cmd);

}

bool cmd_putscript(struct client_command_context *cmd)
{
	const char *scriptname;

	/* <scriptname> */
	if ( !client_read_string_args(cmd, 1, FALSE, &scriptname) )
		return FALSE;

	return cmd_putscript_start(cmd, scriptname);
}

bool cmd_checkscript(struct client_command_context *cmd)
{
	return cmd_putscript_start(cmd, NULL);
}
