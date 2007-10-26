/* 
 *	jabber_autobuddy.c: Tool for managing jabberd "buddies".  Only supports SQLite3 database backend. 
 *	Copyright 2006-2007 Apple Computer Inc.  All Rights Reserved.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sqlite3.h>
/*#include <sys/time.h>*/
#include <syslog.h>

#define MAX_USERNAME_LEN 256
#define MAX_HOSTNAME_LEN 256
#define MAX_JID_LEN 512
#define MAX_PASS_LEN 256
#define MAX_LOG_LINE 1024

short int g_debug = 0;

typedef struct sqlite3 sqlite3;

static const char *_log_level[] =
{
	"emergency",
	"alert",
	"critical",
	"error",
	"warning",
	"notice",
	"info",
	"debug"
};

void _log(int level, const char *msgfmt, ...)
{
	va_list ap;
	char message[MAX_LOG_LINE];
	time_t t;
	int sz;
	char *pos;
	
	// log to syslog
	va_start(ap, msgfmt);
	vsyslog(level, msgfmt, ap);
	va_end(ap);

	if ((! g_debug) && (level > LOG_WARNING)) {
		return;
	}
	
	// duplicate message to standard error
	/* timestamp */
	t = time(NULL);
	pos = (char *)ctime(&t);
	sz = strlen(pos);
	/* chop off the \n */
	pos[sz-1]=' ';

	/* insert the header */
	snprintf(message, MAX_LOG_LINE, "%s[%s] ", pos, _log_level[level]);

	/* find the end and attach the rest of the msg */
	for (pos = message; *pos != '\0'; pos++); /*empty statement */
	sz = pos - message;
	va_start(ap, msgfmt);
	vsnprintf(pos, MAX_LOG_LINE - sz, msgfmt, ap);
	va_end(ap);
	fprintf(stderr, "%s\n", message);
	fflush(stderr);

	return;
}

void usage() {
	fprintf(stderr, "Usage: jabber_autobuddy [options] <command> [command args]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "	-D : Debug mode; verbose output to standard error.\n");
	fprintf(stderr, "	-v, -h, -?: Display this usage information.\n\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "	-i JID: Initialize user's data store in SQLite database.\n"); 
	fprintf(stderr, "		This is a prerequisite for -a and -d.\n");
	fprintf(stderr, "	-d JID: Delete user's data store in SQLite database.\n");
	fprintf(stderr, "	-a JID: Add specified user to the buddy list of all other users.\n");
	fprintf(stderr, "	-m : Make all existing users buddies\n");
	fprintf(stderr, "	-r JID: Delete specified user from all buddy lists.\n\n");
	fprintf(stderr, "Notes:\n");
	fprintf(stderr, "	- JID is of the format <username>@<hostname>.  The username should be a valid\n");
	fprintf(stderr, "		Open Directory user's short name.  The hostname should be a domain (or realm) that\n");
	fprintf(stderr, "		the local jabberd service is configured to host.\n");
	fprintf(stderr, "	- You may specify one (and only one) command per execution.\n");
	fprintf(stderr, "\n\n");
	exit(0);
}

// insert user record into "active" table, and create an empty vcard
int init_user(char *input_jid, sqlite3 *db)
{
	char query[MAX_JID_LEN+128];
	time_t t;
	int ret;
	sqlite3_stmt *stmt;

	// See if user already exists
	snprintf(query, sizeof(query)-1, "select \"collection-owner\" from active where \"collection-owner\" = \"%s\"", input_jid);	
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_ROW:
			_log(LOG_ERR, "Error: %s already exists in the active table.  Skipping init.", input_jid);
			return 1;
		case SQLITE_DONE:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Insert new user into "active" table
	t = time(NULL);
	snprintf(query, sizeof(query)-1, "insert into active (\"collection-owner\", \"time\") values (\"%s\", \"%d\")", input_jid, t);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Create an empty VCARD
	snprintf(query, sizeof(query)-1, "insert into vcard (\"collection-owner\", \"fn\", \"nickname\", \"url\", \"tel\", \"email\", \"title\", \"role\", \"bday\", \"desc\", \"n-given\", \"n-family\", \"adr-street\", \"adr-extadd\", \"adr-locality\", \"adr-region\", \"adr-pcode\", \"adr-country\", \"org-orgname\", \"org-orgunit\", \"photo-type\", \"photo-binval\") values (\"%s\", NULL, NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE: 
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);
	return 0;
}

// Delete all of a user's items throughout the database
int delete_user(char *input_jid, sqlite3 *db)
{
	int i;
	char query[MAX_JID_LEN+128];
	time_t t;
	int ret;
	sqlite3_stmt *stmt;

	// Delete user from active table
	snprintf(query, sizeof(query)-1, "delete from active where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's vcard
	snprintf(query, sizeof(query)-1, "delete from vcard where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
	default:
		_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
		return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's disco-items
	snprintf(query, sizeof(query)-1, "delete from \"disco-items\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's records in privacy-default
	snprintf(query, sizeof(query)-1, "delete from \"privacy-default\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's records in privacy-items
	snprintf(query, sizeof(query)-1, "delete from \"privacy-items\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's records in private
	snprintf(query, sizeof(query)-1, "delete from \"private\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's records in queue
	snprintf(query, sizeof(query)-1, "delete from \"queue\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);
	
	// Delete user's records in roster-groups
	snprintf(query, sizeof(query)-1, "delete from \"roster-groups\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user from other roster-groups
	snprintf(query, sizeof(query)-1, "delete from \"roster-groups\" where jid = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);
	
	// Delete user's records in roster-items
	snprintf(query, sizeof(query)-1, "delete from \"roster-items\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user from other buddy lists
	snprintf(query, sizeof(query)-1, "delete from \"roster-items\" where jid = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	// Delete user's records in vacation-settings
	snprintf(query, sizeof(query)-1, "delete from \"vacation-settings\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);
	return 0;
}


// Add JID to the roster-items of all existing users
int add_user_to_rosters(char *input_jid, sqlite3 *db)
{
	int ret, ret2, ret3;
	sqlite3_stmt *stmt;
	int i;
	char query[MAX_JID_LEN+128];
	const unsigned char *elem;
	sqlite3_stmt *stmt2;
	const char *errmsg;
	
	// See if user exists
	snprintf(query, sizeof(query)-1, "select \"collection-owner\" FROM active where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	_log(LOG_INFO, "Issuing SQL command: %s", query);
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_ROW:
			break;
		case SQLITE_DONE:
			_log(LOG_ERR, "Error: Could not find input JID in database.  Use -i argument to initialize user.", query);
			sqlite3_finalize(stmt);
			return 1;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return 1;
	}
	sqlite3_finalize(stmt);
	
	// add JID to buddy lists
	snprintf(query, sizeof(query)-1, "select \"collection-owner\" from active");
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	_log(LOG_INFO, "Issuing SQL command: %s", query);
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			_log(LOG_NOTICE, "Error: Did not find any collection-owner items in active table");
			sqlite3_finalize(stmt);
			return 1;
		case SQLITE_ROW:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return 1;
	}
	
	// add a buddy for each JID in active table
	while (ret != SQLITE_DONE) {
		if (ret != SQLITE_ROW) {
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return 1;
		}
		elem = sqlite3_column_text(stmt, 0);
		if (strlen((char *)elem) > MAX_JID_LEN)
		{
			_log(LOG_ERR, "Error: JID larger than MAX_JID_LEN, skipping (%s)", elem);
			ret = sqlite3_step(stmt);
			continue;
		}	
		
		// skip our input JID
		if (! (strncmp((char *)elem, input_jid, MAX_JID_LEN))) {
			ret = sqlite3_step(stmt);
			continue;
		}

		// Add roster-items entries for first user
		// see if a roster-items item already exists for user. prevent duplicates.

		snprintf(query, sizeof(query)-1, "select \"collection-owner\" from \"roster-items\" where \"collection-owner\" = \"%s\" and jid = \"%s\"", elem, input_jid);
		ret2 = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt2, NULL);
		if (ret2 != SQLITE_OK) {
			_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt2);
			sqlite3_finalize(stmt);
			return 1;
		}
		ret2 = sqlite3_step(stmt2);
		errmsg = sqlite3_errmsg(db);
		sqlite3_finalize(stmt2);
		if (ret2 == SQLITE_DONE) {
			// add roster-items item
			snprintf(query, sizeof(query)-1, "insert into \"roster-items\" (\"collection-owner\", \"jid\", \"name\", \"to\", \"from\", \"ask\") values (\"%s\", \"%s\", \"\", \"1\", \"1\", \"0\")", elem, input_jid);
			ret3 = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt2, NULL);
			if (ret3 != SQLITE_OK) {
				_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
				sqlite3_finalize(stmt2);
				sqlite3_finalize(stmt);
				return 1;
			}
			_log(LOG_INFO, "Issuing SQL command: %s", query);
			ret3 = sqlite3_step(stmt2);
			switch(ret3) {
				case SQLITE_OK:
				case SQLITE_DONE:
					break;
				default:
					_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
					sqlite3_finalize(stmt2);
					sqlite3_finalize(stmt);
					return 1;
			}
			sqlite3_finalize(stmt2);
		}

		// Add roster-items entries for second user
		// see if a roster item already exists for user. prevent duplicates.
		snprintf(query, sizeof(query)-1, "select \"collection-owner\" from \"roster-items\" where \"collection-owner\" = \"%s\" and jid = \"%s\"", input_jid, elem);
		ret2 = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt2, NULL);
		if (ret2 != SQLITE_OK) {
			_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt2);
			sqlite3_finalize(stmt);
			return 1;
		}
		ret2 = sqlite3_step(stmt2);
		errmsg = sqlite3_errmsg(db);
		sqlite3_finalize(stmt2);
		if (ret2 == SQLITE_DONE) {
			// add roster-items item
			snprintf(query, sizeof(query)-1, "insert into \"roster-items\" (\"collection-owner\", \"jid\", \"name\", \"to\", \"from\", \"ask\") values (\"%s\", \"%s\", \"\", \"1\", \"1\", \"0\")", input_jid, elem);
			ret3 = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt2, NULL);
			if (ret3 != SQLITE_OK) {
				_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
				sqlite3_finalize(stmt2);
				sqlite3_finalize(stmt);
				return 1;
			}
			_log(LOG_INFO, "Issuing SQL command: %s", query);
			ret3 = sqlite3_step(stmt2);
			switch(ret3) {
				case SQLITE_OK:
				case SQLITE_DONE:
					break;
				default:
					_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
					sqlite3_finalize(stmt2);
					sqlite3_finalize(stmt);
					return 1;
			}
			sqlite3_finalize(stmt2);
		}
		ret = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	return 0;
}

// Deletes user's own roster items, and deletes user from other roster items.
int delete_user_from_rosters(char *input_jid, sqlite3 *db)
{
	char query[MAX_JID_LEN+128];
	int ret;
	sqlite3_stmt *stmt;

	snprintf(query, sizeof(query)-1, "delete from \"roster-items\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	snprintf(query, sizeof(query)-1, "delete from \"roster-items\" where \"jid\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	snprintf(query, sizeof(query)-1, "delete from \"roster-groups\" where \"collection-owner\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	sqlite3_finalize(stmt);

	snprintf(query, sizeof(query)-1, "delete from \"roster-groups\" where \"jid\" = \"%s\"", input_jid);
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		return 1;
	}
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			return 1;
	}
	
	sqlite3_finalize(stmt);
	return 0;
}

// Make each exising JID a "buddy" of every other JID.
int make_all_users_buddies(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	char query[MAX_JID_LEN+128];
	const unsigned char *elem;

	snprintf(query, sizeof(query)-1, "select \"collection-owner\" from \"active\"");
	ret = sqlite3_prepare_v2(db, (const char *)query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_prepare_v2): %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	_log(LOG_INFO, "Issuing SQL command: %s", query);
	ret = sqlite3_step(stmt);
	switch(ret) {
		case SQLITE_DONE:
		case SQLITE_OK:
			// no other users are in the database
			return 0;
		case SQLITE_ROW:
			break;
		default:
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return 1;
	}
	
	// add a buddy for each JID in active table
	while (ret != SQLITE_DONE) {
		if (ret != SQLITE_ROW) {
			_log(LOG_ERR, "Error: Unknown status of query (sqlite3_step): %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return 1;
		}
		elem = sqlite3_column_text(stmt, 0);
		
		if ((add_user_to_rosters((char *)elem, db)) != 0) {
			_log(LOG_ERR, "Error: add_user_to_rosters() failed for %s", elem);
		}
		ret = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	return 0;
}

// callback for SQLITE_BUSY state
static int db_busy_cb(void *unused, int count) {
	const int max_tries = 100;
	const int sleep_ms = 100;

	_log(LOG_NOTICE, "Database is busy, attempt %i of %i, sleeping for %i ms...", count, max_tries, sleep_ms);
	usleep(sleep_ms);
	return count<100;
}

// Main
int main (int argc, char * const argv[]) {
	enum operations {
		INIT_USER = 1,
		ADD_BUDDY,
		REMOVE_BUDDY,
		DELETE_USER,
		MAKE_ALL_USERS_BUDDIES
	};

	int opt;
	int operation = 0;
	char input_jid[MAX_JID_LEN+1];
	char jid[MAX_JID_LEN+1];
	char dbname[256];
	extern char* optarg;
	sqlite3 *db;
	int ret;

	/* Read command line arguments */
	while ((opt = getopt(argc, argv, "a:d:i:r:n:mvD?h")) != EOF)
	{
		switch(opt) 
		{
			case 'a':
				if (operation) {
					fprintf(stderr, "Error: You may only specify one operation at a time.\n");
					exit(1);
				}
				operation = ADD_BUDDY;
				strncpy(input_jid, optarg, MAX_JID_LEN);
				break;
		
			case 'r':
				if (operation) {
					fprintf(stderr, "Error: You may only specify one operation at a time.\n");
					exit(1);
				}
				operation = REMOVE_BUDDY;
				strncpy(input_jid, optarg, MAX_JID_LEN);
				break;
			
			case 'd':
				if (operation) {
					fprintf(stderr, "Error: You may only specify one operation at a time.\n");
					exit(1);
				}
				operation = DELETE_USER;
				strncpy(input_jid, optarg, MAX_JID_LEN);
				break;

			case 'i':
				// initialize user in database
				if (operation) {
					fprintf(stderr, "Error: You may only specify one operation at a time.\n");
					exit(1);
				}
				operation = INIT_USER;
				strncpy(input_jid, optarg, MAX_JID_LEN);
				break;

			case 'm':
				// Make every user in the database a buddy of all other users
				if (operation) {
					fprintf(stderr, "Error: You may only specify one operation at a time.\n");
					exit(1);
				}
				operation = MAKE_ALL_USERS_BUDDIES;
				break;

			case 'D':
				// Print debugging output to stderr
				g_debug = 1;
				break;

			case 'h':
			case 'v':
			case '?':
			default:
				usage();
				break;
		}
	}
	
	strncpy(dbname, "/private/var/jabberd/sqlite/jabberd2.db", sizeof(dbname)-1);
	// Connect to db
	ret = sqlite3_open((const char*)dbname, &db);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_open): %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

#if SQLITE_VERSION_NUMBER >= 3003008
	sqlite3_extended_result_codes(db, 1);
#endif
	sqlite3_busy_handler(db, db_busy_cb, dbname);

	switch (operation) {
		case ADD_BUDDY:
			if ((add_user_to_rosters(input_jid, db)) == 0)
			{
				_log(LOG_INFO, "Operation completed. %s added to all jabber buddy lists.", input_jid);
			} else {
				_log(LOG_ERR, "Error: failed to add user to rosters, input_jid = %s", input_jid);
			}
			break;
			
		case REMOVE_BUDDY:
			if ((delete_user_from_rosters(input_jid, db)) == 0)
			{
				_log(LOG_INFO, "Operation completed. %s removed from all jabber buddy lists.", input_jid);
			} else {
				_log(LOG_ERR, "Error: failed to remove user from rosters. input_jid = %s", input_jid);
			}	
			break;
				
		case INIT_USER:
			if ((init_user(input_jid, db)) == 0)
			{
				_log(LOG_INFO, "Operation completed. %s is now listed in the jabberd active table.", input_jid);
			} else {
				_log(LOG_ERR, "Error: failed to init user in jabberd database. input_jid = %s", input_jid);
			}
			break;
	
		case DELETE_USER:
			if ((delete_user(input_jid, db)) == 0)
			{
				_log(LOG_INFO, "Operation completed. %s has been removed from the jabberd database.", input_jid);
			} else {
				_log(LOG_ERR, "Error: failed to delete user from jabberd database. input_jid = %s", input_jid);
			}
			break;

		case MAKE_ALL_USERS_BUDDIES:
			if ((make_all_users_buddies(db)) == 0)
			{
				_log(LOG_INFO, "Operation completed. All users are buddies.");
			} else {
				_log(LOG_ERR, "Error: failed to make all active users buddies.");
			}
			break;
				
		default:
			fprintf(stderr, "Error: You must choose 1 operation to perform.\n\n");
			usage; 	// exits
	}
	ret = sqlite3_close(db);
	if (ret != SQLITE_OK) {
		_log(LOG_ERR, "Error: (sqlite3_close): %s", sqlite3_errmsg(db));
	}
	return 0;
}
