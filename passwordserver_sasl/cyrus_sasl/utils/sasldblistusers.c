/* sasldblistusers.c -- list users in sasldb
 * $Id: sasldblistusers.c,v 1.2 2002/05/23 18:57:35 snsimon Exp $
 * Rob Siemborski
 * Tim Martin
 */
/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <sasl.h>
#include "../sasldb/sasldb.h"

/* Cheating to make the utils work out right */
extern const sasl_utils_t *sasl_global_utils;

/*
 * List all users in database
 */
int listusers(sasl_conn_t *conn)
{
    int result;
    char key_buf[32768];
    size_t key_len;
    sasldb_handle dbh;
    
    dbh = _sasldb_getkeyhandle(sasl_global_utils, conn);

    if(!dbh) {
	printf("can't getkeyhandle\n");
	return SASL_FAIL;
    }

    result = _sasldb_getnextkey(sasl_global_utils, dbh,
				key_buf, 32768, &key_len);

    while (result == SASL_CONTINUE)
    {
	char authid_buf[16384];
	char realm_buf[16384];
	char property_buf[16384];
	int ret;

	ret = _sasldb_parse_key(key_buf, key_len,
				authid_buf, 16384,
				realm_buf, 16384,
				property_buf, 16384);

	if(ret == SASL_BUFOVER) {
	    printf("Key too large\n");
	    continue;
	} else if(ret != SASL_OK) {
	    printf("Bad Key!\n");
	    continue;
	}
	
	printf("%s@%s: %s\n",authid_buf,realm_buf,property_buf);

	result = _sasldb_getnextkey(sasl_global_utils, dbh,
				    key_buf, 32768, &key_len);
    }

    if (result == SASL_BUFOVER) {
	fprintf(stderr, "Key too large!\n");
    } else if (result != SASL_OK) {
	fprintf(stderr,"db failure\n");
    }

    return _sasldb_releasekeyhandle(sasl_global_utils, dbh);
}

char *sasldb_path = SASL_DB_PATH;

int good_getopt(void *context __attribute__((unused)), 
		const char *plugin_name __attribute__((unused)), 
		const char *option,
		const char **result,
		unsigned *len)
{
    if (sasldb_path && !strcmp(option, "sasldb_path")) {
	*result = sasldb_path;
	if (len)
	    *len = strlen(sasldb_path);
	return SASL_OK;
    }

    return SASL_FAIL;
}

static struct sasl_callback goodsasl_cb[] = {
    { SASL_CB_GETOPT, &good_getopt, NULL },
    { SASL_CB_LIST_END, NULL, NULL }
};

int main(int argc, char **argv)
{
    int result;
    sasl_conn_t *conn;
    if (argc > 1)
	sasldb_path = argv[1];

    result = sasl_server_init(goodsasl_cb, "sasldblistusers");
    if(result != SASL_OK) {
	fprintf(stderr, "Couldn't init server\n");
	return 1;
    }
    
    result = sasl_server_new("sasldb",
			     "localhost",
			     NULL,
			     NULL,
			     NULL,
			     NULL,
			     0,
			     &conn);

    if(_sasl_check_db(sasl_global_utils, conn) != SASL_OK) {
	fprintf(stderr, "check_db unsuccessful\n");
	return 1;
    }

    if(listusers(conn) != SASL_OK) {
	fprintf(stderr, "listusers failed\n");
    }

    sasl_dispose(&conn);
    sasl_done();

    return 0;
}
