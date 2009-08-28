

/* Portions of this code are
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


/* Portions of this code are
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


/* Portions of this code are 
 * Copyright (c) 2005 RIPE NCC, Olaf Kolkman
 *
 *
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of the
 * author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS; IN
 * NO EVENT SHALL AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include "./netdns.h"
#include <sys/types.h>


static int		special(int);
static int		printable(int);
static const char	digits[] = "0123456789";



/*
 * Expand compressed domain name 'comp_dn' to full domain name.
 * 'msg' is a pointer to the begining of the message,
 * 'eomorig' points to the first location after the message,
 * 'exp_dn' is a pointer to a buffer of size 'length' for the result.
 * Return size of compressed name or -1 if there was an error.
 */

int netdns_dn_expand(msg, eomorig, comp_dn, exp_dn, length)
     u_char *msg, *eomorig, *comp_dn, *exp_dn;
     int length;
{
  register u_char *cp, *dn;
  register int n, c;
  u_char *eom;
  int len = -1, checked = 0;

  dn = exp_dn;
  cp = comp_dn;
  eom = exp_dn + length;


  /*
   * fetch next label in domain name
   */
  while ( (n = *cp++) ) {
    /*
     * Check for indirection
     */
    switch (n & INDIR_MASK) {
    case 0:
      if (dn != exp_dn) {
	if (dn >= eom)
	  return (-1);
	*dn++ = '.';
      }
      if (dn+n >= eom)
	return (-1);
      checked += n + 1;

      while (--n >= 0) {
	c = *cp++;
	if (special(c)) {
	  if (dn + 1 >= eom) {
	    return (-1);
	  }
	  *dn++ = '\\';
	  *dn++ = (char)c;
	}else if (!printable(c)) {
	  if (dn + 3 >= eom) {
	    return (-1);
	  }
	  *dn++ = '\\';
	  *dn++ = digits[c / 100];
	  *dn++ = digits[(c % 100) / 10];
	  *dn++ = digits[c % 10];

	} else {
	  if (dn >= eom) {
	    return (-1);
	  }
	  *dn++ = (char)c;
	}
	
	if (cp >= eomorig)/* out of range */
	  return(-1);
      }
      break;

    case INDIR_MASK:
      if (len < 0)
	len = cp - comp_dn + 1;
      cp = msg + (((n & 0x3f) << 8) | (*cp & 0xff));
      if (cp < msg || cp >= eomorig)/* out of range */
	return(-1);
      checked += 2;
      /*
       * Check for loops in the compressed name;
       * if we've looked at the whole message,
       * there must be a loop.
       */
      if (checked >= eomorig - msg)
	return (-1);
      break;

    default:
      return (-1);/* flag error */
    }
  }
  *dn = '\0';



  if (len < 0)
    len = cp - comp_dn;
  return (len);
}





/*
 * special(ch)
 *	Thinking in noninternationalized USASCII (per the DNS spec),
 *	is this characted special ("in need of quoting") ?
 * return:
 *	boolean.
 */
static int
special(int ch) {
	switch (ch) {
	case 0x22: /* '"' */
	case 0x2E: /* '.' */
	case 0x3B: /* ';' */
	case 0x5C: /* '\\' */
	case 0x28: /* '(' */
	case 0x29: /* ')' */
	/* Special modifiers in zone files. */
	case 0x40: /* '@' */
	case 0x24: /* '$' */
		return (1);
	default:
		return (0);
	}
}




/*
 * printable(ch)
 *	Thinking in noninternationalized USASCII (per the DNS spec),
 *	is this character visible and not a space when printed ?
 * return:
 *	boolean.
 */
static int
printable(int ch) {
	return (ch > 0x20 && ch < 0x7f);
}






