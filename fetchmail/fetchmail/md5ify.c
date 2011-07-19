/*
 * For license terms, see the file COPYING in this directory.
 */

/***********************************************************************
  module:       md5ify.c
  project:      fetchmail
  programmer:   Carl Harris, ceharris@mal.com
  description:  Simple interface to MD5 module.

 ***********************************************************************/

#include <stdio.h>
#include <string.h>

#if defined(STDC_HEADERS)
#include <string.h>
#endif

#include "fm_md5.h"

char *
MD5Digest (unsigned const char *s)
{
  int i;
  MD5_CTX context;
  unsigned char digest[16];
  static char ascii_digest [33];

  MD5Init(&context);
  MD5Update(&context, s, strlen((const char *)s));
  MD5Final(digest, &context);

  for (i = 0;  i < 16;  i++) 
    sprintf(ascii_digest+2*i, "%02x", digest[i]);
 
  return(ascii_digest);
}
