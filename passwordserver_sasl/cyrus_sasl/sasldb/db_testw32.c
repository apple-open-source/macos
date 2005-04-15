/* db_testw32.c--SASL win32 test/dummy interface
 * G. Diskin    NOTE THIS IS FOR TEST PURPOSES ONLY FOR WIN32
 * $Id: db_testw32.c,v 1.3 2004/07/07 22:53:42 snsimon Exp $
 */
/* 
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
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

#ifdef WIN32
/*
**  Disable warning messages for differences in parameter lists.
**  The Microsoft compiler spits out a warning message if a
**  function pointer is assigned to another function pointer,
**  but the formal parameter lists of the functions do not agree.
**  The assignments are compiled without modification though,
**  so it's safe to disable this warning message.
*/
#pragma warning( disable : 4113 )

#include <config.h>
#include "sasl.h"
#include "saslint.h"
#include <stdio.h>

#error "db_testw32.c uses an obsolete sasldb interface that will need to be updated when we do the win32 port of SASLv2"

/* This provides a version of _sasl_db_getsecret and
 * _sasl_db_putsecret which can be used to test the code on win32.
 * Currently the CRAM, SCRAM, and DIGEST mechanisms need to get a user's
 * secret and match to the user's input.  The win32 saslpwd program will
 * call the putsecret function to store each mech's encoding of the pw.
 *  Note that currently a file is created for each mech and the encoding
 * is stored to and retrieved from the file. */


static int
getsecret(const sasl_utils_t *utils,
	  sasl_conn_t *context __attribute__((unused)),
	  const char *auth_identity,
	  sasl_secret_t ** secret)
{
  int result = SASL_OK;
  FILE *db;
  long the_len;
  char the_secret[256],filename[100];
  int rvalue;

  if (!auth_identity || !secret)
    return SASL_FAIL;

  strcpy(filename, "c:\\tmp\\sasldata.txt");
  db = fopen(filename, "rb");

  if (! db) {
    result = SASL_FAIL;
    goto cleanup;
  }

  rvalue = fread(&the_len, sizeof(long), 1, db);
  if (ferror(db)) {
	  printf("Error reading secret length\n");
	  result = SASL_FAIL;
	  goto cleanup;
  }
  rvalue = fread(the_secret, 1, the_len, db);
  if (ferror(db)) {
	  printf("Error reading secret data\n");
	  result = SASL_FAIL;
	  goto cleanup;
  }

  fclose(db);
  
  *secret = utils->malloc(sizeof(sasl_secret_t)
			  + the_len
			  + 1);
  if (! *secret) {
    result = SASL_NOMEM;
    goto cleanup;
  }
  (*secret)->len = the_len;
  memcpy(&(*secret)->data, the_secret, the_len);
  (*secret)->data[(*secret)->len] = '\0'; /* sanity */

 cleanup:

  return result;
}

static int
putsecret(const sasl_utils_t *utils,
	  sasl_conn_t *context __attribute__((unused)),
	  const char *auth_identity,
	  const sasl_secret_t * secret)
{
  int result = SASL_OK;
  FILE *db;
  char filename[100];

  if (!auth_identity)
      return SASL_FAIL;

  strcpy(filename, "c:\\tmp\\sasldata.txt");
  db = fopen(filename, "wb");

  if (! db) {
      utils->log(NULL, SASL_LOG_ERR,
		"error opening password file. Do you have write permissions?");
    result = SASL_FAIL;
    goto cleanup;
  }

  fwrite(&secret->len, sizeof(long), 1, db);
  if (ferror(db)) {
	  printf("Error writing secret length\n");
	  exit(-1);
  }
  fwrite(secret->data, 1, secret->len, db);
  if (ferror(db)) {
	  printf("Error writing secret data\n");
	  exit(-1);
  }
  fclose(db);
 cleanup:

  return result;
}

sasl_server_getsecret_t *_sasl_db_getsecret = &getsecret;
sasl_server_putsecret_t *_sasl_db_putsecret = &putsecret;


int _sasl_check_db(const sasl_utils_t *utils, sasl_conn_t *conn)
{
    return SASL_OK;
}

/*
**  Restore the generation of code-generation warning message 4113.
*/
#pragma warning( default : 4113 )

#endif /*win32*/
