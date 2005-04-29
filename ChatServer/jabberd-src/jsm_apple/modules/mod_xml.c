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

mreturn mod_xml_set(mapi m, void *arg)
{
    xmlnode storedx, inx = m->packet->iq;
    char *ns = xmlnode_get_attrib(m->packet->iq,"xmlns");
    jid to = m->packet->to;
    int private = 0;
    jpacket jp;

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;

    /* check for a private request */
    if(NSCHECK(m->packet->iq,NS_PRIVATE))
    {
        private = 1;
        inx = xmlnode_get_tag(m->packet->iq,"?xmlns");
        ns = xmlnode_get_attrib(inx,"xmlns");
        if(ns == NULL || strncmp(ns,"jabber:",7) == 0 || strcmp(ns,"vcard-temp") == 0)
        { /* uhoh, can't use jabber: namespaces inside iq:private! */
            jutil_error(m->packet->x,TERROR_NOTACCEPTABLE);
            js_session_to(m->s,m->packet);
            return M_HANDLED;
        }
    }else if(j_strncmp(ns,"jabber:",7) == 0 || j_strcmp(ns,"vcard-temp") == 0){ /* cant set public xml jabber: namespaces either! */
         return M_PASS;
    }

    /* if its to someone other than ourselves */
    if(m->packet->to != NULL) return M_PASS;

    log_debug(ZONE,"handling user request %s",xmlnode2str(m->packet->iq));

    /* no to implies to ourselves */
    if(to == NULL)
        to = m->user->id;

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__GET:
        log_debug("mod_xml","handling get request for %s",ns);
        xmlnode_put_attrib(m->packet->x,"type","result");

        /* insert the chunk into the parent, that being either the iq:private container or the iq itself */
        if((storedx = xdb_get(m->si->xc, to, ns)) != NULL)
        {
            if(private) /* hack, ick! */
                xmlnode_hide_attrib(storedx,"j_private_flag");
            xmlnode_insert_tag_node(xmlnode_get_parent(inx), storedx);
            xmlnode_hide(inx);
        }

        /* send to the user */
        jpacket_reset(m->packet);
        js_session_to(m->s,m->packet);
        xmlnode_free(storedx);

        break;

    case JPACKET__SET:
        log_debug("mod_xml","handling set request for %s with data %s",ns,xmlnode2str(inx));

        /* save the changes */
        if(private) /* hack, ick! */
            xmlnode_put_attrib(inx,"j_private_flag","1");
        if(xdb_set(m->si->xc, to, ns, inx))
            jutil_error(m->packet->x,TERROR_UNAVAIL);
        else
            jutil_iqresult(m->packet->x);

        /* insert the namespace on the list */
        storedx = xmlnode_new_tag("ns");
        xmlnode_insert_cdata(storedx,ns,-1);
        if(private)
            xmlnode_put_attrib(storedx,"type","private");
        xdb_act(m->si->xc, to, NS_XDBNSLIST, "insert", spools(m->packet->p,"ns=",ns,m->packet->p), storedx); /* match and replace any existing namespaces already listed */
        xmlnode_free(storedx);

        /* if it's to a resource that isn't browseable yet, fix that */
        if(to->resource != NULL)
        {
            if((storedx = xdb_get(m->si->xc, to, NS_BROWSE)) == NULL)
            { /* send an iq set w/ a generic browse item for this resource */
                jp = jpacket_new(jutil_iqnew(JPACKET__SET,NS_BROWSE));
                storedx = xmlnode_insert_tag(jp->iq, "item");
                xmlnode_put_attrib(storedx, "jid", jid_full(to));
                js_session_from(m->s, jp);
            }else{
                xmlnode_free(storedx);
            }
        }

        /* send to the user */
        jpacket_reset(m->packet);
        js_session_to(m->s,m->packet);

        break;

    default:
        return M_PASS;
    }

    return M_HANDLED;
}

mreturn mod_xml_get(mapi m, void *arg)
{
    xmlnode xns;
    char *ns = xmlnode_get_attrib(m->packet->iq,"xmlns");

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(j_strncmp(ns,"jabber:",7) == 0 || j_strcmp(ns,"vcard-temp") == 0) return M_PASS; /* only handle alternate namespaces */

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

    log_debug("mod_xml","handling %s request for user %s",ns,jid_full(m->packet->to));

    /* get the foreign namespace */
    xns = xdb_get(m->si->xc, m->packet->to, ns);

    if(xmlnode_get_attrib(xns,"j_private_flag") != NULL)
    { /* uhoh, set from a private namespace */
        js_bounce(m->si,m->packet->x,TERROR_NOTALLOWED);
        return M_HANDLED;
    }

    /* reply to the request w/ any data */
    jutil_iqresult(m->packet->x);
    jpacket_reset(m->packet);
    xmlnode_insert_tag_node(m->packet->x,xns);
    js_deliver(m->si,m->packet);

    xmlnode_free(xns);
    return M_HANDLED;
}

mreturn mod_xml_session(mapi m, void *arg)
{
    js_mapi_session(es_OUT,m->s,mod_xml_set,NULL);
    return M_PASS;
}

void mod_xml(jsmi si)
{
    js_mapi_register(si,e_SESSION,mod_xml_session,NULL);
    js_mapi_register(si,e_OFFLINE,mod_xml_get,NULL);
}


