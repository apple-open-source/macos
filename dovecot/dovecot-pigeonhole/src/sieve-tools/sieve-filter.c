/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "lib-signals.h"
#include "ioloop.h"
#include "env-util.h"
#include "str.h"
#include "str-sanitize.h"
#include "ostream.h"
#include "array.h"
#include "mail-namespace.h"
#include "mail-storage.h"
#include "mail-search-build.h"
#include "imap-utf7.h"

#include "sieve.h"
#include "sieve-extensions.h"
#include "sieve-binary.h"

#include "sieve-tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sysexits.h>

/*
 * Print help
 */

static void print_help(void)
{
	printf(
"Usage: sieve-filter [-c <config-file>] [-C] [-D] [-e] [-m <default-mailbox>]\n"
"                    [-P <plugin>] [-q <output-mailbox>] [-Q <mail-command>]\n"
"                    [-s <script-file>] [-u <user>] [-W] [-x <extensions>]\n"
"                    <script-file> <source-mailbox> <source-action>\n"
	);
}

enum sieve_filter_source_action {
	SIEVE_FILTER_SACT_KEEP,          /* Always keep messages in source folder */ 
	SIEVE_FILTER_SACT_MOVE,          /* Move discarded messages to Trash folder */      
	SIEVE_FILTER_SACT_DELETE,        /* Flag discarded messages as \DELETED */
	SIEVE_FILTER_SACT_EXPUNGE        /* Expunge discarded messages */
};

struct sieve_filter_data {
	enum sieve_filter_source_action source_action;
	struct mailbox *move_mailbox;

	struct sieve_script_env *senv;
	struct sieve_binary *main_sbin;
	struct sieve_error_handler *ehandler;

	unsigned int execute:1;
	unsigned int source_write:1;
	unsigned int move_mode:1;
};

struct sieve_filter_context {
	const struct sieve_filter_data *data;

	struct mailbox_transaction_context *move_trans;

	struct ostream *teststream;
};

static int filter_message
(struct sieve_filter_context *sfctx, struct mail *mail)
{
	struct sieve_error_handler *ehandler = sfctx->data->ehandler; 
	struct sieve_script_env *senv = sfctx->data->senv;
	struct sieve_exec_status estatus;
	struct sieve_binary *sbin;
	struct sieve_message_data msgdata;
	const char *recipient, *sender;
	bool execute = sfctx->data->execute;
	bool source_write = sfctx->data->source_write;
	bool move_mode = sfctx->data->move_mode;
	int ret;

	sieve_tool_get_envelope_data(mail, &recipient, &sender);

	/* Initialize execution status */
	memset(&estatus, 0, sizeof(estatus));
	senv->exec_status = &estatus;

	/* Collect necessary message data */
	memset(&msgdata, 0, sizeof(msgdata));
	msgdata.mail = mail;
	msgdata.return_path = sender;
	msgdata.orig_envelope_to = recipient;
	msgdata.final_envelope_to = recipient;
	msgdata.auth_user = senv->username;
	(void)mail_get_first_header(mail, "Message-ID", &msgdata.id);

	/* Single script */
	sbin = sfctx->data->main_sbin;

	/* Execute script */
	if ( execute ) {
		ret = sieve_execute(sbin, &msgdata, senv, ehandler, NULL);
	} else {
		ret = sieve_test
			(sbin, &msgdata, senv, ehandler, sfctx->teststream, NULL);
	}

	/* Handle message in source folder */
	if ( ret > 0 ) {
		struct mailbox *move_box = sfctx->data->move_mailbox;
		enum sieve_filter_source_action source_action =
			sfctx->data->source_action;

		if ( !source_write ) {
			/* Do nothing */

		} else if ( estatus.keep_original  ) {
			sieve_info(ehandler, NULL, "message kept in source mailbox");

		} else if ( move_mode && estatus.message_saved ) {
			sieve_info(ehandler, NULL,
				"message expunged from source mailbox upon successful move");

			if ( execute )
				mail_expunge(mail);

		} else {
			switch ( source_action ) {
			/* Leave it there */
			case SIEVE_FILTER_SACT_KEEP:
				sieve_info(ehandler, NULL, "message left in source mailbox");
				break;
			/* Move message to indicated folder */
			case SIEVE_FILTER_SACT_MOVE:			
				sieve_info(ehandler, NULL, 
					"message in source mailbox moved to mailbox '%s'",
					mailbox_get_name(move_box));

				if ( execute && move_box != NULL ) {
					struct mailbox_transaction_context *t = sfctx->move_trans;
					struct mail_save_context *save_ctx;
	
				    save_ctx = mailbox_save_alloc(t);

					if ( mailbox_copy(&save_ctx, mail) < 0 ) {
						enum mail_error error;
						const char *errstr;
	
						errstr = mail_storage_get_last_error
							(mailbox_get_storage(move_box), &error);

						sieve_error(ehandler, NULL,
							"failed to move message to mailbox %s: %s",
							mailbox_get_name(move_box), errstr);
						return -1;
				    }
			
					mail_expunge(mail);
				}
				break;
			/* Flag message as \DELETED */
			case SIEVE_FILTER_SACT_DELETE:					
				sieve_info(ehandler, NULL, "message flagged as deleted in source mailbox");
				if ( execute )
					mail_update_flags(mail, MODIFY_ADD, MAIL_DELETED);
				break;
			/* Expunge the message immediately */
			case SIEVE_FILTER_SACT_EXPUNGE:
				sieve_info(ehandler, NULL, "message expunged from source mailbox");
				if ( execute )
					mail_expunge(mail);
				break;
			/* Unknown */
			default:
				i_unreached();
				break;
			}
		}
	}

	return ret;
}

/* FIXME: introduce this into Dovecot */
static void mail_search_build_add_flags
(struct mail_search_args *args, enum mail_flags flags, bool not)
{
	struct mail_search_arg *arg;

	arg = p_new(args->pool, struct mail_search_arg, 1);
	arg->type = SEARCH_FLAGS;
	arg->value.flags = flags;
	arg->not = not;

	arg->next = args->args;
	args->args = arg;
}

static int filter_mailbox
(const struct sieve_filter_data *sfdata, struct mailbox *src_box)
{
	struct sieve_filter_context sfctx;
	struct mailbox *move_box = sfdata->move_mailbox;
	struct sieve_error_handler *ehandler = sfdata->ehandler;
	struct mail_search_args *search_args;
	struct mailbox_transaction_context *t;
	struct mail_search_context *search_ctx;
	struct mail *mail;
	int ret = 1;

	/* Sync source mailbox */

	if ( mailbox_sync(src_box, MAILBOX_SYNC_FLAG_FULL_READ) < 0 ) {
		sieve_error(ehandler, NULL, "failed to sync source mailbox");
		return -1;
	}

	/* Initialize */

	memset(&sfctx, 0, sizeof(sfctx));
	sfctx.data = sfdata;

	/* Create test stream */
	if ( !sfdata->execute )
		sfctx.teststream = o_stream_create_fd(1, 0, FALSE);

	/* Start move mailbox transaction */

	if ( move_box != NULL ) {
		sfctx.move_trans = mailbox_transaction_begin
			(move_box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
	}

	/* Search non-deleted messages in the source folder */

	search_args = mail_search_build_init();
	mail_search_build_add_flags(search_args, MAIL_DELETED, TRUE);

	t = mailbox_transaction_begin(src_box, 0);
	search_ctx = mailbox_search_init(t, search_args, NULL);
	mail_search_args_unref(&search_args);

	/* Iterate through all requested messages */

	mail = mail_alloc(t, 0, NULL);
	while ( ret > 0 && mailbox_search_next(search_ctx, mail) > 0 ) {
		const char *subject, *date;
		uoff_t size = 0;
					
		/* Request message size */

		if ( mail_get_virtual_size(mail, &size) < 0 ) {
			if ( mail->expunged )
				continue;
			
			sieve_error(ehandler, NULL, "failed to obtain message size");
			continue;
		}

		if ( mail_get_first_header(mail, "date", &date) <= 0 )
			date = "";
		if ( mail_get_first_header(mail, "subject", &subject) <= 0 ) 
			subject = "";
		
		sieve_info(ehandler, NULL,
			"filtering: [%s; %"PRIuUOFF_T" bytes] %s", date, size,
			str_sanitize(subject, 40));
	
		ret = filter_message(&sfctx, mail);
	}
	mail_free(&mail);
	
	/* Cleanup */

	if ( mailbox_search_deinit(&search_ctx) < 0 ) {
		ret = -1;
	}

	if ( sfctx.move_trans != NULL ) {
		if ( ret < 0 ) {
			mailbox_transaction_rollback(&sfctx.move_trans);
		} else {
			if ( mailbox_transaction_commit(&sfctx.move_trans) < 0 ) {
				ret = -1;
			}
		}
	}

	if ( ret < 0 ) {
		mailbox_transaction_rollback(&t);
	} else {
		if ( mailbox_transaction_commit(&t) < 0 ) {
			ret = -1;
		}
	}

	if ( sfctx.teststream != NULL )
		o_stream_destroy(&sfctx.teststream);

	if ( ret < 0 ) return ret;

	/* Sync mailbox */

	if ( sfdata->execute ) {
		if ( mailbox_sync(src_box, MAILBOX_SYNC_FLAG_FULL_WRITE) < 0 ) {
			sieve_error(ehandler, NULL, "failed to sync source mailbox");
			return -1;
		}
	}
	
	return ret;
}

static const char *mailbox_name_to_mutf7(const char *mailbox_utf8)
{
	string_t *str = t_str_new(128);

	if (imap_utf8_to_utf7(mailbox_utf8, str) < 0)
		return mailbox_utf8;
	else
		return str_c(str);
}

/*
 * Tool implementation
 */

int main(int argc, char **argv) 
{
	struct sieve_instance *svinst;
	ARRAY_TYPE (const_string) scriptfiles;
	const char *scriptfile,	*src_mailbox, *dst_mailbox, *move_mailbox;
	struct sieve_filter_data sfdata;
	enum sieve_filter_source_action source_action = SIEVE_FILTER_SACT_KEEP;
	struct mail_user *mail_user;
	struct sieve_binary *main_sbin;
	struct sieve_script_env scriptenv;
	struct sieve_error_handler *ehandler;
	bool force_compile, execute, source_write, move_mode;
	struct mail_namespace *ns;
	struct mailbox *src_box = NULL, *move_box = NULL;
	enum mailbox_flags open_flags =
		MAILBOX_FLAG_KEEP_RECENT | MAILBOX_FLAG_IGNORE_ACLS;
	enum mail_error error;
	int c;

	sieve_tool = sieve_tool_init("sieve-filter", &argc, &argv, 
		"m:s:x:P:u:q:Q:DCeWM", FALSE);

	t_array_init(&scriptfiles, 16);

	/* Parse arguments */
	scriptfile =  NULL;
	src_mailbox = dst_mailbox = move_mailbox = NULL;
	force_compile = execute = source_write = move_mode = FALSE;
	while ((c = sieve_tool_getopt(sieve_tool)) > 0) {
		switch (c) {
		case 'm':
			/* default mailbox (keep box) */
			dst_mailbox = optarg;
			break;
		case 's': 
			/* scriptfile executed before main script */
			{
				const char *file;			

				file = t_strdup(optarg);
				array_append(&scriptfiles, &file, 1);

				/* FIXME: */
				i_fatal_status(EX_USAGE, 
					"The -s argument is currently NOT IMPLEMENTED");
			}
			break;
		case 'q': 
			i_fatal_status(EX_USAGE, 
				"The -q argument is currently NOT IMPLEMENTED");
			break;
		case 'Q': 
			i_fatal_status(EX_USAGE, 
				"The -Q argument is currently NOT IMPLEMENTED");
			break;
		case 'e':
			/* execution mode */
			execute = TRUE;
			break;
		case 'C':
			/* force script compile */
			force_compile = TRUE;
			break;
		case 'M':
			/* move mode */
			move_mode = TRUE;
			break;
		case 'W':
			/* enable source mailbox write */
			source_write = TRUE;
			break;
		default:
			/* unrecognized option */
			print_help();
			i_fatal_status(EX_USAGE, "Unknown argument: %c", c);
			break;
		}
	}

	/* Script file argument */	
	if ( optind < argc ) {
		scriptfile = t_strdup(argv[optind++]);
	} else {
		print_help();
		i_fatal_status(EX_USAGE, "Missing <script-file> argument");
	}

	/* Source mailbox argument */
	if ( optind < argc ) {
		src_mailbox = t_strdup(argv[optind++]);
	} else {
		print_help();
		i_fatal_status(EX_USAGE, "Missing <source-mailbox> argument");
	}
		
	/* Source action argument */
	if ( optind < argc ) {
		const char *srcact = argv[optind++];

		if ( strcmp(srcact, "keep") == 0 ) {
			source_action = SIEVE_FILTER_SACT_KEEP;
		} else if ( strcmp(srcact, "move") == 0 ) {
			source_action = SIEVE_FILTER_SACT_MOVE;
			if ( optind < argc ) {
				move_mailbox = t_strdup(argv[optind++]);
			} else {
				print_help();
				i_fatal_status(EX_USAGE,
					"Invalid <source-action> argument: "
					"the `move' action requires mailbox argument");
			}
		} else if ( strcmp(srcact, "flag") == 0 ) {
			source_action = SIEVE_FILTER_SACT_DELETE;
		} else if ( strcmp(srcact, "expunge") == 0 ) {
			source_action = SIEVE_FILTER_SACT_EXPUNGE;
		} else {
			print_help();
			i_fatal_status(EX_USAGE, "Invalid <source-action> argument");
		}
	} 

	if ( optind != argc ) {
		print_help();
		i_fatal_status(EX_USAGE, "Unknown argument: %s", argv[optind]);
	}

	if ( dst_mailbox == NULL ) {
		dst_mailbox = src_mailbox;
	}

	/* Finish tool initialization */
	svinst = sieve_tool_init_finish(sieve_tool, TRUE);

        /* Enable debug extension */
        sieve_enable_debug_extension(svinst);

	/* Create error handler */
	ehandler = sieve_stderr_ehandler_create(svinst, 0);
	sieve_system_ehandler_set(ehandler);
	sieve_error_handler_accept_infolog(ehandler, TRUE);

	/* Compile main sieve script */
	if ( force_compile ) {
		main_sbin = sieve_tool_script_compile(svinst, scriptfile, NULL);
		if ( main_sbin != NULL )
			(void) sieve_save(main_sbin, NULL, TRUE, NULL);
	} else {
		main_sbin = sieve_tool_script_open(svinst, scriptfile);
	}

	/* Initialize mail user */
	mail_user = sieve_tool_get_mail_user(sieve_tool);

	/* Open the source mailbox */	

	src_mailbox = mailbox_name_to_mutf7(src_mailbox);
	ns = mail_namespace_find(mail_user->namespaces, &src_mailbox);
	if ( ns == NULL )
		i_fatal("Unknown namespace for source mailbox '%s'", src_mailbox);

	if ( !source_write || !execute ) 
		open_flags |= MAILBOX_FLAG_READONLY;
	
	src_box = mailbox_alloc(ns->list, src_mailbox, open_flags);
	if ( mailbox_open(src_box) < 0 ) {
		i_fatal("Couldn't open source mailbox '%s': %s", 
			src_mailbox, mail_storage_get_last_error(ns->storage, &error));
	}
	
	/* Open move box if necessary */

	if ( execute && source_action == SIEVE_FILTER_SACT_MOVE &&
		move_mailbox != NULL ) {
		move_mailbox = mailbox_name_to_mutf7(move_mailbox);
		ns = mail_namespace_find(mail_user->namespaces, &move_mailbox);
		if ( ns == NULL )
			i_fatal("Unknown namespace for mailbox '%s'", move_mailbox);

		move_box = mailbox_alloc(ns->list, move_mailbox, open_flags);
		if ( mailbox_open(move_box) < 0 ) {
			i_fatal("Couldn't open mailbox '%s': %s", 
				move_mailbox, mail_storage_get_last_error(ns->storage, &error));
		}

		if ( mailbox_backends_equal(src_box, move_box) ) {
			i_fatal("Source mailbox and mailbox for move action are identical.");
		}
	}

	/* Compose script environment */
	memset(&scriptenv, 0, sizeof(scriptenv));
	scriptenv.mailbox_autocreate = FALSE;
	scriptenv.default_mailbox = dst_mailbox;
	scriptenv.user = mail_user;
	scriptenv.username = sieve_tool_get_username(sieve_tool);
	scriptenv.hostname = "host.example.com";
	scriptenv.postmaster_address = "postmaster@example.com";
	scriptenv.smtp_open = NULL;
	scriptenv.smtp_close = NULL;

	/* Compose filter context */
	memset(&sfdata, 0, sizeof(sfdata));
	sfdata.senv = &scriptenv;
	sfdata.source_action = source_action;
	sfdata.move_mailbox = move_box;
	sfdata.main_sbin = main_sbin;
	sfdata.ehandler = ehandler;
	sfdata.execute = execute;
	sfdata.source_write = source_write;
	sfdata.move_mode = move_mode;

	/* Apply Sieve filter to all messages found */
	(void) filter_mailbox(&sfdata, src_box);
	
	/* Close the source mailbox */
	if ( src_box != NULL )
		mailbox_free(&src_box);

	/* Close the move mailbox */
	if ( move_box != NULL )
		mailbox_free(&move_box);

	/* Cleanup error handler */
	sieve_error_handler_unref(&ehandler);

	sieve_tool_deinit(&sieve_tool);

	return 0;
}
