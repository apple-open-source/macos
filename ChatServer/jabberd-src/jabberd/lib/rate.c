/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "JOSL").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the JOSL. You
 * may obtain a copy of the JOSL at http://www.jabber.org/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the JOSL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the JOSL
 * for the specific language governing rights and limitations under the
 * JOSL.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2002 Jabber.com, Inc.  All Rights Reserved.  Contact
 * information for Jabber.com, Inc. is available at http://www.jabber.com/.
 *
 * Portions Copyright (c) 1998-1999 Jeremie Miller.
 * 
 * Acknowledgements
 * 
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * 
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of those above.  If you
 * wish to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the JOSL,
 * indicate your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.  If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the JOSL or the GPL. 
 * 
 * 
 * --------------------------------------------------------------------------*/

#include "lib.h"

jlimit jlimit_new(int maxt, int maxp)
{
    pool p;
    jlimit r;

    p = pool_new();
    r = pmalloc(p,sizeof(_jlimit));
    r->key = NULL;
    r->start = r->points = 0;
    r->maxt = maxt;
    r->maxp = maxp;
    r->p = p;

    return r;
}

void jlimit_free(jlimit r)
{
    if(r != NULL)
    {
        if(r->key != NULL) free(r->key);
        pool_free(r->p);
    }
}

int jlimit_check(jlimit r, char *key, int points)
{
    int now = time(NULL);

    if(r == NULL) return 0;

    /* make sure we didn't go over the time frame or get a null/new key */
    if((now - r->start) > r->maxt || key == NULL || j_strcmp(key,r->key) != 0)
    { /* start a new key */
        free(r->key);
        if(key != NULL)
	  /* We use strdup instead of pstrdup since r->key needs to be free'd before 
	     and more often than the rest of the rlimit structure */
            r->key = strdup(key); 
        else
            r->key = NULL;
        r->start = now;
        r->points = 0;
    }

    r->points += points;

    /* if we're within the time frame and over the point limit */
    if(r->points > r->maxp && (now - r->start) < r->maxt)
    {
        return 1; /* we don't reset the rate here, so that it remains rated until the time runs out */
    }

    return 0;
}
