/* Generic SASL plugin utility functions
 * Rob Siemborski
 * $Id: plugin_common.c,v 1.3 2006/02/03 22:33:14 snsimon Exp $
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
#ifndef macintosh
#ifdef WIN32
# include <winsock.h>
#else
# include <sys/param.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif /* WIN32 */
#endif /* macintosh */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sasl.h>
#include <saslutil.h>
#include <saslplug.h>

#include <errno.h>
#include <ctype.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "plugin_common.h"

/* translate IPv4 mapped IPv6 address to IPv4 address */
static void sockaddr_unmapped(
#ifdef IN6_IS_ADDR_V4MAPPED
  struct sockaddr *sa, socklen_t *len
#else
  struct sockaddr *sa __attribute__((unused)),
  socklen_t *len __attribute__((unused))
#endif
)
{
#ifdef IN6_IS_ADDR_V4MAPPED
    struct sockaddr_in6 *sin6;
    struct sockaddr_in *sin4;
    uint32_t addr;
    int port;

    if (sa->sa_family != AF_INET6)
	return;
    sin6 = (struct sockaddr_in6 *)sa;
    if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
	return;
    sin4 = (struct sockaddr_in *)sa;
    addr = *(uint32_t *)&sin6->sin6_addr.s6_addr[12];
    port = sin6->sin6_port;
    memset(sin4, 0, sizeof(struct sockaddr_in));
    sin4->sin_addr.s_addr = addr;
    sin4->sin_port = port;
    sin4->sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_SA_LEN
    sin4->sin_len = sizeof(struct sockaddr_in);
#endif
    *len = sizeof(struct sockaddr_in);
#else
    return;
#endif
}

int _plug_ipfromstring(const sasl_utils_t *utils, const char *addr,
		       struct sockaddr *out, socklen_t outlen) 
{
    int i, j;
    socklen_t len;
    struct sockaddr_storage ss;
    struct addrinfo hints, *ai = NULL;
    char hbuf[NI_MAXHOST];
    
    if(!utils || !addr || !out) {
	if(utils) PARAMERROR( utils );
	return SASL_BADPARAM;
    }

    /* Parse the address */
    for (i = 0; addr[i] != '\0' && addr[i] != ';'; i++) {
	if (i >= NI_MAXHOST) {
	    if(utils) PARAMERROR( utils );
	    return SASL_BADPARAM;
	}
	hbuf[i] = addr[i];
    }
    hbuf[i] = '\0';

    if (addr[i] == ';')
	i++;
    /* XXX/FIXME: Do we need this check? */
    for (j = i; addr[j] != '\0'; j++)
	if (!isdigit((int)(addr[j]))) {
	    PARAMERROR( utils );
	    return SASL_BADPARAM;
	}

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

    if (getaddrinfo(hbuf, &addr[i], &hints, &ai) != 0) {	
	PARAMERROR( utils );
	return SASL_BADPARAM;
    }

    len = ai->ai_addrlen;
    memcpy(&ss, ai->ai_addr, len);
    freeaddrinfo(ai);
    sockaddr_unmapped((struct sockaddr *)&ss, &len);
    if (outlen < len) {
	PARAMERROR( utils );
	return SASL_BUFOVER;
    }

    memcpy(out, &ss, len);

    return SASL_OK;
}

int _plug_iovec_to_buf(const sasl_utils_t *utils, const struct iovec *vec,
		       unsigned numiov, buffer_info_t **output) 
{
    unsigned i;
    int ret;
    buffer_info_t *out;
    unsigned char *pos;

    if(!utils || !vec || !output) {
	if(utils) PARAMERROR( utils );
	return SASL_BADPARAM;
    }
    
    if(!(*output)) {
	*output = utils->malloc(sizeof(buffer_info_t));
	if(!*output) {
	    MEMERROR(utils);
	    return SASL_NOMEM;
	}
	memset(*output,0,sizeof(buffer_info_t));
    }

    out = *output;
    
    out->curlen = 0;
    for(i=0; i<numiov; i++)
	out->curlen += vec[i].iov_len;

    ret = _plug_buf_alloc(utils, &out->data, &out->reallen, out->curlen);

    if(ret != SASL_OK) {
	MEMERROR(utils);
	return SASL_NOMEM;
    }
    
    memset(out->data, 0, out->reallen);
    pos = out->data;
    
    for(i=0; i<numiov; i++) {
		memcpy(pos, vec[i].iov_base, vec[i].iov_len);
		pos += vec[i].iov_len;
    }
	
    return SASL_OK;
}

/* Basically a conditional call to realloc(), if we need more */
int _plug_buf_alloc(const sasl_utils_t *utils, unsigned char **rwbuf,
		    unsigned int *curlen, unsigned int newlen) 
{
    if(!utils || !rwbuf || !curlen) {
	PARAMERROR(utils);
	return SASL_BADPARAM;
    }

    if(!(*rwbuf)) {
	*rwbuf = utils->malloc(newlen);
	if (*rwbuf == NULL) {
	    *curlen = 0;
	    MEMERROR(utils);
	    return SASL_NOMEM;
	}
	*curlen = newlen;
    } else if(*rwbuf && *curlen < newlen) {
	size_t needed = 2*(*curlen);

	while(needed < newlen)
	    needed *= 2;

	*rwbuf = utils->realloc(*rwbuf, needed);
	if (*rwbuf == NULL) {
	    *curlen = 0;
	    MEMERROR(utils);
	    return SASL_NOMEM;
	}
	*curlen = needed;
    } 

    return SASL_OK;
}

/* copy a string */
int _plug_strdup(const sasl_utils_t * utils, const char *in,
		 char **out, int *outlen)
{
  size_t len = strlen(in);

  if(!utils || !in || !out) {
      if(utils) PARAMERROR(utils);
      return SASL_BADPARAM;
  }

  *out = utils->malloc(len + 1);
  if (!*out) {
      MEMERROR(utils);
      return SASL_NOMEM;
  }

  strlcpy((char *) *out, in, len+1);

  if (outlen)
      *outlen = len;

  return SASL_OK;
}

void _plug_free_string(const sasl_utils_t *utils, char **str)
{
  size_t len;

  if (!utils || !str || !(*str)) return;

  len = strlen(*str);

  utils->erasebuffer(*str, len);
  utils->free(*str);

  *str=NULL;
}

void _plug_free_secret(const sasl_utils_t *utils, sasl_secret_t **secret) 
{
    if(!utils || !secret || !(*secret)) return;

    utils->erasebuffer((char *)(*secret)->data, (*secret)->len);
    utils->free(*secret);
    *secret = NULL;
}

/* 
 * Trys to find the prompt with the lookingfor id in the prompt list
 * Returns it if found. NULL otherwise
 */
sasl_interact_t *_plug_find_prompt(sasl_interact_t **promptlist,
				   unsigned int lookingfor)
{
    sasl_interact_t *prompt;

    if (promptlist && *promptlist) {
	for (prompt = *promptlist; prompt->id != SASL_CB_LIST_END; ++prompt) {
	    if (prompt->id==lookingfor)
		return prompt;
	}
    }

    return NULL;
}

/*
 * Retrieve the simple string given by the callback id.
 */
int _plug_get_simple(const sasl_utils_t *utils, unsigned int id,
		     const char **result, sasl_interact_t **prompt_need)
{

    int ret;
    sasl_getsimple_t *simple_cb;
    void *simple_context;
    sasl_interact_t *prompt;

    /* see if we were given the result in the prompt */
    prompt = _plug_find_prompt(prompt_need, id);
    if (prompt != NULL) {
	/* We prompted, and got.*/
	
	if (!prompt->result) {
	    SETERROR(utils, "Unexpectedly missing a prompt result");
	    return SASL_BADPARAM;
	}

	*result = prompt->result;
	return SASL_OK;
    }
  
    /* Try to get the callback... */
    ret = utils->getcallback(utils->conn, id, &simple_cb, &simple_context);

    if (ret == SASL_OK && simple_cb) {
	ret = simple_cb(simple_context, id, result, NULL);
	if (ret != SASL_OK)
	    return ret;

	if (!*result) {
	    PARAMERROR(utils);
	    return SASL_BADPARAM;
	}
    }
  
    return ret;
}

/*
 * Retrieve the user password.
 */
int _plug_get_password(const sasl_utils_t *utils, sasl_secret_t **password,
		       unsigned int *iscopy, sasl_interact_t **prompt_need)
{
    int ret;
    sasl_getsecret_t *pass_cb;
    void *pass_context;
    sasl_interact_t *prompt;

    *iscopy = 0;

    /* see if we were given the password in the prompt */
    prompt = _plug_find_prompt(prompt_need, SASL_CB_PASS);
    if (prompt != NULL) {
	/* We prompted, and got.*/
	
	if (!prompt->result) {
	    SETERROR(utils, "Unexpectedly missing a prompt result");
	    return SASL_BADPARAM;
	}
      
	/* copy what we got into a secret_t */
	*password = (sasl_secret_t *) utils->malloc(sizeof(sasl_secret_t) +
						    prompt->len + 1);
	if (!*password) {
	    MEMERROR(utils);
	    return SASL_NOMEM;
	}
      
	(*password)->len=prompt->len;
	memcpy((*password)->data, prompt->result, prompt->len);
	(*password)->data[(*password)->len]=0;

	*iscopy = 1;

	return SASL_OK;
    }

    /* Try to get the callback... */
    ret = utils->getcallback(utils->conn, SASL_CB_PASS,
			     &pass_cb, &pass_context);

    if (ret == SASL_OK && pass_cb) {
	ret = pass_cb(utils->conn, pass_context, SASL_CB_PASS, password);
	if (ret != SASL_OK)
	    return ret;

	if (!*password) {
	    PARAMERROR(utils);
	    return SASL_BADPARAM;
	}
    }

    return ret;
}

/*
 * Retrieve the string given by the challenge prompt id.
 */
int _plug_challenge_prompt(const sasl_utils_t *utils, unsigned int id,
			   const char *challenge, const char *promptstr,
			   const char **result, sasl_interact_t **prompt_need)
{
    int ret;
    sasl_chalprompt_t *chalprompt_cb;
    void *chalprompt_context;
    sasl_interact_t *prompt;

    /* see if we were given the password in the prompt */
    prompt = _plug_find_prompt(prompt_need, id);
    if (prompt != NULL) {
	/* We prompted, and got.*/
	
	if (!prompt->result) {
	    SETERROR(utils, "Unexpectedly missing a prompt result");
	    return SASL_BADPARAM;
	}
      
	*result = prompt->result;
	return SASL_OK;
    }

    /* Try to get the callback... */
    ret = utils->getcallback(utils->conn, id,
			     &chalprompt_cb, &chalprompt_context);

    if (ret == SASL_OK && chalprompt_cb) {
	ret = chalprompt_cb(chalprompt_context, id,
			    challenge, promptstr, NULL, result, NULL);
	if (ret != SASL_OK)
	    return ret;

	if (!*result) {
	    PARAMERROR(utils);
	    return SASL_BADPARAM;
	}
    }

    return ret;
}

/*
 * Retrieve the client realm.
 */
int _plug_get_realm(const sasl_utils_t *utils, const char **availrealms,
		    const char **realm, sasl_interact_t **prompt_need)
{
    int ret;
    sasl_getrealm_t *realm_cb;
    void *realm_context;
    sasl_interact_t *prompt;

    /* see if we were given the result in the prompt */
    prompt = _plug_find_prompt(prompt_need, SASL_CB_GETREALM);
    if (prompt != NULL) {
	/* We prompted, and got.*/
	
	if (!prompt->result) {
	    SETERROR(utils, "Unexpectedly missing a prompt result");
	    return SASL_BADPARAM;
	}

	*realm = prompt->result;
	return SASL_OK;
    }

    /* Try to get the callback... */
    ret = utils->getcallback(utils->conn, SASL_CB_GETREALM,
			     &realm_cb, &realm_context);

    if (ret == SASL_OK && realm_cb) {
	ret = realm_cb(realm_context, SASL_CB_GETREALM, availrealms, realm);
	if (ret != SASL_OK)
	    return ret;

	if (!*realm) {
	    PARAMERROR(utils);
	    return SASL_BADPARAM;
	}
    }
  
    return ret;
}

/*
 * Make the requested prompts. (prompt==NULL means we don't want it)
 */
int _plug_make_prompts(const sasl_utils_t *utils,
		       sasl_interact_t **prompts_res,
		       const char *user_prompt, const char *user_def,
		       const char *auth_prompt, const char *auth_def,
		       const char *pass_prompt, const char *pass_def,
		       const char *echo_chal,
		       const char *echo_prompt, const char *echo_def,
		       const char *realm_chal,
		       const char *realm_prompt, const char *realm_def)
{
    int num = 1;
    int alloc_size;
    sasl_interact_t *prompts;

    if (user_prompt) num++;
    if (auth_prompt) num++;
    if (pass_prompt) num++;
    if (echo_prompt) num++;
    if (realm_prompt) num++;

    if (num == 1) {
	SETERROR( utils, "make_prompts() called with no actual prompts" );
	return SASL_FAIL;
    }

    alloc_size = sizeof(sasl_interact_t)*num;
    prompts = utils->malloc(alloc_size);
    if (!prompts) {
	MEMERROR( utils );
	return SASL_NOMEM;
    }
    memset(prompts, 0, alloc_size);
  
    *prompts_res = prompts;

    if (user_prompt) {
	(prompts)->id = SASL_CB_USER;
	(prompts)->challenge = "Authorization Name";
	(prompts)->prompt = user_prompt;
	(prompts)->defresult = user_def;

	prompts++;
    }

    if (auth_prompt) {
	(prompts)->id = SASL_CB_AUTHNAME;
	(prompts)->challenge = "Authentication Name";
	(prompts)->prompt = auth_prompt;
	(prompts)->defresult = auth_def;

	prompts++;
    }

    if (pass_prompt) {
	(prompts)->id = SASL_CB_PASS;
	(prompts)->challenge = "Password";
	(prompts)->prompt = pass_prompt;
	(prompts)->defresult = pass_def;

	prompts++;
    }

    if (echo_prompt) {
	(prompts)->id = SASL_CB_ECHOPROMPT;
	(prompts)->challenge = echo_chal;
	(prompts)->prompt = echo_prompt;
	(prompts)->defresult = echo_def;

	prompts++;
    }

    if (realm_prompt) {
	(prompts)->id = SASL_CB_GETREALM;
	(prompts)->challenge = realm_chal;
	(prompts)->prompt = realm_prompt;
	(prompts)->defresult = realm_def;

	prompts++;
    }

    /* add the ending one */
    (prompts)->id = SASL_CB_LIST_END;
    (prompts)->challenge = NULL;
    (prompts)->prompt = NULL;
    (prompts)->defresult = NULL;

    return SASL_OK;
}

/*
 * Decode and concatenate multiple packets using the given function
 * to decode each packet.
 */
int _plug_decode(const sasl_utils_t *utils,
		 void *context,
		 const unsigned char *input, unsigned inputlen,
		 unsigned char **output,		/* output buffer */
		 unsigned int *outputsize,	/* current size of output buffer */
		 unsigned int *outputlen,	/* length of data in output buffer */
		 int (*decode_pkt)(void *context,
				   const unsigned char **input, unsigned int *inputlen,
				   unsigned char **output, unsigned int *outputlen))
{
    unsigned char *tmp = NULL;
    unsigned tmplen = 0;
    int ret;
    
    *outputlen = 0;

    while (inputlen!=0)
    {
	/* no need to free tmp */
      ret = decode_pkt(context, &input, &inputlen, &tmp, &tmplen);

      if(ret != SASL_OK) return ret;

      if (tmp!=NULL) /* if received 2 packets merge them together */
      {
	  ret = _plug_buf_alloc(utils, (unsigned char **)output, outputsize, *outputlen + tmplen + 1);
	  if(ret != SASL_OK) return ret;

	  memcpy(*output + *outputlen, tmp, tmplen);

	  /* Protect stupid clients */
	  *(*output + *outputlen + tmplen) = '\0';

	  *outputlen+=tmplen;
      }
    }

    return SASL_OK;    
}

/* returns the realm we should pretend to be in */
int _plug_parseuser(const sasl_utils_t *utils,
		    char **user, char **realm, const char *user_realm, 
		    const char *serverFQDN, const char *input)
{
    int ret;
    char *r;

    if(!user || !serverFQDN) {
	PARAMERROR( utils );
	return SASL_BADPARAM;
    }

    r = strchr(input, '@');
    if (!r) {
	/* hmmm, the user didn't specify a realm */
	if(user_realm && user_realm[0]) {
	    ret = _plug_strdup(utils, user_realm, realm, NULL);
	} else {
	    /* Default to serverFQDN */
	    ret = _plug_strdup(utils, serverFQDN, realm, NULL);
	}
	
	if (ret == SASL_OK) {
	    ret = _plug_strdup(utils, input, user, NULL);
	}
    } else {
	r++;
	ret = _plug_strdup(utils, r, realm, NULL);
	*--r = '\0';
	*user = utils->malloc(r - input + 1);
	if (*user) {
	    strncpy(*user, input, r - input +1);
	} else {
	    MEMERROR( utils );
	    ret = SASL_NOMEM;
	}
	*r = '@';
    }

    return ret;
}
