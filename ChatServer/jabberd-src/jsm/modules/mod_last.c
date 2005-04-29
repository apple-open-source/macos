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

mreturn mod_last_server(mapi m, void *arg)
{
    time_t start = time(NULL) - *(time_t*)arg;
    char str[10];
    xmlnode last;

    /* pre-requisites */
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(jpacket_subtype(m->packet) != JPACKET__GET || !NSCHECK(m->packet->iq,NS_LAST) || m->packet->to->resource != NULL) return M_PASS;

    jutil_iqresult(m->packet->x);
    jpacket_reset(m->packet);

    last = xmlnode_insert_tag(m->packet->x,"query");
    xmlnode_put_attrib(last,"xmlns",NS_LAST);
    sprintf(str,"%d",(int)start);
    xmlnode_put_attrib(last,"seconds",str);

    js_deliver(m->si,m->packet);

    return M_HANDLED;
}

void mod_last_set(mapi m, jid to, char *reason)
{
    xmlnode last;
    char str[10];

    log_debug("mod_last","storing last for user %s",jid_full(to));

    /* make a generic last chunk and store it */
    last = xmlnode_new_tag("query");
    xmlnode_put_attrib(last,"xmlns",NS_LAST);
    sprintf(str,"%d",(int)time(NULL));
    xmlnode_put_attrib(last,"last",str);
    xmlnode_insert_cdata(last,reason,-1);
    xdb_set(m->si->xc, jid_user(to), NS_LAST, last);
    xmlnode_free(last);
}

mreturn mod_last_init(mapi m, void *arg)
{
    if(jpacket_subtype(m->packet) != JPACKET__SET) return M_PASS;

    mod_last_set(m, m->packet->to, "Registered");

    return M_PASS;
}

mreturn mod_last_sess_end(mapi m, void *arg)
{
    if(m->s->presence != NULL) /* presence is only set if there was presence sent, and we only track logins that were available */
        mod_last_set(m, m->user->id, xmlnode_get_tag_data(m->s->presence,"status"));

    return M_PASS;
}

mreturn mod_last_sess(mapi m, void *arg)
{
    js_mapi_session(es_END, m->s, mod_last_sess_end, NULL);

    return M_PASS;
}

mreturn mod_last_reply(mapi m, void *arg)
{
    xmlnode last;
    int lastt;
    char str[10];

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(!NSCHECK(m->packet->iq,NS_LAST)) return M_PASS;

    /* first, is this a valid request? */
    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__RESULT:
    case JPACKET__ERROR:
        return M_PASS;
    case JPACKET__SET:
        js_bounce(m->si,m->packet->x,TERROR_NOTALLOWED);
        return M_HANDLED;
    }

    /* make sure they're in the roster */
    if(!js_trust(m->user,m->packet->from))
    {
        js_bounce(m->si,m->packet->x,TERROR_FORBIDDEN);
        return M_HANDLED;
    }

    log_debug("mod_last","handling query for user %s",m->user->user);

    last = xdb_get(m->si->xc, m->user->id, NS_LAST);

    jutil_iqresult(m->packet->x);
    jpacket_reset(m->packet);
    lastt = j_atoi(xmlnode_get_attrib(last,"last"),0);
    if(lastt > 0)
    {
        xmlnode_hide_attrib(last,"last");
        lastt = time(NULL) - lastt;
        sprintf(str,"%d",lastt);
        xmlnode_put_attrib(last,"seconds",str);
        xmlnode_insert_tag_node(m->packet->x,last);
    }
    js_deliver(m->si,m->packet);

    xmlnode_free(last);
    return M_HANDLED;
}


void mod_last(jsmi si)
{
    time_t *ttmp;
    log_debug("mod_last","initing");

    if (js_config(si,"register") != NULL) js_mapi_register(si, e_REGISTER, mod_last_init, NULL);
    js_mapi_register(si, e_SESSION, mod_last_sess, NULL);
    js_mapi_register(si, e_OFFLINE, mod_last_reply, NULL);

    /* set up the server responce, giving the startup time :) */
    ttmp = pmalloc(si->p, sizeof(time_t));
    time(ttmp);
    js_mapi_register(si, e_SERVER, mod_last_server, (void *)ttmp);
}
