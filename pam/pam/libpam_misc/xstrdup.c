/* $Id: xstrdup.c,v 1.5 2002/03/27 06:18:21 bbraun Exp $ */

#include <malloc.h>
#include <string.h>
#include <pam/pam_misc.h>

/*
 * Safe duplication of character strings. "Paranoid"; don't leave
 * evidence of old token around for later stack analysis.
 */

char *xstrdup(const char *x)
{
     register char *new=NULL;

     if (x != NULL) {
	  register int i;

	  for (i=0; x[i]; ++i);                       /* length of string */
	  if ((new = malloc(++i)) == NULL) {
	       i = 0;
	  } else {
	       while (i-- > 0) {
		    new[i] = x[i];
	       }
	  }
	  x = NULL;
     }

     return new;                 /* return the duplicate or NULL on error */
}
