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
 * Portions (c) Copyright 2005 Apple Computer, Inc.
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
 * users.c -- functions for manipulating data for logged in users
 * 
 --------------------------------------------------------------------------*/

#include "jsm.h"

int js__usercount = 0;
/*
 *  _js_users_del -- call-back for deleting user from the hash table
 *  
 *  This function is called periodically by the user data garbage collection
 *  thread. It removes users aren't logged in from the global hashtable.
 *
 *  parameters
 *  	arg -- not used
 *		key -- the users key in the hashtable, not used
 *      data -- the user data to check
 *
 *  returns
 *      1  
 */
int _js_users_del(void *arg, const void *key, void *data)
{
    HASHTABLE ht = (HASHTABLE)arg;
    udata u = (udata)data;	/* cast the pointer into udata */

    /*
     * if the reference count for this user's record
     * is positive, or if there are active sessions
     * we can't free it, so return immediately
     */
    if(u->ref > 0 || (u->sessions != NULL && ++js__usercount))
        return 1;

    log_debug(ZONE,"freeing %s",u->user);

    ghash_remove(ht,u->user);
    pool_free(u->p);

    return 1;
}


/* callback for walking the host hash tree */
int _js_hosts_del(void *arg, const void *key, void *data)
{
    HASHTABLE ht = (HASHTABLE)data;

    log_debug(ZONE,"checking users for host %s",(char*)key);

    ghash_walk(ht,_js_users_del,ht);

    return 1;
}

/*
 *  js_users_gc is a heartbeat that
 *  flushes old users from memory.  
 */
result js_users_gc(void *arg)
{
    jsmi si = (jsmi)arg;

    /* free user struct if we can */
    js__usercount = 0;
    ghash_walk(si->hosts,_js_hosts_del,NULL);
    log_debug("usercount","%d\ttotal users",js__usercount);
    return r_DONE;
}



/*
 *  js_user -- gets the udata record for a user
 *  
 *  js_user attempts to locate the user data record
 *  for the specifed id. First it looks in current list,
 *  if that fails, it looks in xdb and creates new list entry.
 *  If THAT fails, it returns NULL (not a user).
 */
udata js_user(jsmi si, jid id, HASHTABLE ht)
{
    pool p;
    udata cur, newu;
    char *ustr;
    xmlnode x, y;
    jid uid;

    if(si == NULL || id == NULL || id->user == NULL) return NULL;

    /* get the host hash table if it wasn't provided */
    if(ht == NULL)
        ht = ghash_get(si->hosts,id->server);

    /* hrm, like, this isn't our user! */
    if(ht == NULL) return NULL;

    /* copy the id and convert user to lower case */
    uid = jid_new(id->p, jid_full(jid_user(id)));
    for(ustr = uid->user; *ustr != '\0'; ustr++)
        *ustr = tolower(*ustr);

    /* debug message */
    log_debug(ZONE,"js_user(%s,%X)",jid_full(uid),ht);

    /* try to get the user data from the hash table */
    if((cur = ghash_get(ht,uid->user)) != NULL)
        return cur;

    /* debug message */
    log_debug(ZONE,"## js_user not current ##");

    /* try to get the user auth data from xdb */
    x = xdb_get(si->xc, uid, NS_AUTH);
        
    /* try to get hashed user auth data from xdb */
    y = xdb_get(si->xc, uid, NS_AUTH_CRYPT);

    /* does the user exist? */
//apple    if (x == NULL && y == NULL)
//apple	    return NULL;

    /* create a udata struct */
    p = pool_heap(64);
    newu = pmalloco(p, sizeof(_udata));
    newu->p = p;
    newu->si = si;
    newu->user = pstrdup(p, uid->user);
    newu->pass = x ? pstrdup(p, xmlnode_get_data(x)) : NULL;
    newu->id = jid_new(p,jid_full(uid));
    if (x)
	xmlnode_free(x);
    if (y)
	xmlnode_free(y);


    /* got the user, add it to the user list */
    ghash_put(ht,newu->user,newu);
    log_debug(ZONE,"js_user debug %X %X",ghash_get(ht,newu->user),newu);

    return newu;
}

