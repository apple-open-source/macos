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

/* THIS MODULE will soon be depreciated by mod_filter */

/* mod_offline must go before mod_presence */

/* handle an offline message */
mreturn mod_offline_message(mapi m)
{
    session top;
    xmlnode cur = NULL, cur2;
    char str[10];

    /* if there's an existing session, just give it to them */
    if((top = js_session_primary(m->user)) != NULL)
    {
        js_session_to(top,m->packet);
        return M_HANDLED;
    }

   /* look for event messages */
    for(cur = xmlnode_get_firstchild(m->packet->x); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        if(NSCHECK(cur,NS_EVENT))
        {
            if(xmlnode_get_tag(cur,"id") != NULL)
                return M_PASS; /* bah, we don't want to store events offline (XXX: do we?) */
            if(xmlnode_get_tag(cur,"offline") != NULL)
                break; /* cur remaining set is the flag */
        }

    log_debug("mod_offline","handling message for %s",m->user->user);

    if((cur2 = xmlnode_get_tag(m->packet->x,"x?xmlns=" NS_EXPIRE)) != NULL)
    {
        if(j_atoi(xmlnode_get_attrib(cur2, "seconds"),0) == 0)
            return M_PASS; 
        
        sprintf(str,"%d",(int)time(NULL));
        xmlnode_put_attrib(cur2,"stored",str);
    }
    jutil_delay(m->packet->x,"Offline Storage");

    if(xdb_act(m->si->xc, m->user->id, NS_OFFLINE, "insert", NULL, m->packet->x)) /* feed the message itself, and do an xdb insert */
        return M_PASS;

    if(cur != NULL)
    { /* if there was an offline event to be sent, send it for gosh sakes! */

        jutil_tofrom(m->packet->x);

        /* erease everything else in the message */
        for(cur2 = xmlnode_get_firstchild(m->packet->x); cur2 != NULL; cur2 = xmlnode_get_nextsibling(cur2))
            if(cur2 != cur)
                xmlnode_hide(cur2);

        /* erase any other events */
        for(cur2 = xmlnode_get_firstchild(cur); cur2 != NULL; cur2 = xmlnode_get_nextsibling(cur2))
            xmlnode_hide(cur2);

        /* fill it in and send it on */
        xmlnode_insert_tag(cur,"offline");
        xmlnode_insert_cdata(xmlnode_insert_tag(cur,"id"),xmlnode_get_attrib(m->packet->x,"id"), -1);
        js_deliver(m->si, jpacket_reset(m->packet));

    }else{
        xmlnode_free(m->packet->x);
    }
    return M_HANDLED;

}

/* just breaks out to our message/presence offline handlers */
mreturn mod_offline_handler(mapi m, void *arg)
{
    if(m->packet->type == JPACKET_MESSAGE) return mod_offline_message(m);

    return M_IGNORE;
}

/* watches for when the user is available and sends out offline messages */
void mod_offline_out_available(mapi m)
{
    xmlnode opts, cur, x;
    int now = time(NULL);
    int expire, stored, diff;
    char str[10];

    log_debug("mod_offline","avability established, check for messages");

    if((opts = xdb_get(m->si->xc, m->user->id, NS_OFFLINE)) == NULL)
        return;

    /* check for msgs */
    for(cur = xmlnode_get_firstchild(opts); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        /* check for expired stuff */
        if((x = xmlnode_get_tag(cur,"x?xmlns=" NS_EXPIRE)) != NULL)
        {
            expire = j_atoi(xmlnode_get_attrib(x,"seconds"),0);
            stored = j_atoi(xmlnode_get_attrib(x,"stored"),now);
            diff = now - stored;
            if(diff >= expire)
            {
                log_debug(ZONE,"dropping expired message %s",xmlnode2str(cur));
                xmlnode_hide(cur);
                continue;
            }
            sprintf(str,"%d",expire - diff);
            xmlnode_put_attrib(x,"seconds",str);
            xmlnode_hide_attrib(x,"stored");
        }
        js_session_to(m->s,jpacket_new(xmlnode_dup(cur)));
        xmlnode_hide(cur);
    }
    /* messages are gone, save the new sun-dried opts container */
    xdb_set(m->si->xc, m->user->id, NS_OFFLINE, opts); /* can't do anything if this fails anyway :) */
    xmlnode_free(opts);
}

mreturn mod_offline_out(mapi m, void *arg)
{
    if(m->packet->type != JPACKET_PRESENCE) return M_IGNORE;

    if(js_online(m))
        mod_offline_out_available(m);

    return M_PASS;
}

/* sets up the per-session listeners */
mreturn mod_offline_session(mapi m, void *arg)
{
    log_debug(ZONE,"session init");

    js_mapi_session(es_OUT, m->s, mod_offline_out, NULL);

    return M_PASS;
}

void mod_offline(jsmi si)
{
    log_debug("mod_offline","init");
    js_mapi_register(si,e_OFFLINE, mod_offline_handler, NULL);
    js_mapi_register(si,e_SESSION, mod_offline_session, NULL);
}

