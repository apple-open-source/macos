/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#include "lib.h"
#include "array.h"
#include "home-expand.h"
#include "eacces-error.h"
#include "mail-storage.h"
#include "mail-deliver.h"
#include "mail-user.h"
#include "duplicate.h"
#include "smtp-client.h"
#include "mail-send.h"
#include "lda-settings.h"

#include "sieve.h"

#include "lda-sieve-plugin.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

/*
 * Configuration
 */

#define SIEVE_DEFAULT_PATH "~/.dovecot.sieve"

#define LDA_SIEVE_MAX_USER_ERRORS 30

/*
 * Global variables 
 */

static deliver_mail_func_t *next_deliver_mail;

/*
 * Settings handling
 */

static const char *lda_sieve_get_homedir
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

static const char *lda_sieve_get_setting
(void *context, const char *identifier)
{
	struct mail_user *mail_user = (struct mail_user *) context;

	if ( mail_user == NULL )
		return NULL;

	return mail_user_plugin_getenv(mail_user, identifier);	
}

static const struct sieve_environment lda_sieve_env = {
	lda_sieve_get_homedir,
    lda_sieve_get_setting
};

/*
 * Mail transmission
 */

static void *lda_sieve_smtp_open
(void *script_ctx, const char *destination,
	const char *return_path, FILE **file_r)
{
	struct mail_deliver_context *dctx =
		(struct mail_deliver_context *) script_ctx;

	return (void *) smtp_client_open(dctx->set, destination, return_path, file_r);
}

static bool lda_sieve_smtp_close
(void *script_ctx ATTR_UNUSED, void *handle)
{
	struct smtp_client *smtp_client = (struct smtp_client *) handle;

	return ( smtp_client_close(smtp_client) == 0 );
}

static int lda_sieve_reject_mail(void *script_ctx, const char *recipient,
	const char *reason)
{
	struct mail_deliver_context *dctx =
		(struct mail_deliver_context *) script_ctx;
	
	return mail_send_rejection(dctx, recipient, reason);
}

/*
 * Duplicate checking
 */

static int lda_sieve_duplicate_check
(void *script_ctx, const void *id, size_t id_size, const char *user)
{
	struct mail_deliver_context *dctx = (struct mail_deliver_context *) script_ctx;

	return duplicate_check(dctx->dup_ctx, id, id_size, user);
}

static void lda_sieve_duplicate_mark
(void *script_ctx, const void *id, size_t id_size, const char *user,
	time_t time)
{
	struct mail_deliver_context *dctx = (struct mail_deliver_context *) script_ctx;

	duplicate_mark(dctx->dup_ctx, id, id_size, user, time);
}

/*
 * Plugin implementation
 */

struct lda_sieve_run_context {
	struct sieve_instance *svinst;

	struct mail_deliver_context *mdctx;

	const char *const *script_files;
	unsigned int script_count;

	const char *user_script;
	const char *main_script;

	const struct sieve_message_data *msgdata;
	const struct sieve_script_env *scriptenv;

	struct sieve_error_handler *user_ehandler;
	struct sieve_error_handler *master_ehandler;
	const char *userlog;
};

static const char *lda_sieve_get_personal_path
(struct sieve_instance *svinst, struct mail_user *user)
{
	const char *script_path, *home;

	if ( mail_user_get_home(user, &home) <= 0 )
        home = NULL;

	script_path = mail_user_plugin_getenv(user, "sieve");

	/* userdb may specify Sieve path */
	if (script_path != NULL) {
		if (*script_path == '\0') {
			/* disabled */
			if ( user->mail_debug )
				sieve_sys_debug(svinst, "empty script path, disabled");
			return NULL;
		}

		script_path = mail_user_home_expand(user, script_path);

		if (*script_path != '/' && *script_path != '\0') {
			/* relative path. change to absolute. */

			if ( home == NULL || *home == '\0' ) {
				sieve_sys_error(svinst, "relative script path, but empty home dir: %s", script_path);
				return NULL;
			}

			script_path = t_strconcat(home, "/", script_path, NULL);
		}
	} else {
		if ( home == NULL || *home == '\0' ) {
			sieve_sys_error(svinst, 
				"path to user's main active personal script is unknown. "
				"See http://wiki2.dovecot.org/Pigeonhole/Sieve/Configuration");
			return NULL;
		}

		script_path = mail_user_home_expand(user, SIEVE_DEFAULT_PATH);
	}

	return script_path;
}

static const char *lda_sieve_get_default_path
(struct mail_user *user)
{
	const char *script_path;

	/* Use global script path, if one exists */
	script_path = mail_user_plugin_getenv(user, "sieve_global_path");
	if (script_path == NULL) {
		/* For backwards compatibility */
		script_path = mail_user_plugin_getenv(user, "global_script_path");
	}

	return script_path;
}

static int lda_sieve_multiscript_get_scriptfiles
(struct sieve_instance *svinst, const char *script_path,
	ARRAY_TYPE(const_string) *scriptfiles)
{
	struct sieve_directory *sdir;
	enum sieve_error error;
	const char *file;

	if ( (sdir=sieve_directory_open(svinst, script_path, &error)) == NULL )
		return ( error == SIEVE_ERROR_NOT_FOUND ? 0 : -1 );

	while ( (file=sieve_directory_get_scriptfile(sdir)) != NULL ) {
		const char *const *scripts;
		unsigned int count, i;

		/* Insert into sorted array */

		scripts = array_get(scriptfiles, &count);
		for ( i = 0; i < count; i++ ) {
			if ( strcmp(file, scripts[i]) < 0 )
				break;
		}

		if ( i == count ) 
			array_append(scriptfiles, &file, 1);
		else
			array_insert(scriptfiles, i, &file, 1);
	}

	sieve_directory_close(&sdir);
	return 1;
}

static void lda_sieve_binary_save
(struct lda_sieve_run_context *srctx, struct sieve_binary *sbin, 
	unsigned int script_index)
{
	const char *script_path = srctx->script_files[script_index];
	enum sieve_error error;

	/* Save binary when compiled */
	if ( sieve_save(sbin, NULL, FALSE, &error) < 0 &&
		error == SIEVE_ERROR_NO_PERM  && script_path != srctx->user_script ) {
		/* Cannot save binary for global script */
		sieve_sys_error(srctx->svinst,
			"the lda sieve plugin does not have permission "
			"to save global sieve script binaries; "
			"global sieve scripts like %s need to be "
			"pre-compiled using the sievec tool", script_path);
	}
}

static struct sieve_binary *lda_sieve_open
(struct lda_sieve_run_context *srctx, unsigned int script_index,
	enum sieve_error *error_r)
{
	struct sieve_instance *svinst = srctx->svinst;
	const char *script_path = srctx->script_files[script_index];
	const char *script_name =
		( script_path == srctx->main_script ? "main_script" : NULL );
	struct sieve_error_handler *ehandler;
	struct sieve_binary *sbin;
	bool debug = srctx->mdctx->dest_user->mail_debug;

	if ( script_path == srctx->user_script )
		ehandler = srctx->user_ehandler;
	else
		ehandler = srctx->master_ehandler;

	if ( debug )
		sieve_sys_debug(svinst, "opening script %s", script_path);

	sieve_error_handler_reset(ehandler);

	/* Open the sieve script */
	if ( (sbin=sieve_open(svinst, script_path, script_name, ehandler, error_r))
		== NULL ) {
		if ( *error_r == SIEVE_ERROR_NOT_FOUND ) {
			if ( debug )
				sieve_sys_debug(svinst, "script file %s is missing", script_path);
		} else if ( *error_r == SIEVE_ERROR_NOT_VALID && 
			script_path == srctx->user_script && srctx->userlog != NULL ) {
			sieve_sys_error(svinst,
				"failed to open script %s "
				"(view user logfile %s for more information)",
				script_path, srctx->userlog);
		} else {
			sieve_sys_error(svinst,
				"failed to open script %s", script_path);
		}

		return NULL;
	}

	lda_sieve_binary_save(srctx, sbin, script_index);
	return sbin;
}

static struct sieve_binary *lda_sieve_recompile
(struct lda_sieve_run_context *srctx, unsigned int script_index,
	enum sieve_error *error_r)
{
	struct sieve_instance *svinst = srctx->svinst;
	const char *script_path = srctx->script_files[script_index];
	const char *script_name =
		( script_path == srctx->main_script ? "main_script" : NULL );
	struct sieve_error_handler *ehandler;
	struct sieve_binary *sbin;
	bool debug = srctx->mdctx->dest_user->mail_debug;

	/* Warn */

	sieve_sys_warning(svinst,
		"encountered corrupt binary: re-compiling script %s", script_path);

	/* Recompile */

	if ( script_path == srctx->user_script )
		ehandler = srctx->user_ehandler;
	else
		ehandler = srctx->master_ehandler;

	if ( (sbin=sieve_compile
		(svinst, script_path, script_name, ehandler, error_r)) == NULL ) {

		if ( *error_r == SIEVE_ERROR_NOT_FOUND ) {
			if ( debug )
				sieve_sys_debug(svinst, "script file %s is missing for re-compile", 
					script_path);
		} else if ( *error_r == SIEVE_ERROR_NOT_VALID && 
			script_path == srctx->user_script && srctx->userlog != NULL ) {
			sieve_sys_error(svinst,
				"failed to re-compile script %s "
				"(view user logfile %s for more information)",
				script_path, srctx->userlog);
		} else {
			sieve_sys_error(svinst,
				"failed to re-compile script %s", script_path);
		}

		return NULL;
	}

	return sbin;
}

static int lda_sieve_handle_exec_status
(struct lda_sieve_run_context *srctx, const char *script_path, int status)
{
	struct sieve_instance *svinst = srctx->svinst;
	const char *userlog_notice = "";
	int ret;

	if ( script_path == srctx->user_script && srctx->userlog != NULL ) {
		userlog_notice = t_strdup_printf
			(" (user logfile %s may reveal additional details)", srctx->userlog);
	}

	switch ( status ) {
	case SIEVE_EXEC_FAILURE:
		sieve_sys_error(svinst,
			"execution of script %s failed, but implicit keep was successful%s", 
			script_path, userlog_notice);
		ret = 1;
		break;
	case SIEVE_EXEC_BIN_CORRUPT:
		sieve_sys_error(svinst,
			"!!BUG!!: binary compiled from %s is still corrupt; "
			"bailing out and reverting to default delivery", 
			script_path);
		ret = -1;
		break;
	case SIEVE_EXEC_KEEP_FAILED:
		sieve_sys_error(svinst,
			"script %s failed with unsuccessful implicit keep%s", 
			script_path, userlog_notice);
		ret = -1;
		break;
	default:
		ret = status > 0 ? 1 : -1;
		break;
	}

	return ret;
}

static int lda_sieve_singlescript_execute
(struct lda_sieve_run_context *srctx)
{
	struct sieve_instance *svinst = srctx->svinst;
	const char *script_file = srctx->script_files[0];
	bool user_script = ( script_file == srctx->user_script );
	struct sieve_error_handler *ehandler;
	struct sieve_binary *sbin;
	bool debug = srctx->mdctx->dest_user->mail_debug;
	enum sieve_error error;
	int ret;

	/* Open the script */

	if ( (sbin=lda_sieve_open(srctx, 0, &error)) == NULL )
		return ( error == SIEVE_ERROR_NOT_FOUND ? 0 : -1 );

	/* Execute */

	if ( debug )
		sieve_sys_debug(svinst, "executing script from %s", sieve_get_source(sbin));

	if ( user_script ) {
		ehandler = srctx->user_ehandler;
	} else {
		ehandler = srctx->master_ehandler;
	}

	ret = sieve_execute(sbin, srctx->msgdata, srctx->scriptenv, ehandler, NULL);

	/* Recompile if corrupt binary */

	if ( ret == SIEVE_EXEC_BIN_CORRUPT && sieve_is_loaded(sbin) ) {
		/* Close corrupt script */

		sieve_close(&sbin);

		/* Recompile */

		if ( (sbin=lda_sieve_recompile(srctx, 0, &error)) == NULL )
			return ( error == SIEVE_ERROR_NOT_FOUND ? 0 : -1 );

		/* Execute again */

		if ( debug )
			sieve_sys_debug
				(svinst, "executing script from %s", sieve_get_source(sbin));

		ret = sieve_execute(sbin, srctx->msgdata, srctx->scriptenv, ehandler, NULL);

		/* Save new version */

		if ( ret != SIEVE_EXEC_BIN_CORRUPT )
			lda_sieve_binary_save(srctx, sbin, 0);
	}

	sieve_close(&sbin);

	/* Report status */
	return lda_sieve_handle_exec_status(srctx, script_file, ret);
}

static int lda_sieve_multiscript_execute
(struct lda_sieve_run_context *srctx)
{
	struct sieve_instance *svinst = srctx->svinst;
	const char *const *scripts = srctx->script_files;
	unsigned int count = srctx->script_count;
	struct sieve_multiscript *mscript;
	struct sieve_error_handler *ehandler = srctx->master_ehandler;
	bool debug = srctx->mdctx->dest_user->mail_debug;
	const char *last_script = NULL;
	bool user_script = FALSE;
	unsigned int i;
	int ret = 1; 
	bool more = TRUE;
	enum sieve_error error;

	/* Start execution */

	mscript = sieve_multiscript_start_execute
		(svinst, srctx->msgdata, srctx->scriptenv);

	/* Execute scripts before main script */

	for ( i = 0; i < count && more; i++ ) {
		struct sieve_binary *sbin = NULL;
		const char *script_file = scripts[i];
		bool final = ( i == count - 1 );

		user_script = ( script_file == srctx->user_script );
		last_script = script_file;

		if ( user_script )
			ehandler = srctx->user_ehandler;
		else
			ehandler = srctx->master_ehandler;

		/* Open */

		if ( (sbin=lda_sieve_open(srctx, i, &error)) == NULL ) {
			ret = ( error == SIEVE_ERROR_NOT_FOUND ? 0 : -1 );
			break;
		}

		/* Execute */

		if ( debug )
			sieve_sys_debug(svinst, "executing script from %s", sieve_get_source(sbin));

		more = sieve_multiscript_run(mscript, sbin, ehandler, final);

		if ( !more ) {
			if ( sieve_multiscript_status(mscript) == SIEVE_EXEC_BIN_CORRUPT &&
				sieve_is_loaded(sbin) ) {
				/* Close corrupt script */

				sieve_close(&sbin);

				/* Recompile */

				if ( (sbin=lda_sieve_recompile(srctx, i, &error))	== NULL ) {
					ret = ( error == SIEVE_ERROR_NOT_FOUND ? 0 : -1 );
					break;
				}

				/* Execute again */

				more = sieve_multiscript_run(mscript, sbin, ehandler, final);

				/* Save new version */

				if ( more && 
					sieve_multiscript_status(mscript) != SIEVE_EXEC_BIN_CORRUPT )
					lda_sieve_binary_save(srctx, sbin, i);
			}
		}

		sieve_close(&sbin);
	}

	/* Finish execution */

	ret = sieve_multiscript_finish(&mscript, ehandler, NULL);

	return lda_sieve_handle_exec_status(srctx, last_script, ret);
}

static int lda_sieve_run
(struct mail_deliver_context *mdctx, struct sieve_instance *svinst,
	const char *user_script, const char *default_script,
	const ARRAY_TYPE (const_string) *scripts_before,
	const ARRAY_TYPE (const_string) *scripts_after,
	struct mail_storage **storage_r)
{
	ARRAY_TYPE (const_string) scripts;
	struct lda_sieve_run_context srctx;
	struct sieve_message_data msgdata;
	struct sieve_script_env scriptenv;
	struct sieve_exec_status estatus;
	int ret = 0;

	*storage_r = NULL;

	/* Initialize */

	memset(&srctx, 0, sizeof(srctx));
	srctx.svinst = svinst;
	srctx.mdctx = mdctx;

	/* Compose execution sequence */

	t_array_init(&scripts, 32);

	array_append_array(&scripts, scripts_before);

	if ( user_script != NULL ) {
		array_append(&scripts, &user_script, 1);
		srctx.user_script = user_script;
		srctx.main_script = user_script;
	} else if ( default_script != NULL ) {
		array_append(&scripts, &default_script, 1);
		srctx.user_script = NULL;
		srctx.main_script = default_script;
	} else {
		srctx.user_script = NULL;
		srctx.main_script = NULL;
	}

	array_append_array(&scripts, scripts_after);

	/* Initialize error handlers */

	srctx.master_ehandler = sieve_system_ehandler_get(svinst);

	if ( user_script != NULL ) {
		srctx.userlog = t_strconcat(user_script, ".log", NULL);
		srctx.user_ehandler = sieve_logfile_ehandler_create
			(svinst, srctx.userlog, LDA_SIEVE_MAX_USER_ERRORS);
	}

	/* Collect necessary message data */

	memset(&msgdata, 0, sizeof(msgdata));

	msgdata.mail = mdctx->src_mail;
	msgdata.return_path = mail_deliver_get_return_address(mdctx);
	msgdata.orig_envelope_to = mdctx->dest_addr;
	msgdata.final_envelope_to = mdctx->final_dest_addr;
	msgdata.auth_user = mdctx->dest_user->username;
	(void)mail_get_first_header(msgdata.mail, "Message-ID", &msgdata.id);

	srctx.msgdata = &msgdata;

	/* Compose script execution environment */

	memset(&scriptenv, 0, sizeof(scriptenv));
	memset(&estatus, 0, sizeof(estatus));

	scriptenv.action_log_format = mdctx->set->deliver_log_format;
	scriptenv.default_mailbox = mdctx->dest_mailbox_name;
	scriptenv.mailbox_autocreate = mdctx->set->lda_mailbox_autocreate;
	scriptenv.mailbox_autosubscribe = mdctx->set->lda_mailbox_autosubscribe;
	scriptenv.user = mdctx->dest_user;
	scriptenv.username = mdctx->dest_user->username;
	scriptenv.hostname = mdctx->set->hostname;
	scriptenv.postmaster_address = mdctx->set->postmaster_address;
	scriptenv.smtp_open = lda_sieve_smtp_open;
	scriptenv.smtp_close = lda_sieve_smtp_close;
	scriptenv.duplicate_mark = lda_sieve_duplicate_mark;
	scriptenv.duplicate_check = lda_sieve_duplicate_check;
	scriptenv.reject_mail = lda_sieve_reject_mail;
	scriptenv.script_context = (void *) mdctx;
	scriptenv.exec_status = &estatus;

	srctx.scriptenv = &scriptenv;

	/* Assign script sequence */

	srctx.script_files = array_get(&scripts, &srctx.script_count);

	/* Execute script(s) */

	if ( srctx.script_count == 1 )
		ret = lda_sieve_singlescript_execute(&srctx);
	else
		ret = lda_sieve_multiscript_execute(&srctx);

	/* Record status */

	mdctx->tried_default_save = estatus.tried_default_save;
	*storage_r = estatus.last_storage;

	/* Clean up */

	if ( srctx.user_ehandler != NULL )
		sieve_error_handler_unref(&srctx.user_ehandler);

	return ret;
}

static int lda_sieve_deliver_mail
(struct mail_deliver_context *mdctx, struct mail_storage **storage_r)
{
	struct sieve_instance *svinst;
	struct sieve_error_handler *master_ehandler; 
	const char *user_script, *default_script, *sieve_before, *sieve_after;
	ARRAY_TYPE (const_string) scripts_before;
	ARRAY_TYPE (const_string) scripts_after;
	bool debug = mdctx->dest_user->mail_debug;
	int ret = 0;

	/* Initialize Sieve engine */

	svinst = sieve_init(&lda_sieve_env, mdctx->dest_user, debug);

	/* Initialize master error handler */

	master_ehandler = sieve_master_ehandler_create(svinst, mdctx->session_id, 0);
	sieve_system_ehandler_set(master_ehandler);

	sieve_error_handler_accept_infolog(master_ehandler, TRUE);
	sieve_error_handler_accept_debuglog(master_ehandler, debug);

	*storage_r = NULL;

	/* Find scripts and run them */

	T_BEGIN { 
		struct stat st;

		/* Find the personal script to execute */

		user_script = lda_sieve_get_personal_path(svinst, mdctx->dest_user);
		default_script = lda_sieve_get_default_path(mdctx->dest_user);

		if ( user_script != NULL && stat(user_script, &st) < 0 ) {

			switch ( errno ) {
			case ENOENT:
				if ( debug )
					sieve_sys_debug(svinst, "user's script path %s doesn't exist "
						"(using global script path in stead)", user_script);
				break;
			case EACCES:
				sieve_sys_error(svinst, "failed to stat user's sieve script: %s "
					"(using global script path in stead)",
					eacces_error_get("stat", user_script));
				break;
			default:
				sieve_sys_error(svinst, "failed to stat user's sieve script: "
					"stat(%s) failed: %m (using global script path in stead)",
					user_script);
				break;
			}

			user_script = NULL;
		}

		if ( debug ) {
			const char *script = user_script == NULL ? default_script : user_script;

			if ( script == NULL )
				sieve_sys_debug(svinst, "user has no valid personal script");
			else
				sieve_sys_debug(svinst, "using sieve path for user's script: %s", script);
		}

		/* Check for multiscript */

		t_array_init(&scripts_before, 16);
		t_array_init(&scripts_after, 16);

		sieve_before = mail_user_plugin_getenv(mdctx->dest_user, "sieve_before");
		sieve_after = mail_user_plugin_getenv(mdctx->dest_user, "sieve_after");

		if ( sieve_before != NULL && *sieve_before != '\0' ) {
			if ( lda_sieve_multiscript_get_scriptfiles
				(svinst, sieve_before, &scripts_before) == 0 && debug ) {
				sieve_sys_debug(svinst, "sieve_before path not found: %s", sieve_before);
			}
		}

		if ( sieve_after != NULL && *sieve_after != '\0' ) {
			if ( lda_sieve_multiscript_get_scriptfiles
				(svinst, sieve_after, &scripts_after) == 0 && debug ) {
				sieve_sys_debug(svinst, "sieve_after path not found: %s", sieve_after);
			}
		}

		if ( debug ) {
			const char *const *scriptfiles;
			unsigned int count, i;

			scriptfiles = array_get(&scripts_before, &count);
			for ( i = 0; i < count; i ++ ) {
				sieve_sys_debug(svinst, "executed before user's script(%d): %s",
					i+1, scriptfiles[i]);
			}

			scriptfiles = array_get(&scripts_after, &count);
			for ( i = 0; i < count; i ++ ) {
				sieve_sys_debug(svinst, "executed after user's script(%d): %s",
					i+1, scriptfiles[i]);
			}
		}

		/* Check whether there are any scripts to execute */

		if ( array_count(&scripts_before) == 0 && array_count(&scripts_after) == 0 &&
			user_script == NULL && default_script == NULL ) {
			if ( debug )
				sieve_sys_debug(svinst, "no scripts to execute: reverting to default delivery.");

			/* No error, but no delivery by this plugin either. A return value of <= 0 for a 
			 * deliver plugin is is considered a failure. In deliver itself, saved_mail and 
			 * tried_default_save remain unset, meaning that deliver will then attempt the 
			 * default delivery. We return 0 to signify the lack of a real error. 
			 */
			ret = 0; 
		} else {
			/* Run the script(s) */

			ret = lda_sieve_run
				(mdctx, svinst, user_script, default_script, &scripts_before,
					&scripts_after, storage_r);
		}

	} T_END;

	sieve_deinit(&svinst);
	sieve_error_handler_unref(&master_ehandler);
	return ret; 
}

/*
 * Plugin interface
 */

const char *sieve_plugin_version = DOVECOT_VERSION;
const char sieve_plugin_binary_dependency[] = "lda lmtp";

void sieve_plugin_init(void)
{
	/* Hook into the delivery process */
	next_deliver_mail = mail_deliver_hook_set(lda_sieve_deliver_mail);
}

void sieve_plugin_deinit(void)
{
	/* Remove hook */
	mail_deliver_hook_set(next_deliver_mail);
}
