/*
 * $Id: pam_env.c,v 1.5 2002/03/27 02:36:24 bbraun Exp $
 *
 * Written by Dave Kinchlea <kinch@kinch.ark.com> 1997/01/31
 * Inspired by Andrew Morgan <morgan@kernel.org>, who also supplied the 
 * template for this file (via pam_mail)
 *
 * Portions Copyright (C) 2002-2009 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms of Linux-PAM, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 * 
 * 1. Redistributions of source code must retain any existing copyright
 * notice, and this entire permission notice in its entirety,
 * including the disclaimer of warranties.
 * 
 * 2. Redistributions in binary form must reproduce all prior and current
 * copyright notices, this list of conditions, and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 
 * 3. The name of any author may not be used to endorse or promote
 * products derived from this software without their specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE. 
 */

#ifndef DEFAULT_CONF_FILE
#define DEFAULT_CONF_FILE       "/etc/security/pam_env.conf"
#endif

#define DEFAULT_ETC_ENVFILE     "/etc/environment"
#define DEFAULT_READ_ENVFILE    1

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH         /* This is primarily a AUTH_SETCRED module */
#define PAM_SM_SESSION      /* But I like to be friendly */
#define PAM_SM_PASSWORD     /*        ""                 */
#define PAM_SM_ACCOUNT      /*        ""                 */

#include <security/pam_modules.h>
#include <security/pam_appl.h>

/* This little structure makes it easier to keep variables together */

typedef struct var {
  char *name;
  char *value;
  char *defval;
  char *override;
} VAR;

#define BUF_SIZE 1024
#define MAX_ENV  8192

#define GOOD_LINE    0
#define BAD_LINE     100       /* This must be > the largest PAM_* error code */

#define DEFINE_VAR   101     
#define UNDEFINE_VAR 102
#define ILLEGAL_VAR  103

static int  _assemble_line(pam_handle_t *, FILE *, char *, int);
static int  _parse_line(pam_handle_t *, char *, VAR *);
static int  _check_var(pam_handle_t *, VAR *);           /* This is the real meat */
static void _clean_var(pam_handle_t *, VAR *);        
static int  _expand_arg(pam_handle_t *, char **);
static const char * _pam_get_item_byname(pam_handle_t *, const char *);
static int  _define_var(pam_handle_t *, VAR *);
static int  _undefine_var(pam_handle_t *, VAR *);

/* This is a flag used to designate an empty string */
static char quote='Z';         

/* argument parsing */

#define PAM_DEBUG_ARG       0x01
#define PAM_NEW_CONF_FILE   0x02
#define PAM_ENV_SILENT      0x04
#define PAM_NEW_ENV_FILE    0x10

static int _pam_parse(pam_handle_t *pamh, int flags, int argc, const char **argv, char **conffile,
	char **envfile, int *readenv)
{
    int ctrl=0;
	const char *readenvchar = NULL;

	/* generic options */

	if (NULL != openpam_get_option(pamh, "debug")) {
		openpam_log(PAM_LOG_DEBUG, "debugging on");
		ctrl |= PAM_DEBUG_ARG;
	}
	if (NULL != (*conffile = (char *)openpam_get_option(pamh, "conffile"))) {
		openpam_log(PAM_LOG_DEBUG, "new Configuration File: %s", *conffile);
		ctrl |= PAM_NEW_CONF_FILE;
	}
	if (NULL != (*envfile = (char *)openpam_get_option(pamh, "conffile"))) {
		openpam_log(PAM_LOG_DEBUG, "new Env File: %s", *envfile);
		ctrl |= PAM_NEW_ENV_FILE;
	}
	readenvchar = (char *)openpam_get_option(pamh, "readenv");
	if (NULL != readenvchar && 0 != (*readenv = atoi(readenvchar))) {
		openpam_log(PAM_LOG_DEBUG, "readenv: %d", *readenv);
	}

    return ctrl;
}

static int _parse_config_file(pam_handle_t *pamh, int ctrl, char **conffile)
{
    int retval;
    const char *file;
    char buffer[BUF_SIZE];
    FILE *conf;
    VAR Var, *var=&Var;   

    var->name=NULL; var->defval=NULL; var->override=NULL;
    openpam_log(PAM_LOG_DEBUG, "Called.");

    if (ctrl & PAM_NEW_CONF_FILE) {
	file = *conffile;
    } else {
	file = DEFAULT_CONF_FILE;
    }

    openpam_log(PAM_LOG_DEBUG, "Config file name is: %s", file);

    /* 
     * Lets try to open the config file, parse it and process 
     * any variables found.
     */

    if ((conf = fopen(file,"r")) == NULL) {
      openpam_log(PAM_LOG_ERROR, "Unable to open config file: %s",
	       strerror(errno));
      return PAM_IGNORE;
    }

    /* _pam_assemble_line will provide a complete line from the config file, with all 
     * comments removed and any escaped newlines fixed up
     */

    while (( retval = _assemble_line(pamh, conf, buffer, BUF_SIZE)) > 0) {
      openpam_log(PAM_LOG_DEBUG, "Read line: %s", buffer);

      if ((retval = _parse_line(pamh, buffer, var)) == GOOD_LINE) {
	retval = _check_var(pamh, var);

	if (DEFINE_VAR == retval) {
	  retval = _define_var(pamh, var);     

	} else if (UNDEFINE_VAR == retval) {
	  retval = _undefine_var(pamh, var);   
	} 
      } 
      if (PAM_SUCCESS != retval && ILLEGAL_VAR != retval 
	  && BAD_LINE != retval) break;
      
      _clean_var(pamh, var);    

    }  /* while */
    
    (void) fclose(conf);

    /* tidy up */
    _clean_var(pamh, var);        /* We could have got here prematurely, this is safe though */
    if (NULL != conffile) {
        memset(conffile, 0, sizeof(*conffile));
        *conffile = NULL;
    }
    file = NULL;
    openpam_log(PAM_LOG_DEBUG, "Exit.");
    return (retval<0?PAM_ABORT:PAM_SUCCESS);
}

static int _parse_env_file(pam_handle_t *pamh, int ctrl, char **env_file)
{
    int retval=PAM_SUCCESS, i, t;
    const char *file;
    char buffer[BUF_SIZE], *key, *mark;
    FILE *conf;

    if (ctrl & PAM_NEW_ENV_FILE)
	file = *env_file;
    else
	file = DEFAULT_ETC_ENVFILE;

    openpam_log(PAM_LOG_DEBUG, "Env file name is: %s", file);

    if ((conf = fopen(file,"r")) == NULL) {
      openpam_log(PAM_LOG_DEBUG, "Unable to open env file: %s", strerror(errno));
      return PAM_ABORT;
    }

    while (_assemble_line(pamh, conf, buffer, BUF_SIZE) > 0) {
	openpam_log(PAM_LOG_DEBUG, "Read line: %s", buffer);
	key = buffer;

	/* skip leading white space */
	key += strspn(key, " \n\t");

	/* skip blanks lines and comments */
	if (!key || key[0] == '#')
	    continue;

	/* skip over "export " if present so we can be compat with
	   bash type declerations */
	if (strncmp(key, "export ", (size_t) 7) == 0)
	    key += 7;

	/* now find the end of value */
	mark = key;
	while(mark[0] != '\n' && mark[0] != '#' && mark[0] != '\0')
	    mark++;
	if (mark[0] != '\0')
	    mark[0] = '\0';

       /*
	* sanity check, the key must be alpha-numeric
	*/

	for ( i = 0 ; key[i] != '=' && key[i] != '\0' ; i++ )
	if (!isalnum(key[i]) && key[i] != '_') {
		openpam_log(PAM_LOG_DEBUG, "key is not alpha numeric - '%s', ignoring", key);
		continue;
	    }

	/* now we try to be smart about quotes around the value,
	   but not too smart, we can't get all fancy with escaped
	   values like bash */
	if (key[i] == '=' && (key[++i] == '\"' || key[i] == '\'')) {
	    for ( t = i+1 ; key[t] != '\0' ; t++)
		if (key[t] != '\"' && key[t] != '\'')
		    key[i++] = key[t];
		else if (key[t+1] != '\0')
		    key[i++] = key[t];
	    key[i] = '\0';
	}

	/* set the env var, if it fails, we break out of the loop */
	retval = pam_putenv(pamh, key);
	if (retval != PAM_SUCCESS) {
	    openpam_log(PAM_LOG_DEBUG, "error setting env \"%s\"", key);
	    break;
	}
    }
    
    (void) fclose(conf);

    /* tidy up */
	if (NULL != env_file) {
        memset(env_file, 0, sizeof(*env_file));
        *env_file = NULL;
    }
    file = NULL;
    openpam_log(PAM_LOG_DEBUG, "Exit.");
    return (retval<0?PAM_IGNORE:PAM_SUCCESS);
}

/*
 * This is where we read a line of the PAM config file. The line may be
 * preceeded by lines of comments and also extended with "\\\n"
 */

static int _assemble_line(pam_handle_t *pamh, FILE *f, char *buffer, int buf_len)
{
    char *p = buffer;
    char *s, *os;
    int used = 0;

    /* loop broken with a 'break' when a non-'\\n' ended line is read */

    openpam_log(PAM_LOG_DEBUG, "called.");
    for (;;) {
	if (used >= buf_len) {
	    /* Overflow */
	    openpam_log(PAM_LOG_DEBUG, "_assemble_line: overflow");
	    return -1;
	}
	if (fgets(p, buf_len - used, f) == NULL) {
	    if (used) {
		/* Incomplete read */
		return -1;
	    } else {
		/* EOF */
		return 0;
	    }
	}

	/* skip leading spaces --- line may be blank */

	s = p + strspn(p, " \n\t");
	if (*s && (*s != '#')) {
	    os = s;

	    /*
	     * we are only interested in characters before the first '#'
	     * character
	     */

	    while (*s && *s != '#')
		 ++s;
	    if (*s == '#') {
		 *s = '\0';
		 used += strlen(os);
		 break;                /* the line has been read */
	    }

	    s = os;

	    /*
	     * Check for backslash by scanning back from the end of
	     * the entered line, the '\n' has been included since
	     * normally a line is terminated with this
	     * character. fgets() should only return one though!
	     */

	    s += strlen(s);
	    while (s > os && ((*--s == ' ') || (*s == '\t')
			      || (*s == '\n')));

	    /* check if it ends with a backslash */
	    if (*s == '\\') {
		*s = '\0';              /* truncate the line here */
		used += strlen(os);
		p = s;                  /* there is more ... */
	    } else {
		/* End of the line! */
		used += strlen(os);
		break;                  /* this is the complete line */
	    }

	} else {
	    /* Nothing in this line */
	    /* Don't move p         */
	}
    }

    return used;
}

static int _parse_line(pam_handle_t *pamh, char *buffer, VAR *var)
{
  /* 
   * parse buffer into var, legal syntax is 
   * VARIABLE [DEFAULT=[[string]] [OVERRIDE=[value]]
   *
   * Any other options defined make this a bad line, 
   * error logged and no var set
   */
  
  int length, quoteflg=0;
  char *ptr, **valptr, *tmpptr; 
  
  openpam_log(PAM_LOG_DEBUG, "Called buffer = <%s>", buffer);

  length = strcspn(buffer," \t\n");
  
  if ((var->name = malloc(length + 1)) == NULL) {
    openpam_log(PAM_LOG_ERROR, "Couldn't malloc %d bytes", length+1);
    return PAM_BUF_ERR;
  }
  
  /* 
   * The first thing on the line HAS to be the variable name, 
   * it may be the only thing though.
   */
  strncpy(var->name, buffer, length);
  var->name[length] = '\0';
  openpam_log(PAM_LOG_DEBUG, "var->name = <%s>, length = %d", var->name, length);

  /* 
   * Now we check for arguments, we only support two kinds and ('cause I am lazy)
   * each one can actually be listed any number of times
   */
  
  ptr = buffer+length;
  while ((length = strspn(ptr, " \t")) > 0) { 
    ptr += length;                              /* remove leading whitespace */
    openpam_log(PAM_LOG_DEBUG, "ptr: %s", ptr);
    if (strncmp(ptr,"DEFAULT=",8) == 0) {
      ptr+=8;
      openpam_log(PAM_LOG_DEBUG, "Default arg found: <%s>", ptr);
      valptr=&(var->defval);
    } else if (strncmp(ptr, "OVERRIDE=", 9) == 0) {
      ptr+=9;
      openpam_log(PAM_LOG_DEBUG, "Override arg found: <%s>", ptr);
      valptr=&(var->override);
    } else {
      openpam_log(PAM_LOG_DEBUG, "Unrecognized options: <%s> - ignoring line", ptr);
      openpam_log(PAM_LOG_ERROR, "Unrecognized Option: %s - ignoring line", ptr);
      return BAD_LINE;
    }
    
    if ('"' != *ptr) {       /* Escaped quotes not supported */
      length = strcspn(ptr, " \t\n");
      tmpptr = ptr+length;
    } else {
      tmpptr = strchr(++ptr, '"');   
      if (!tmpptr) {
	openpam_log(PAM_LOG_DEBUG, "Unterminated quoted string: %s", ptr-1);
	openpam_log(PAM_LOG_ERROR, "Unterminated quoted string: %s", ptr-1);
	return BAD_LINE;
      }
      length = tmpptr - ptr;        
      if (*++tmpptr && ' ' != *tmpptr && '\t' != *tmpptr && '\n' != *tmpptr) {
	openpam_log(PAM_LOG_DEBUG, "Quotes must cover the entire string: <%s>", ptr);
	openpam_log(PAM_LOG_ERROR, "Quotes must cover the entire string: <%s>", ptr);
	return BAD_LINE;
      }
      quoteflg++;
    }
    if (length) {
      if ((*valptr = malloc(length + 1)) == NULL) {
	openpam_log(PAM_LOG_DEBUG, "Couldn't malloc %d bytes", length+1);
	openpam_log(PAM_LOG_ERROR, "Couldn't malloc %d bytes", length+1);
	return PAM_BUF_ERR;
      }
      (void)strncpy(*valptr,ptr,length);
      (*valptr)[length]='\0';
    } else if (quoteflg--) {
      *valptr = &quote;      /* a quick hack to handle the empty string */
    }
    ptr = tmpptr;         /* Start the search where we stopped */
  } /* while */
  
  /* 
   * The line is parsed, all is well.
   */
  
  openpam_log(PAM_LOG_DEBUG, "Exit.");
  ptr = NULL; tmpptr = NULL; valptr = NULL;
  return GOOD_LINE;
}

static int _check_var(pam_handle_t *pamh, VAR *var)
{
  /* 
   * Examine the variable and determine what action to take. 
   * Returns DEFINE_VAR, UNDEFINE_VAR depending on action to take
   * or a PAM_* error code if passed back from other routines
   *
   * if no DEFAULT provided, the empty string is assumed
   * if no OVERRIDE provided, the empty string is assumed
   * if DEFAULT=  and OVERRIDE evaluates to the empty string, 
   *    this variable should be undefined
   * if DEFAULT=""  and OVERRIDE evaluates to the empty string, 
   *    this variable should be defined with no value
   * if OVERRIDE=value   and value turns into the empty string, DEFAULT is used
   *
   * If DEFINE_VAR is to be returned, the correct value to define will
   * be pointed to by var->value
   */

  int retval;

  openpam_log(PAM_LOG_DEBUG, "Called.");

  /*
   * First thing to do is to expand any arguments, but only
   * if they are not the special quote values (cause expand_arg
   * changes memory).
   */

  if (var->defval && (&quote != var->defval) &&
      ((retval = _expand_arg(pamh, &(var->defval))) != PAM_SUCCESS)) {
      return retval;
  }
  if (var->override && (&quote != var->override) &&
      ((retval = _expand_arg(pamh, &(var->override))) != PAM_SUCCESS)) {
    return retval;
  }

  /* Now its easy */
  
  if (var->override && *(var->override) && &quote != var->override) { 
    /* if there is a non-empty string in var->override, we use it */
    openpam_log(PAM_LOG_DEBUG, "OVERRIDE variable <%s> being used: <%s>", var->name, var->override);
    var->value = var->override;
    retval = DEFINE_VAR;
  } else {
    
    var->value = var->defval;
    if (&quote == var->defval) {
      /* 
       * This means that the empty string was given for defval value 
       * which indicates that a variable should be defined with no value
       */
      *var->defval = '\0';
      openpam_log(PAM_LOG_DEBUG, "An empty variable: <%s>", var->name);
      retval = DEFINE_VAR;
    } else if (var->defval) {
      openpam_log(PAM_LOG_DEBUG, "DEFAULT variable <%s> being used: <%s>", var->name, var->defval);
      retval = DEFINE_VAR;
    } else {
      openpam_log(PAM_LOG_DEBUG, "UNDEFINE variable <%s>", var->name);
      retval = UNDEFINE_VAR;
    }
  }

  openpam_log(PAM_LOG_DEBUG, "Exit.");
  return retval;
}

static int _expand_arg(pam_handle_t *pamh, char **value)
{
  const char *orig=*value, *tmpptr=NULL;
  char *ptr;       /* 
		    * Sure would be nice to use tmpptr but it needs to be 
		    * a constant so that the compiler will shut up when I
		    * call pam_getenv and _pam_get_item_byname -- sigh
		    */
		      
  /* No unexpanded variable can be bigger than BUF_SIZE */
  char type, tmpval[BUF_SIZE];

  /* I know this shouldn't be hard-coded but it's so much easier this way */
  char tmp[MAX_ENV];

  openpam_log(PAM_LOG_DEBUG, "Remember to initialize tmp!");
  memset(tmp, 0, MAX_ENV);

  /* 
   * (possibly non-existent) environment variables can be used as values
   * by prepending a "$" and wrapping in {} (ie: ${HOST}), can escape with "\"
   * (possibly non-existent) PAM items can be used as values 
   * by prepending a "@" and wrapping in {} (ie: @{PAM_RHOST}, can escape 
   *
   */
  openpam_log(PAM_LOG_DEBUG, "Expanding <%s>",orig);
  while (*orig) {     /* while there is some input to deal with */
    if ('\\' == *orig) {
      ++orig;
      if ('$' != *orig && '@' != *orig) {
	openpam_log(PAM_LOG_DEBUG, "Unrecognized escaped character: <%c> - ignoring", *orig);
	openpam_log(PAM_LOG_ERROR, "Unrecognized escaped character: <%c> - ignoring",
		 *orig);
      } else if ((strlen(tmp) + 1) < MAX_ENV) {
	tmp[strlen(tmp)] = *orig++;        /* Note the increment */
      } else {
	/* is it really a good idea to try to log this? */
	openpam_log(PAM_LOG_DEBUG, "Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
	openpam_log(PAM_LOG_ERROR, "Variable buffer overflow: <%s> + <%s>",
		 tmp, tmpptr);
      }
      continue;
    } 
    if ('$' == *orig || '@' == *orig) {
      if ('{' != *(orig+1)) {
	openpam_log(PAM_LOG_DEBUG, "Expandable variables must be wrapped in {}"
	   " <%s> - ignoring", orig);
	openpam_log(PAM_LOG_ERROR, "Expandable variables must be wrapped in {}"
		 " <%s> - ignoring", orig);
	if ((strlen(tmp) + 1) < MAX_ENV) {
	  tmp[strlen(tmp)] = *orig++;        /* Note the increment */
	}
	continue;
      } else {
	openpam_log(PAM_LOG_DEBUG, "Expandable argument: <%s>", orig);
	type = *orig;
	orig+=2;     /* skip the ${ or @{ characters */
	ptr = strchr(orig, '}');
	if (ptr) { 
	  *ptr++ = '\0';
	} else {
	  openpam_log(PAM_LOG_DEBUG, "Unterminated expandable variable: <%s>", orig-2);
	  openpam_log(PAM_LOG_ERROR, "Unterminated expandable variable: <%s>", orig-2);
	  return PAM_ABORT;
	}
	strncpy(tmpval, orig, sizeof(tmpval));
	tmpval[sizeof(tmpval)-1] = '\0';
	orig=ptr;
	/* 
	 * so, we know we need to expand tmpval, it is either 
	 * an environment variable or a PAM_ITEM. type will tell us which
	 */
	switch (type) {
	  
	case '$':
	  openpam_log(PAM_LOG_DEBUG, "Expanding env var: <%s>",tmpval);
	  tmpptr = pam_getenv(pamh, tmpval);
	  openpam_log(PAM_LOG_DEBUG, "Expanded to <%s>", tmpptr);
	  break;
	  
	case '@':
	  openpam_log(PAM_LOG_DEBUG, "Expanding pam item: <%s>",tmpval);
	  tmpptr = _pam_get_item_byname(pamh, tmpval);
	  openpam_log(PAM_LOG_DEBUG, "Expanded to <%s>", tmpptr);
	  break;

	default:
	  openpam_log(PAM_LOG_DEBUG, "Impossible error, type == <%c>", type);
	  openpam_log(PAM_LOG_ERROR, "Impossible error, type == <%c>", type);
	  return PAM_ABORT;
	}         /* switch */
	
	if (tmpptr) {
	  if (strlcat(tmp, tmpptr, MAX_ENV) >= MAX_ENV) {
	    /* is it really a good idea to try to log this? */
	    openpam_log(PAM_LOG_DEBUG, "Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
	    openpam_log(PAM_LOG_ERROR, "Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
	  }
	}
      }           /* if ('{' != *orig++) */
    } else {      /* if ( '$' == *orig || '@' == *orig) */
      if ((strlen(tmp) + 1) < MAX_ENV) {
	tmp[strlen(tmp)] = *orig++;        /* Note the increment */
      } else {
	/* is it really a good idea to try to log this? */
	openpam_log(PAM_LOG_DEBUG, "Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
	openpam_log(PAM_LOG_ERROR, "Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
      }
    }
  }              /* for (;*orig;) */

  size_t tmp_len = strlen(tmp);
  if (tmp_len > strlen(*value)) {
    free(*value);
    if ((*value = malloc(tmp_len+1)) == NULL) {
      openpam_log(PAM_LOG_DEBUG, "Couldn't malloc %lu bytes for expanded var", tmp_len+1);
      openpam_log(PAM_LOG_ERROR,"Couldn't malloc %lu bytes for expanded var",
	       tmp_len+1);
      return PAM_BUF_ERR;
    }
  }
  strlcpy(*value, tmp, tmp_len+1);
  openpam_log(PAM_LOG_DEBUG, "Exit.");

  return PAM_SUCCESS;
}

static const char * _pam_get_item_byname(pam_handle_t *pamh, const char *name)
{
  /* 
   * This function just allows me to use names as given in the config
   * file and translate them into the appropriate PAM_ITEM macro
   */

  int item;
  const char *itemval;

  openpam_log(PAM_LOG_DEBUG, "Called.");
  if (strcmp(name, "PAM_USER") == 0) {
    item = PAM_USER;
  } else if (strcmp(name, "PAM_USER_PROMPT") == 0) {
    item = PAM_USER_PROMPT;
  } else if (strcmp(name, "PAM_TTY") == 0) {
    item = PAM_TTY;
  } else if (strcmp(name, "PAM_RUSER") == 0) {
    item = PAM_RUSER;
  } else if (strcmp(name, "PAM_RHOST") == 0) {
    item = PAM_RHOST;
  } else {
    openpam_log(PAM_LOG_DEBUG, "Unknown PAM_ITEM: <%s>", name);
    openpam_log(PAM_LOG_ERROR, "Unknown PAM_ITEM: <%s>", name);
    return NULL;
  }
  
  if (pam_get_item(pamh, item, (const void **)&itemval) != PAM_SUCCESS) {
    openpam_log(PAM_LOG_DEBUG, "pam_get_item failed");
    return NULL;     /* let pam_get_item() log the error */
  }
  openpam_log(PAM_LOG_DEBUG, "Exit.");
  return itemval;
}

static int _define_var(pam_handle_t *pamh, VAR *var)
{
  /* We have a variable to define, this is a simple function */
  
  char *envvar;
  int size, retval=PAM_SUCCESS;
  
  openpam_log(PAM_LOG_DEBUG, "Called.");
  size = strlen(var->name)+strlen(var->value)+2;
  if ((envvar = malloc(size)) == NULL) {
    openpam_log(PAM_LOG_DEBUG, "Malloc fail, size = %d", size);
    openpam_log(PAM_LOG_ERROR, "Malloc fail, size = %d", size);
    return PAM_BUF_ERR;
  }
  (void) snprintf(envvar,size,"%s=%s",var->name,var->value);
  retval = pam_putenv(pamh, envvar);
  free(envvar); envvar=NULL;
  openpam_log(PAM_LOG_DEBUG, "Exit.");
  return retval;
}

static int _undefine_var(pam_handle_t *pamh, VAR *var)
{
  /* We have a variable to undefine, this is a simple function */
  
  openpam_log(PAM_LOG_DEBUG, "Called and exit.");
  return pam_putenv(pamh, var->name);
}

static void   _clean_var(pam_handle_t *pamh, VAR *var)
{
    if (var->name) {
      free(var->name); 
    }
    if (var->defval && (&quote != var->defval)) {
      free(var->defval); 
    }
    if (var->override && (&quote != var->override)) {
      free(var->override); 
    }
    var->name = NULL;
    var->value = NULL;    /* never has memory specific to it */
    var->defval = NULL;
    var->override = NULL;
    return;
}



/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
			const char **argv)
{ 
  return PAM_IGNORE;
}

PAM_EXTERN 
int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, 
		   const char **argv)
{
  int retval, ctrl, readenv=DEFAULT_READ_ENVFILE;
  char *conf_file=NULL, *env_file=NULL;

  /*
   * this module sets environment variables read in from a file
   */
  
  openpam_log(PAM_LOG_DEBUG, "Called.");
  ctrl = _pam_parse(pamh, flags, argc, argv, &conf_file, &env_file, &readenv);

  retval = _parse_config_file(pamh, ctrl, &conf_file);

  if(readenv)
    _parse_env_file(pamh, ctrl, &env_file);

  /* indicate success or failure */
  
  openpam_log(PAM_LOG_DEBUG, "Exit.");
  return retval;
}

PAM_EXTERN 
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, 
		     const char **argv)
{
  openpam_log(PAM_LOG_NOTICE, "pam_sm_acct_mgmt called inappropriatly");
  return PAM_SERVICE_ERR;
}
 
PAM_EXTERN
int pam_sm_open_session(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
  int retval, ctrl, readenv=DEFAULT_READ_ENVFILE;
  char *conf_file=NULL, *env_file=NULL;
  
  /*
   * this module sets environment variables read in from a file
   */
  
  openpam_log(PAM_LOG_DEBUG, "Called.");
  ctrl = _pam_parse(pamh, flags, argc, argv, &conf_file, &env_file, &readenv);
  
  retval = _parse_config_file(pamh, ctrl, &conf_file);
  
  if(readenv)
    _parse_env_file(pamh, ctrl, &env_file);

  /* indicate success or failure */
  
  openpam_log(PAM_LOG_DEBUG, "Exit.");
  return retval;
}

PAM_EXTERN
int pam_sm_close_session(pam_handle_t *pamh,int flags,int argc,
			 const char **argv)
{
  openpam_log(PAM_LOG_DEBUG, "Called and Exit");
  return PAM_SUCCESS;
}

PAM_EXTERN 
int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, 
		     const char **argv)
{
  openpam_log(PAM_LOG_NOTICE, "pam_sm_chauthtok called inappropriatly");
  return PAM_SERVICE_ERR;
}

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_env_modstruct = {
     "pam_env",
     pam_sm_authenticate,
     pam_sm_setcred,
     pam_sm_acct_mgmt,
     pam_sm_open_session,
     pam_sm_close_session,
     pam_sm_chauthtok,
};

#endif

/* end of module definition */
