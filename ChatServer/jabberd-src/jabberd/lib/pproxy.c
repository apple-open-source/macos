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

/* these aren't the most efficient things in the world, a hash optimized for tiny spaces would be far better */

ppdb _ppdb_new(pool p, jid id)
{
    ppdb ret;
    ret = pmalloc(p,sizeof(_ppdb));
    ret->p = p;
    ret->pri = -1;
    ret->next = NULL;
    ret->user = NULL;
    ret->x = NULL;
    ret->id = jid_new(p,jid_full(id));

    return ret;
}

ppdb _ppdb_get(ppdb db, jid id)
{
    ppdb cur;

    if(db == NULL || id == NULL) return NULL;

    for(cur = db->next; cur != NULL; cur = cur->next)
        if(jid_cmp(cur->id,id) == 0) return cur;

    return NULL;
}

ppdb ppdb_insert(ppdb db, jid id, xmlnode x)
{
    ppdb cur, curu;
    pool p;

    if(id == NULL || id->server == NULL || x == NULL)
        return db;

    /* new ppdb list dummy holder */
    if(db == NULL)
    {
        p = pool_heap(1024);
        db = _ppdb_new(p,NULL);
    }

    cur = _ppdb_get(db,id);

    /* just update it */
    if(cur != NULL)
    {
        xmlnode_free(cur->x);
        cur->x = xmlnode_dup(x);
        cur->pri = jutil_priority(x);
        return db;
    }

    /* make an entry for it */
    cur = _ppdb_new(db->p,id);
    cur->x = xmlnode_dup(x);
    cur->pri = jutil_priority(x);
    cur->next = db->next;
    db->next = cur;

    /* if this is a user's resource presence, get the the user entry */
    if(id->user != NULL && (curu = _ppdb_get(db,jid_user(id))) != cur)
    {
        /* no user entry, make one */
        if(curu == NULL)
        {
            curu = _ppdb_new(db->p,jid_user(id));
            curu->next = db->next;
            db->next = curu;
        }

        /* insert this resource into the user list */
        cur->user = curu->user;
        curu->user = cur;
    }

    return db;
}

xmlnode ppdb_primary(ppdb db, jid id)
{
    ppdb cur, top;

    if(db == NULL || id == NULL) return NULL;

    cur = _ppdb_get(db,id);

    if(cur == NULL) return NULL;

    /* not user@host check, just return */
    if(id->user == NULL || id->resource != NULL) return cur->x;

    top = cur;
    for(cur = cur->user; cur != NULL; cur = cur->user)
        if(cur->pri >= top->pri) top = cur;

    if(top != NULL && top->pri >= 0) return top->x;

    return NULL;
}

/* return the presence for the id, successive calls return all of the known resources for a user@host address */
xmlnode ppdb_get(ppdb db, jid id)
{
    static ppdb last = NULL;
    ppdb cur;

    if(db == NULL || id == NULL) return NULL;

    /* MODE: if this is NOT just user@host addy, return just the single entry */
    if(id->user == NULL || id->resource != NULL)
    {
        /* we were just here, return now */
        if(last != NULL)
        {
            last = NULL;
            return NULL;
        }

        last = _ppdb_get(db,id);
        if(last != NULL)
            return last->x;
        else
            return NULL;
    }

    /* handle looping for user@host */

    /* we're already in the loop */
    if(last != NULL)
    {
        /* this is the last entry in the list */
        if(last->user == NULL)
        {
            last = NULL;
            return NULL;
        }

        last = last->user;
        return last->x;
    }

    /* start a new loop */
    cur = _ppdb_get(db,id);

    if(cur == NULL) return NULL;

    last = cur->user;
    if(last != NULL)
        return last->x;
    else
        return NULL;
}


void ppdb_free(ppdb db)
{
    ppdb cur;

    if(db == NULL) return;

    for(cur = db; cur != NULL; cur = cur->next)
        xmlnode_free(cur->x);

    pool_free(db->p);
}

