/*
 * Copyright (c) 2008-2011 Apple Inc. All rights reserved.
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

#include "auth-common.h"
#include "userdb.h"

#ifdef USERDB_OD

#include "str.h"
#include "istream.h"
#include "auth-cache.h"
#include "var-expand.h"
#include "db-od.h"

#include <stdlib.h>
#include <fcntl.h>

#define OD_CACHE_KEY "%u"
#define SIEVE_BASE	"/Library/Server/Mail/Data/rules"
#define DEFAULT_MAIL_DRIVER "maildir"

struct od_userdb_module {
	struct userdb_module module;
	struct db_od	*od_data;
	const char		*od_partitions;
	const char		*luser_relay;
	const char		*enforce_quotas;
	const char		*mail_driver;
	int				global_quota;
};


/* ------------------------------------------------------------------
 *	od_lookup ()
 * ------------------------------------------------------------------*/

static void od_lookup ( struct auth_request *in_request, userdb_callback_t *callback )
{
	struct userdb_module	*user_module	= in_request->userdb->userdb;
	struct od_userdb_module *od_user_module	= (struct od_userdb_module *)user_module;
	struct od_user			*user_info		= NULL;
	const char				*base_path		= NULL;
	const char				*alt_path		= NULL;
	const char				*quota_str		= NULL;

	auth_request_log_debug(in_request, "od", "lookup user=%s", in_request->user );

	user_info = db_od_user_lookup( in_request, od_user_module->od_data, in_request->user, FALSE );
	if ( !user_info ) {
		if ( od_user_module->luser_relay != NULL ) {
			user_info = db_od_user_lookup( in_request, od_user_module->od_data, od_user_module->luser_relay, FALSE );
			if ( user_info )
				auth_request_log_info(in_request, "od", "user record not found: %s  copying mail to undeliverable account: %s",
										in_request->user, od_user_module->luser_relay );
			else {
				auth_request_log_info(in_request, "od", "user record not found: %s  cold not deliver to undeliverable account: %s  record not found",
										in_request->user, od_user_module->luser_relay );
				callback( USERDB_RESULT_USER_UNKNOWN, in_request );
				return;
			}
		} else {
			callback( USERDB_RESULT_USER_UNKNOWN, in_request );
			return;
		}
	}

	/* do SACL check here for kerberos authentication */
	if ( in_request->mech_name && (strcmp(in_request->mech_name, "GSSAPI") == 0) ) {
		if ( is_acct_enabled( in_request, user_info->acct_state, user_info->record_name) == FALSE) {
			db_od_user_unref(&user_info);
			callback( PASSDB_RESULT_USER_DISABLED, in_request );
			return;
		}
	}

	auth_request_log_debug(in_request, "od", "record name=%s, uid=%d, gid=%d, but using _dovecot:mail", user_info->record_name, user_info->user_uid, user_info->user_gid );

	auth_request_init_userdb_reply( in_request );
	auth_request_set_userdb_field( in_request, "uid", "214" );	/* _dovecot's uid */
	auth_request_set_userdb_field( in_request, "gid", "6" );	/* mail's gid */

	/* individual quotas override global quota settings */
	if ( user_info->mail_quota != 0 )
		quota_str = t_strdup_printf( "*:storage=%u", user_info->mail_quota * 1024 );		/* make quota string from user specific settings */
	else if ( od_user_module->global_quota != 0 )
		quota_str = t_strdup_printf( "*:storage=%u", od_user_module->global_quota * 1024 );		/* make quota string from global quota settings */
	else
		quota_str = t_strdup_printf( "*:storage=%u", 0 );		/* set default quota to 0 for no quota */

	/* need to set for all users to all for usage reporting */
	if ( (od_user_module->enforce_quotas != NULL) && strcmp( od_user_module->enforce_quotas, "yes") == 0 )
		auth_request_set_userdb_field( in_request, "quota", "maildir:User quota" );
	else
		auth_request_set_userdb_field( in_request, "quota", "maildir:User quota:noenforcing" );

	auth_request_set_userdb_field( in_request, "quota_rule", quota_str );

	auth_request_log_debug(in_request, "od", "user=%s, quota=%s", user_info->record_name, quota_str );
	if ( user_info->alt_data_loc == NULL )
		base_path = db_od_get_ms_path( in_request, "default", od_user_module->od_partitions );
	else
		base_path = db_od_get_ms_path( in_request, user_info->alt_data_loc, od_user_module->od_partitions );

	alt_path = t_strconcat( od_user_module->mail_driver, ":", base_path, "/", user_info->user_guid, NULL );

	/* use GUID not username in mail directory */
	auth_request_log_debug(in_request, "od", "data store location=%s", alt_path );
	auth_request_set_userdb_field( in_request, "mail", alt_path );
	auth_request_set_userdb_field( in_request, "mail_location", alt_path );

	/* use GUID for sieve plugin too */
	alt_path = t_strconcat( SIEVE_BASE "/", user_info->user_guid, "/dovecot.sieve", NULL );
	auth_request_set_userdb_field( in_request, "sieve", alt_path );
	alt_path = t_strconcat( SIEVE_BASE "/", user_info->user_guid, NULL );
	auth_request_set_userdb_field( in_request, "sieve_dir", alt_path );
	/* use GUID for managesieve too */
	auth_request_set_userdb_field( in_request, "sieve_storage", alt_path );

	/* if migrating, set account stat */
	struct stat st;
	if (stat("/var/db/.mailmigration.plist", &st) == 0)
		if (!(user_info->acct_state & acct_migrated))
			auth_request_set_userdb_field( in_request, "acct_migration_state", "NOT" );

	db_od_user_unref(&user_info);
	callback( USERDB_RESULT_OK, in_request );
} /* od_lookup */


/* ------------------------------------------------------------------
 *	od_preinit ()
 * ------------------------------------------------------------------*/

static struct userdb_module * od_preinit ( pool_t pool, const char *in_args )
{
	struct od_userdb_module	*module;
	const char *const *str;

	module = p_new( pool, struct od_userdb_module, 1 );

	module->luser_relay = NULL;
	module->enforce_quotas = NULL;
	module->mail_driver = DEFAULT_MAIL_DRIVER;
	module->global_quota = 0;
	module->module.cache_key = OD_CACHE_KEY;
	module->module.blocking = FALSE;

	if ( in_args != NULL ) {
		for ( str = t_strsplit(in_args, " "); *str != NULL; str++ ) {
			if ( (strncmp( *str, "partition=", 10) == 0) && (strlen( *str ) > 10) ) {
				int fd = open( *str + 10, O_RDONLY );
				if ( fd == -1 )
					i_warning( "od: failed to open partition file %s: %m", *str + 10 );
				else {
					struct istream *input;
					char *line = NULL;
					const char *data = NULL;
					input = i_stream_create_fd(fd, 1024, TRUE);
					while ( (line = i_stream_read_next_line(input)) != NULL ) {
						if ( !data )
							data = t_strdup_noconst( line );
						else
							data = t_strconcat(data, "\n", line, NULL);
					}
					module->od_partitions = p_strndup(pool, data, strlen(data));
					i_stream_destroy(&input);
					close(fd);
				}
			}
			else if((strncmp(*str,"luser_relay=",12)==0)&&(strlen(*str)>12))
				module->luser_relay=p_strndup(pool,*str+12,strlen(*str+12));
			else if((strncmp(*str,"enforce_quotas=",15)==0)&&(strlen(*str)>15))
				module->enforce_quotas=p_strndup(pool,*str+15,strlen(*str+15));
			else if((strncmp(*str,"global_quota=",13)==0)&&(strlen(*str)>13)) {
				int quota = atoi(*str + 13);
				if (quota >= 0)
					module->global_quota = quota;
			} else if (!strncmp(*str, "pos_cache_ttl=", 14)) {
				int ttl = atoi(*str + 14);
				if (ttl >= 0)
					od_pos_cache_ttl = ttl;
			} else if (!strncmp(*str, "neg_cache_ttl=", 14)) {
				int ttl = atoi(*str + 14);
				if (ttl >= 0)
					od_neg_cache_ttl = ttl;
			} else if (!strncmp(*str, "use_getpwnam_ext=", 17))
				od_use_getpwnam_ext = strcmp(*str + 17, "yes") == 0 ? TRUE : FALSE;
			else if (!strncmp(*str, "blocking=", 9))
				module->module.blocking = strcmp(*str + 9, "yes") == 0 ? TRUE : FALSE;
			else if (!strncmp(*str, "mail_driver=", 12))
				module->mail_driver = p_strdup(pool, *str + 12);
		}
	}

	if (global_auth_settings->debug)
		i_debug("od-debug: userdb-od: args=%s", in_args);

	module->od_data	= db_od_init( TRUE );

	return( &module->module );
} /* od_preinit */


/* ------------------------------------------------------------------
 *	od_init ()
 * ------------------------------------------------------------------*/

static void od_init ( struct userdb_module *_module )
{
	struct od_userdb_module *module = (struct od_userdb_module *)_module;
	db_od_do_init( module->od_data );
} /* od_init */


/* ------------------------------------------------------------------
 *	od_deinit ()
 * ------------------------------------------------------------------*/

static void od_deinit ( struct userdb_module *_module )
{
	struct od_userdb_module *module = (struct od_userdb_module *)_module;
	db_od_unref( &module->od_data );
} /* od_deinit */


/* ------------------------------------------------------------------
 *	userdb_module_interface
 * ------------------------------------------------------------------*/

struct userdb_module_interface userdb_od = {
	"od",
	od_preinit,
	od_init,
	od_deinit,
	od_lookup,

	NULL,
	NULL,
	NULL
};

#else
struct userdb_module_interface userdb_od = {
	.name = "od"
};
#endif
