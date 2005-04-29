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
#include "jsm.h"

int _mod_admin_browse(void *arg, const void *key, void *data)
{
    xmlnode browse = (xmlnode)arg;
    udata u = (udata)data;
    xmlnode x;
    session s = js_session_primary(u);
    spool sp;
    int t = time(NULL);
    char buff[10];

    /* make a user generic entry */
    x = xmlnode_insert_tag(browse,"user");
    xmlnode_put_attrib(x,"jid",jid_full(u->id));
    if(s == NULL)
    {
        xmlnode_put_attrib(x,"name",u->user);
        return 1;
    }
    sp = spool_new(xmlnode_pool(browse));
    spooler(sp,u->user," (",sp);

    /* insert extended data for the primary session */
    sprintf(buff,"%d", (int)(t - s->started));
    spooler(sp,buff,", ",sp);
    sprintf(buff,"%d", s->c_out);
    spooler(sp,buff,", ",sp);
    sprintf(buff,"%d", s->c_in);
    spooler(sp,buff,")",sp);

    xmlnode_put_attrib(x,"name",spool_print(sp));

    return 1;
}

/* who */
void mod_admin_browse(jsmi si, jpacket p)
{
    xmlnode browse;

    jutil_iqresult(p->x);
    browse = xmlnode_insert_tag(p->x,"item");
    xmlnode_put_attrib(browse,"jid",spools(xmlnode_pool(browse),p->to->server,"/admin",xmlnode_pool(browse)));
    xmlnode_put_attrib(browse,"name","Online Users (seconds, sent, received)");
    xmlnode_put_attrib(browse,"xmlns",NS_BROWSE);

    if(jpacket_subtype(p) == JPACKET__GET)
    {
        log_debug("mod_admin","handling who GET");

        /* walk the users on this host */
        ghash_walk(ghash_get(si->hosts, p->to->server),_mod_admin_browse,(void *)browse);
    }

    if(jpacket_subtype(p) == JPACKET__SET)
    {
        log_debug("mod_admin","handling who SET");

        /* kick them? */
    }

    jpacket_reset(p);
    js_deliver(si,p);
}

int _mod_admin_who(void *arg, const void *key, void *data)
{
    xmlnode who = (xmlnode)arg;
    udata u = (udata)data;
    session s;
    xmlnode x;
    time_t t;
    char buff[10];

    t = time(NULL);

    /* loop through all the sessions */
    for(s = u->sessions; s != NULL; s = s->next)
    {
        /* make a presence entry for each one with a custom extension */
        x = xmlnode_insert_tag_node(who,s->presence);
        x = xmlnode_insert_tag(x,"x");
        xmlnode_put_attrib(x,"xmlns","jabber:mod_admin:who");

        /* insert extended data */
        sprintf(buff,"%d", (int)(t - s->started));
        xmlnode_put_attrib(x,"timer",buff);
        sprintf(buff,"%d", s->c_in);
        xmlnode_put_attrib(x,"from",buff);
        sprintf(buff,"%d", s->c_out);
        xmlnode_put_attrib(x,"to",buff);
    }

    return 1;
}

/* who */
mreturn  mod_admin_who(jsmi si, jpacket p)
{
    xmlnode who = xmlnode_get_tag(p->iq,"who");

    if(jpacket_subtype(p) == JPACKET__GET)
    {
        log_debug("mod_admin","handling who GET");

        /* walk the users on this host */
        ghash_walk(ghash_get(si->hosts, p->to->server),_mod_admin_who,(void *)who);
    }

    if(jpacket_subtype(p) == JPACKET__SET)
    {
        log_debug("mod_admin","handling who SET");

        /* kick them? */
    }

    jutil_tofrom(p->x);
    xmlnode_put_attrib(p->x,"type","result");
    jpacket_reset(p);
    js_deliver(si,p);
    return M_HANDLED;
}

/* config */
mreturn mod_admin_config(jsmi si, jpacket p)
{
    xmlnode config = xmlnode_get_tag(p->iq,"config");
    xmlnode cur;

    if(jpacket_subtype(p) == JPACKET__GET)
    {
        log_debug("mod_admin","handling config GET");

        /* insert the loaded config file */
        xmlnode_insert_node(config,xmlnode_get_firstchild(si->config));
    }

    if(jpacket_subtype(p) == JPACKET__SET)
    {
        log_debug("mod_admin","handling config SET");

        /* XXX FIX ME, like do init stuff for the new config, etc */
        si->config = xmlnode_dup(config);


        /* empty the iq result */
        for(cur = xmlnode_get_firstchild(p->x); cur != NULL; cur = xmlnode_get_nextsibling(cur))
            xmlnode_hide(cur);
    }

    jutil_tofrom(p->x);
    xmlnode_put_attrib(p->x,"type","result");
    jpacket_reset(p);
    js_deliver(si,p);
    return M_HANDLED;
}

/* user */
mreturn mod_admin_user(jsmi si, jpacket p)
{
    if(jpacket_subtype(p) == JPACKET__GET)
    {
        log_debug("mod_admin","handling user GET");
    }

    if(jpacket_subtype(p) == JPACKET__SET)
    {
        log_debug("mod_admin","handling user SET");
    }

    jutil_tofrom(p->x);
    xmlnode_put_attrib(p->x,"type","result");
    jpacket_reset(p);
    js_deliver(si,p);
    return M_HANDLED;
}

/* monitor */
mreturn mod_admin_monitor(jsmi si, jpacket p)
{
    if(jpacket_subtype(p) == JPACKET__GET)
    {
        log_debug("mod_admin","handling monitor GET");
    }

    if(jpacket_subtype(p) == JPACKET__SET)
    {
        log_debug("mod_admin","handling monitor SET");
    }

    jutil_tofrom(p->x);
    xmlnode_put_attrib(p->x,"type","result");
    jpacket_reset(p);
    js_deliver(si,p);
    return M_HANDLED;
}

/* dispatch */
mreturn mod_admin_dispatch(mapi m, void *arg)
{
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(jpacket_subtype(m->packet) == JPACKET__ERROR) return M_PASS;

    /* first check the /admin browse feature */
    if(NSCHECK(m->packet->iq,NS_BROWSE) && j_strcmp(m->packet->to->resource,"admin") == 0)
    {
        if(js_admin(m->user,ADMIN_READ))
            mod_admin_browse(m->si, m->packet);
        else
            js_bounce(m->si,m->packet->x,TERROR_NOTALLOWED);
        return M_HANDLED;
    }

    /* now normal iq:admin stuff */
    if(!NSCHECK(m->packet->iq,NS_ADMIN)) return M_PASS;

    log_debug("mod_admin","checking admin request from %s",jid_full(m->packet->from));

    if(js_admin(m->user,ADMIN_READ))
    {
        if(xmlnode_get_tag(m->packet->iq,"who") != NULL) return mod_admin_who(m->si, m->packet);
        if(0 && xmlnode_get_tag(m->packet->iq,"monitor") != NULL) return mod_admin_monitor(m->si, m->packet);
    }

    if(js_admin(m->user,ADMIN_WRITE))
    {
        if(0 && xmlnode_get_tag(m->packet->iq,"user") != NULL) return mod_admin_user(m->si, m->packet);
        if(xmlnode_get_tag(m->packet->iq,"config") != NULL) return mod_admin_config(m->si, m->packet);
    }

    js_bounce(m->si,m->packet->x,TERROR_NOTALLOWED);
    return M_HANDLED;
}


/* message */
mreturn mod_admin_message(mapi m, void *arg)
{
    jpacket p;
    xmlnode cur;
    char *subject;
    static char jidlist[1024] = "";

    if(m->packet->type != JPACKET_MESSAGE) return M_IGNORE;
    if(m->packet->to->resource != NULL || js_config(m->si,"admin") == NULL || jpacket_subtype(m->packet) == JPACKET__ERROR) return M_PASS;

    /* drop ones w/ a delay! (circular safety) */
    if(xmlnode_get_tag(m->packet->x,"x?xmlns=" NS_DELAY) != NULL)
    {
        xmlnode_free(m->packet->x);
        return M_HANDLED;
    }

    log_debug("mod_admin","delivering admin message from %s",jid_full(m->packet->from));

    subject=spools(m->packet->p,"Admin: ",xmlnode_get_tag_data(m->packet->x,"subject")," (",m->packet->to->server,")",m->packet->p);
    xmlnode_hide(xmlnode_get_tag(m->packet->x,"subject"));
    xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->x,"subject"),subject,-1);
    jutil_delay(m->packet->x,"admin");

    for(cur = xmlnode_get_firstchild(js_config(m->si,"admin")); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if(xmlnode_get_name(cur) == NULL || xmlnode_get_data(cur) == NULL) continue;

        p = jpacket_new(xmlnode_dup(m->packet->x));
        p->to = jid_new(p->p,xmlnode_get_data(cur));
        xmlnode_put_attrib(p->x,"to",jid_full(p->to));
        js_deliver(m->si,p);
    }

    /* reply, but only if we haven't in the last few or so jids */
    if((cur = js_config(m->si,"admin/reply")) != NULL && strstr(jidlist,jid_full(jid_user(m->packet->from))) == NULL)
    {
        /* tack the jid onto the front of the list, depreciating old ones off the end */
        char njidlist[1024];
        snprintf(njidlist,1024,"%s %s",jid_full(jid_user(m->packet->from)),jidlist);
        memcpy(jidlist,njidlist,1024);

        if(xmlnode_get_tag(cur,"subject") != NULL)
        {
            xmlnode_hide(xmlnode_get_tag(m->packet->x,"subject"));
            xmlnode_insert_tag_node(m->packet->x,xmlnode_get_tag(cur,"subject"));
        }
        xmlnode_hide(xmlnode_get_tag(m->packet->x,"body"));
        xmlnode_insert_tag_node(m->packet->x,xmlnode_get_tag(cur,"body"));
        jutil_tofrom(m->packet->x);
        jpacket_reset(m->packet);
        js_deliver(m->si,m->packet);
    }else{
        xmlnode_free(m->packet->x);
    }
    return M_HANDLED;
}

void mod_admin(jsmi si)
{
    js_mapi_register(si,e_SERVER,mod_admin_dispatch,NULL);
    js_mapi_register(si,e_SERVER,mod_admin_message,NULL);
}


