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

xmlnode mod_browse_get(mapi m, jid id)
{
    xmlnode browse, x;

    if(id == NULL) /* use the user id as a backup */
        id = m->user->id;

    /* get main account browse */
    if((browse = xdb_get(m->si->xc, id, NS_BROWSE)) == NULL)
    { /* no browse is set up yet, we must create one for this user! */
        if(id->resource == NULL)
        { /* a user is only the user@host */
            browse = xmlnode_new_tag("user");
            /* get the friendly name for this user from somewhere */
            if((x = xdb_get(m->si->xc, m->user->id, NS_VCARD)) != NULL)
                xmlnode_put_attrib(browse,"name",xmlnode_get_tag_data(x,"FN"));
            else if((x = xdb_get(m->si->xc, m->user->id, NS_REGISTER)) != NULL)
                xmlnode_put_attrib(browse,"name",xmlnode_get_tag_data(x,"name"));
            xmlnode_free(x);
        }else{ /* everything else is generic unless set by the user */
            browse = xmlnode_new_tag("item");
        }

        xmlnode_put_attrib(browse,"xmlns",NS_BROWSE);
        xmlnode_put_attrib(browse,"jid",jid_full(id));

        xdb_set(m->si->xc, id, NS_BROWSE, browse);
    }

    return browse;
}

mreturn mod_browse_set(mapi m, void *arg)
{
    xmlnode browse, cur;
    jid id, to;

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(!NSCHECK(m->packet->iq,NS_BROWSE) || jpacket_subtype(m->packet) != JPACKET__SET) return M_PASS;
    if(m->packet->to != NULL) return M_PASS; /* if its to someone other than ourselves */

    log_debug("mod_browse","handling set request %s",xmlnode2str(m->packet->iq));

    /* no to implies to ourselves */
    if(m->packet->to != NULL)
        to = m->packet->to;
    else
        to = m->user->id;

    /* if we set to a resource, we need to make sure that resource's browse is in the users browse */
    if(to->resource != NULL)
    {
        browse = mod_browse_get(m, to); /* get our browse info */
        xmlnode_hide_attrib(browse,"xmlns"); /* don't need a ns as a child */
        for(cur = xmlnode_get_firstchild(browse); cur != NULL; cur = xmlnode_get_nextsibling(cur))
            xmlnode_hide(cur); /* erase all children */
        xdb_act(m->si->xc, m->user->id, NS_BROWSE, "insert", spools(m->packet->p,"?jid=",jid_full(to),m->packet->p), browse); /* insert and match replace */
        xmlnode_free(browse);
    }

    /* get the id of the new browse item */
    if((cur = xmlnode_get_firstchild(m->packet->iq)) == NULL || (id = jid_new(m->packet->p, xmlnode_get_attrib(cur,"jid"))) == NULL)
    {
        js_bounce(m->si,m->packet->x,TERROR_NOTACCEPTABLE);
        return M_HANDLED;
    }

    /* insert the new item into the resource it was sent to */
    xmlnode_hide_attrib(cur,"xmlns"); /* just in case, to make sure it inserts */
    if(xdb_act(m->si->xc, to, NS_BROWSE, "insert", spools(m->packet->p,"?jid=",jid_full(id),m->packet->p), cur))
    {
        js_bounce(m->si,m->packet->x,TERROR_UNAVAIL);
        return M_HANDLED;
    }
        
    /* if the new data we're inserting is to one of our resources, update that resource's browse */
    if(jid_cmpx(m->user->id, id, JID_USER|JID_SERVER) == 0 && id->resource != NULL)
    {
        /* get the old */
        browse = mod_browse_get(m, id);
        /* transform the new one into the old one */
        xmlnode_put_attrib(cur, "xmlns", NS_BROWSE);
        xmlnode_insert_node(cur, xmlnode_get_firstchild(browse));
        xdb_set(m->si->xc, id, NS_BROWSE, cur); /* replace the resource's browse w/ this one */
        xmlnode_free(browse);
    }

    /* send response to the user */
    jutil_iqresult(m->packet->x);
    jpacket_reset(m->packet);
    js_session_to(m->s,m->packet);

    return M_HANDLED;
}

mreturn mod_browse_session(mapi m, void *arg)
{
    js_mapi_session(es_OUT,m->s,mod_browse_set,NULL);
    return M_PASS;
}

mreturn mod_browse_reply(mapi m, void *arg)
{
    xmlnode browse, ns, cur;
    session s;

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(!NSCHECK(m->packet->iq,NS_BROWSE)) return M_PASS;

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

    log_debug("mod_browse","handling query for user %s",m->user->user);

    /* get this dudes browse info */
    browse = mod_browse_get(m, m->packet->to);

    /* insert the namespaces */
    ns = xdb_get(m->si->xc, m->packet->to, NS_XDBNSLIST);
    for(cur = xmlnode_get_firstchild(ns); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        if(xmlnode_get_attrib(cur,"type") == NULL)
            xmlnode_insert_tag_node(browse,cur); /* only include the generic <ns>foo</ns> */
    xmlnode_free(ns);

    /* include any connected resources if there's a s10n from them */
    if(js_trust(m->user, m->packet->from))
        for(s = m->user->sessions; s != NULL; s = s->next)
        {
            /* if(s->priority < 0) continue; *** include all resources I guess */
            if(xmlnode_get_tag(browse,spools(m->packet->p,"?jid=",jid_full(s->id),m->packet->p)) != NULL) continue; /* already in the browse result */
            cur = xmlnode_insert_tag(browse,"user");
            xmlnode_put_attrib(cur,"type", "client");
            xmlnode_put_attrib(cur,"jid", jid_full(s->id));
        }

    /* XXX include iq:filter forwards */

    jutil_iqresult(m->packet->x);
    jpacket_reset(m->packet);
    xmlnode_insert_tag_node(m->packet->x,browse);
    js_deliver(m->si,m->packet);

    xmlnode_free(browse);
    return M_HANDLED;
}

mreturn mod_browse_server(mapi m, void *arg)
{
    xmlnode browse, query, x;

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(jpacket_subtype(m->packet) != JPACKET__GET || !NSCHECK(m->packet->iq,NS_BROWSE) || m->packet->to->resource != NULL) return M_PASS;

    /* get data from the config file */
    if((browse = js_config(m->si,"browse")) == NULL)
        return M_PASS;

    log_debug("mod_browse","handling browse query");

    /* build the result IQ */
    query = xmlnode_insert_tag(jutil_iqresult(m->packet->x),"service");
    xmlnode_put_attrib(query,"xmlns",NS_BROWSE);
    xmlnode_put_attrib(query,"type","jabber");
    xmlnode_put_attrib(query,"jid",m->packet->to->server);
    xmlnode_put_attrib(query,"name",xmlnode_get_data(js_config(m->si,"vCard/FN"))); /* pull name from the server vCard */

    /* copy in the configured services */
    xmlnode_insert_node(query,xmlnode_get_firstchild(browse));

    /* list the admin stuff */
    if(js_admin(m->user, ADMIN_READ))
    {
        x = xmlnode_insert_tag(query,"item");
        xmlnode_put_attrib(x,"jid",spools(xmlnode_pool(x),m->packet->to->server,"/admin",xmlnode_pool(x)));
        xmlnode_put_attrib(x,"name","Online Users");
        xmlnode_insert_cdata(xmlnode_insert_tag(query,"ns"),NS_ADMIN,-1);
    }

    jpacket_reset(m->packet);
    js_deliver(m->si,m->packet);

    return M_HANDLED;
}

void mod_browse(jsmi si)
{
    js_mapi_register(si,e_SESSION,mod_browse_session,NULL);
    js_mapi_register(si,e_OFFLINE,mod_browse_reply,NULL);
    js_mapi_register(si,e_SERVER,mod_browse_server,NULL);
}


/*
<mass> wow
<mass> they had this four gallon thing of detergent at costco
<mass> I'm set for about two years
 * mass goes to read the directions on how to get soap out of it
<mass> the thing is really complicated. I'm reminded of the lament cube from Hellraiser
<mass> although I'm hoping I get soap instead of pinhead
*/

