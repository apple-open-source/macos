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
 * main.c - entry point for jsm.so
 * --------------------------------------------------------------------------*/
#include "jsm.h"

/*

packet handler
    check for new session auth/reg request, handle seperately
    check master jid hash table for session
    pass to offline thread
    track server names for master "i am" table

*/

typedef void (*modcall)(jsmi si);

result jsm_stat(void *arg)
{
    pool_stat(0);
    return r_DONE;
}

int __jsm_shutdown(void *arg, const void *key, void *data)
{
    udata u = (udata)data;	/* cast the pointer into udata */
    session cur;

    for(cur = u->sessions; cur != NULL; cur = cur->next)
    {
        js_session_end(cur, "JSM shutdown");
    }
    return 1;
}


/* callback for walking the host hash tree */
int _jsm_shutdown(void *arg, const void *key, void *data)
{
    HASHTABLE ht = (HASHTABLE)data;

    log_debug(ZONE,"JSM SHUTDOWN: deleting users for host %s",(char*)key);

    ghash_walk(ht,__jsm_shutdown,NULL);

    return 1;
}

void jsm_shutdown(void *arg)
{
    jsmi si = (jsmi)arg;

    log_debug(ZONE, "JSM SHUTDOWN: Begining shutdown sequence");
    ghash_walk(si->hosts,_jsm_shutdown,arg);
    ghash_destroy(si->hosts);
    xmlnode_free(si->config);
}

void jsm(instance i, xmlnode x)
{
    jsmi si;
    xmlnode cur;
    modcall module;
    int n;

    log_debug(ZONE,"jsm initializing for section '%s'",i->id);

    /* create and init the jsm instance handle */
    si = pmalloco(i->p, sizeof(_jsmi));
    si->i = i;
    si->p = i->p;
    si->xc = xdb_cache(i); /* getting xdb_* handle and fetching config */
    si->config = xdb_get(si->xc, jid_new(xmlnode_pool(x),"config@-internal"),"jabber:config:jsm");
    si->hosts = ghash_create(j_atoi(xmlnode_get_tag_data(si->config,"maxhosts"),HOSTS_PRIME),(KEYHASHFUNC)str_hash_code,(KEYCOMPAREFUNC)j_strcmp);
    for(n=0;n<e_LAST;n++)
        si->events[n] = NULL;

    /* initialize globally trusted ids */
    for(cur = xmlnode_get_firstchild(xmlnode_get_tag(si->config,"admin")); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if(xmlnode_get_type(cur) != NTYPE_TAG) continue;

        if(si->gtrust == NULL)
            si->gtrust = jid_new(si->p,xmlnode_get_data(cur));
        else
            jid_append(si->gtrust,jid_new(si->p,xmlnode_get_data(cur)));
    }

    /* fire up the modules by scanning the attribs on the xml we received */
    for(cur = xmlnode_get_firstattrib(x); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        /* avoid multiple personality complex */
        if(j_strcmp(xmlnode_get_name(cur),"jsm") == 0)
            continue;

        /* vattrib is stored as firstchild on an attrib node */
        if((module = (modcall)xmlnode_get_firstchild(cur)) == NULL)
            continue;

        /* call this module for this session instance */
        log_debug(ZONE,"jsm: loading module %s",xmlnode_get_name(cur));
        (module)(si);
    }

    pool_cleanup(i->p, jsm_shutdown, (void*)si);
    register_phandler(i, o_DELIVER, js_packet, (void *)si);
    register_beat(5,jsm_stat,NULL);
    register_beat(j_atoi(xmlnode_get_tag_data(si->config,"usergc"),60),js_users_gc,(void *)si);
}
