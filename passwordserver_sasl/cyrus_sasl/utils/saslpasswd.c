/* saslpasswd.c -- SASL password setting program
 * Rob Earhart
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
#include <assert.h>

#ifndef WIN32
#include <termios.h>
#include <unistd.h>
#else
#include <stdio.h>
#include <io.h>
typedef int ssize_t;
#define STDIN_FILENO stdin
#include <saslutil.h>
__declspec(dllimport) char *optarg;
__declspec(dllimport) int optind;
__declspec(dllimport) int getsubopt(char **optionp, char * const *tokens, char **valuep);
#endif /*WIN32*/
#include <sasl.h>
#include <saslplug.h>
#include "../sasldb/sasldb.h"

/* Cheating to make the utils work out right */
extern const sasl_utils_t *sasl_global_utils;

char myhostname[1025];

#define PW_BUF_SIZE 2048

static const char build_ident[] = "$Build: saslpasswd " PACKAGE "-" VERSION " $";

const char *progname = NULL;
char *sasldb_path = NULL;

void read_password(const char *prompt,
		   int flag_pipe,
		   char ** password,
		   unsigned *passlen)
{
  char buf[PW_BUF_SIZE];
#ifndef WIN32
  struct termios ts, nts;
  ssize_t n_read;
#else
  HANDLE hStdin;
  DWORD n_read, fdwMode, fdwOldMode;
  hStdin = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdin == INVALID_HANDLE_VALUE) {
	  perror(progname);
	  exit(-SASL_FAIL);
  }
#endif /*WIN32*/

  if (! flag_pipe) {
    fputs(prompt, stdout);
    fflush(stdout);
#ifndef WIN32
    tcgetattr(STDIN_FILENO, &ts);
    nts = ts;
    nts.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOCTL| ECHOPRT | ECHOKE);
    nts.c_lflag |= ICANON | ECHONL;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &nts);
#else
  if (! GetConsoleMode(hStdin, &fdwOldMode)) {
	  perror(progname);
	  exit(-SASL_FAIL);
  }
  fdwMode = fdwOldMode & ~ENABLE_ECHO_INPUT;
  if (! SetConsoleMode(hStdin, fdwMode)) {
	  perror(progname);
	  exit(-SASL_FAIL);
  }
#endif /*WIN32*/
  }

#ifndef WIN32
  n_read = read(STDIN_FILENO, buf, PW_BUF_SIZE);
  if (n_read < 0) {
#else
  if (! ReadFile(hStdin, buf, PW_BUF_SIZE, &n_read, NULL)) {
#endif /*WIN32*/

    perror(progname);
    exit(-SASL_FAIL);
  }

  if (! flag_pipe) {
#ifndef WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &ts);
    if (0 < n_read && buf[n_read - 1] != '\n') {
      /* if we didn't end with a \n, echo one */
      putchar('\n');
      fflush(stdout);
    }
#else
	SetConsoleMode(hStdin, fdwOldMode);
    putchar('\n');
    fflush(stdout);
#endif /*WIN32*/
  }

  if (0 < n_read && buf[n_read - 1] == '\n') /* if we ended with a \n */
    n_read--;			             /* remove it */

#ifdef WIN32
  /*WIN32 will have a CR in the buffer also*/
  if (0 < n_read && buf[n_read - 1] == '\r') /* if we ended with a \r */
    n_read--;			             /* remove it */
#endif /*WIN32*/

  *password = malloc(n_read + 1);
  if (! *password) {
    perror(progname);
    exit(-SASL_FAIL);
  }

  memcpy(*password, buf, n_read);
  (*password)[n_read] = '\0';	/* be nice... */
  *passlen = n_read;
}

void exit_sasl(int result, const char *errstr) __attribute__((noreturn));

void
exit_sasl(int result, const char *errstr)
{
  (void)fprintf(stderr, errstr ? "%s: %s: %s\n" : "%s: %s\n",
		progname,
		sasl_errstring(result, NULL, NULL),
		errstr);
  exit(-result);
}

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

/* returns the realm we should pretend to be in */
static int parseuser(char **user, char **realm, const char *user_realm, 
		     const char *serverFQDN, const char *input)
{
    char *r;

    assert(user && realm && serverFQDN);

    *realm = *user = NULL;

    if (!user_realm) {
	*realm = strdup(serverFQDN);
	*user = strdup(input);
    } else if (user_realm[0]) {
	*realm = strdup(user_realm);
	*user = strdup(input);
    } else {
	/* otherwise, we gotta get it from the user */
	r = strchr(input, '@');
	if (!r) {
	    /* hmmm, the user didn't specify a realm */
	    /* we'll default to the serverFQDN */
	    *realm = strdup(serverFQDN);
	    *user = strdup(input);
	} else {
	    r++;
	    *realm = strdup(r);
	    *--r = '\0';
	    *user = malloc(r - input + 1);
	    if (*user) {
		strncpy(*user, input, r - input +1);
	    } else {
		return SASL_NOMEM;
	    }
	    *r = '@';
	}
    }

    if(! *user && ! *realm ) return SASL_FAIL;
    else return SASL_OK;
}

/* this routine sets the sasldb password given a user/pass */
int _sasl_sasldb_set_pass(sasl_conn_t *conn,
			  const char *serverFQDN,
			  const char *userstr, 
			  const char *pass,
			  unsigned passlen,
			  const char *user_realm,
			  int flags)
{
    char *userid = NULL;
    char *realm = NULL;
    int ret = SASL_OK;

    ret = parseuser(&userid, &realm, user_realm, serverFQDN, userstr);
    if (ret != SASL_OK) {
	return ret;
    }

    if (pass != NULL && !(flags & SASL_SET_DISABLE)) {
	/* set the password */
	sasl_secret_t *sec = NULL;

	/* if SASL_SET_CREATE is set, we don't want to overwrite an
	   existing account */
	if (flags & SASL_SET_CREATE) {
	    ret = _sasldb_getsecret(sasl_global_utils,
				    conn, userid, realm, &sec);
	    if (ret == SASL_OK) {
		memset(sec->data, 0, sec->len);
		free(sec);
		sec = NULL;
		ret = SASL_NOCHANGE;
	    } else {
		/* Don't give up yet-- the DB might have failed because
		 * does not exist, but will be created later... */
		ret = SASL_OK;
	    }
	}
	
	/* ret == SASL_OK iff we still want to set this password */
	if (ret == SASL_OK) {
	    /* Create the sasl_secret_t */
	    sec = malloc(sizeof(sasl_secret_t) + passlen);
	    if(!sec) ret = SASL_NOMEM;
	    else {
		memcpy(sec->data, pass, passlen);
		sec->data[passlen] = '\0';
		sec->len = passlen;
	    }
	}
	if (ret == SASL_OK) {
	    ret = _sasldb_putsecret(sasl_global_utils,
				    conn, userid, realm, sec);
	}
	if ( ret != SASL_OK ) {
	    printf("Could not set secret for %s\n", userid);
	}
	if (sec) {
	    memset(sec->data, 0, sec->len);
	    free(sec);
	    sec = NULL;
	}
    } else { 
	/* SASL_SET_DISABLE specified */
	ret = _sasldb_putsecret(sasl_global_utils, conn, userid, realm, NULL);
    }

    if (userid)   free(userid);
    if (realm)    free(realm);
    return ret;
}

int
main(int argc, char *argv[])
{
  int flag_pipe = 0, flag_create = 0, flag_disable = 0, flag_error = 0;
  int flag_nouserpass = 0;
  int c;
  char *userid, *password, *verify;
  unsigned passlen, verifylen;
  const char *errstr = NULL;
  int result;
  sasl_conn_t *conn;
  char *user_domain = NULL;
  char *appname = "saslpasswd";

  memset(myhostname, 0, sizeof(myhostname));
  result = gethostname(myhostname, sizeof(myhostname)-1);
  if (result == -1) exit_sasl(SASL_FAIL, "gethostname");

  if (! argv[0])
    progname = "saslpasswd";
  else {
    progname = strrchr(argv[0], '/');
    if (progname)
      progname++;
    else
      progname = argv[0];
  }

  while ((c = getopt(argc, argv, "pcdnf:u:a:h?")) != EOF)
    switch (c) {
    case 'p':
      flag_pipe = 1;
      break;
    case 'c':
      if (flag_disable)
	flag_error = 1;
      else
	flag_create = 1;
      break;
    case 'd':
      if (flag_create)
	flag_error = 1;
      else
	flag_disable = 1;
      break;
    case 'n':
	flag_nouserpass = 1;
	break;
    case 'u':
      user_domain = optarg;
      break;
    case 'f':
      sasldb_path = optarg;
      break;
    case 'a':
      appname = optarg;
      if (strchr(optarg, '/') != NULL) {
        (void)fprintf(stderr, "filename must not contain /\n");
        exit(-SASL_FAIL);
      }
      break;
    default:
      flag_error = 1;
      break;
    }

  if (optind != argc - 1)
    flag_error = 1;

  if (flag_error) {
    (void)fprintf(stderr,
		  "%s: usage: %s [-p] [-c] [-d] [-a appname] [-f sasldb] [-u DOM] userid\n"
		  "\t-p\tpipe mode -- no prompt, password read on stdin\n"
		  "\t-c\tcreate -- ask mechs to create the account\n"
		  "\t-d\tdisable -- ask mechs to disable/delete the account\n"
		  "\t-n\tno userPassword -- don't set plaintext userPassword property\n"
		  "\t  \t                   (only set mechanism-specific secrets)\n"
		  "\t-f sasldb\tuse given file as sasldb\n"
		  "\t-a appname\tuse appname as application name\n"
		  "\t-u DOM\tuse DOM for user domain\n",
		  progname, progname);
    exit(-SASL_FAIL);
  }

  userid = argv[optind];

  result = sasl_server_init(goodsasl_cb, appname);
  if (result != SASL_OK)
    exit_sasl(result, NULL);

  result = sasl_server_new("sasldb",
			   myhostname,
			   user_domain,
			   NULL,
			   NULL,
			   NULL,
			   0,
			   &conn);
  if (result != SASL_OK)
    exit_sasl(result, NULL);
 
#ifndef WIN32
  if (! flag_pipe && ! isatty(STDIN_FILENO))
    flag_pipe = 1;
#endif /*WIN32*/

  if (!flag_disable) {
      read_password("Password: ", flag_pipe, &password, &passlen);

      if (! flag_pipe) {
	  read_password("Again (for verification): ", flag_pipe, &verify,
		  &verifylen);
	  if (passlen != verifylen
	      || memcmp(password, verify, verifylen)) {
	      fprintf(stderr, "%s: passwords don't match; aborting\n", 
		      progname);
	      exit(-SASL_BADPARAM);
	  }
      }
  }

  if(!flag_nouserpass) {
      if((result = _sasl_check_db(sasl_global_utils,conn)) == SASL_OK) {
	  result = _sasl_sasldb_set_pass(conn, myhostname, userid, password,
					 passlen, user_domain,
					 (flag_create ? SASL_SET_CREATE : 0)
					 | (flag_disable ? SASL_SET_DISABLE : 0));
      }
  }
  

  if(result != SASL_OK && !flag_disable)
      exit_sasl(result, NULL);
  else {
      int ret = 1;
      /* Either we were setting and succeeded or we were disableing and
	 failed.  In either case, we want to wipe old entries */

      /* Delete the possibly old entries */
      /* We don't care if these fail */
      ret = _sasldb_putdata(sasl_global_utils, conn,
			    userid, (user_domain ? user_domain : myhostname),
			    "cmusaslsecretCRAM-MD5", NULL, 0);
      if(ret == SASL_OK) result = ret;

      ret = _sasldb_putdata(sasl_global_utils, conn,
			    userid, (user_domain ? user_domain : myhostname),
			    "cmusaslsecretDIGEST-MD5", NULL, 0);
      if(ret == SASL_OK) result = ret;

      ret = _sasldb_putdata(sasl_global_utils, conn,
			    userid, (user_domain ? user_domain : myhostname),
			    "cmusaslsecretPLAIN", NULL, 0);
      if(ret == SASL_OK) result = ret;

#if 0
      /* Were we disableing and failed above? */
      if(result != SASL_OK)
	  exit_sasl(result, NULL);
#endif
  }
      
      
  result = sasl_setpass(conn,
			userid,
			password,
			passlen,
			NULL, 0,
			(flag_create ? SASL_SET_CREATE : 0)
			| (flag_disable ? SASL_SET_DISABLE : 0));

  if (result != SASL_OK)
    exit_sasl(result, errstr);

  sasl_dispose(&conn);
  sasl_done();

  return 0;
}

