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

mreturn mod_agents_agents(mapi m)
{
    xmlnode ret, retq, agents, cur, a, cur2;

    /* get data from the config file */
    agents = js_config(m->si,"browse");

    /* if we don't have anything to say, bounce */
    if(agents == NULL)
        return M_PASS;

    log_debug("mod_agents","handling agents query");

    /* build the result IQ */
    ret = jutil_iqresult(m->packet->x);
    retq = xmlnode_insert_tag(ret,"query");
    xmlnode_put_attrib(retq,"xmlns",NS_AGENTS);

    /* parse the new browse data into old agents format */
    for(cur = xmlnode_get_firstchild(agents); cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if(xmlnode_get_type(cur) != NTYPE_TAG) continue;

        /* generic <agent> part */
        a = xmlnode_insert_tag(retq,"agent");
        xmlnode_put_attrib(a, "jid", xmlnode_get_attrib(cur,"jid"));
        xmlnode_insert_cdata(xmlnode_insert_tag(a,"name"), xmlnode_get_attrib(cur,"name"), -1);
        xmlnode_insert_cdata(xmlnode_insert_tag(a,"service"), xmlnode_get_attrib(cur,"type"), -1);

        if(j_strcmp(xmlnode_get_name(cur),"conference") == 0)
            xmlnode_insert_tag(a,"groupchat");

        /* map the included <ns>'s in browse to the old agent flags */
        for(cur2 = xmlnode_get_firstchild(cur); cur2 != NULL; cur2 = xmlnode_get_nextsibling(cur2))
        {
            if(j_strcmp(xmlnode_get_name(cur2),"ns") != 0) continue;
            if(j_strcmp(xmlnode_get_data(cur2),"jabber:iq:register") == 0)
                xmlnode_insert_tag(a,"register");
            if(j_strcmp(xmlnode_get_data(cur2),"jabber:iq:search") == 0)
                xmlnode_insert_tag(a,"search");
            if(j_strcmp(xmlnode_get_data(cur2),"jabber:iq:gateway") == 0)
                xmlnode_insert_cdata(xmlnode_insert_tag(a,"transport"),"Enter ID", -1);
        }
    }

    jpacket_reset(m->packet);
    if(m->s != NULL) /* XXX null session hack! */
    {
        xmlnode_put_attrib(m->packet->x,"from",m->packet->from->server);
        js_session_to(m->s,m->packet);
    }else{
        js_deliver(m->si,m->packet);
    }

    return M_HANDLED;
}

mreturn mod_agents_agent(mapi m)
{
    xmlnode ret, retq, info, agents, reg;

    /* get data from the config file */
    info = js_config(m->si,"vCard");
    agents = js_config(m->si,"agents");
    reg = js_config(m->si,"register");

    /* if we don't have anything to say, bounce */
    if(info == NULL && agents == NULL && reg == NULL)
        return M_PASS;

    log_debug("mod_agent","handling agent query");

    /* build the result IQ */
    ret = jutil_iqresult(m->packet->x);
    retq = xmlnode_insert_tag(ret,"query");
    xmlnode_put_attrib(retq,"xmlns",NS_AGENT);

    /* copy in the vCard info */
    xmlnode_insert_cdata(xmlnode_insert_tag(retq,"name"),xmlnode_get_tag_data(info,"FN"),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(retq,"url"),xmlnode_get_tag_data(info,"URL"),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(retq,"service"),"jabber",6);

    /* set the flags */
    if(agents != NULL)
        xmlnode_insert_tag(retq,"agents");
    if(reg != NULL)
        xmlnode_insert_tag(retq,"register");

    jpacket_reset(m->packet);
    if(m->s != NULL) /* XXX null session hack! */
    {
        xmlnode_put_attrib(m->packet->x,"from",m->packet->from->server);
        js_session_to(m->s,m->packet);
    }else{
        js_deliver(m->si,m->packet);
    }

    return M_HANDLED;
}

mreturn mod_agents_handler(mapi m, void *arg)
{
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;

    if(jpacket_subtype(m->packet) != JPACKET__GET) return M_PASS;
    if(m->s != NULL && (m->packet->to != NULL && j_strcmp(jid_full(m->packet->to),m->packet->from->server) != 0)) return M_PASS; /* for session calls, only answer to=NULL or to=server */

    if(NSCHECK(m->packet->iq,NS_AGENT)) return mod_agents_agent(m);
    if(NSCHECK(m->packet->iq,NS_AGENTS)) return mod_agents_agents(m);

    return M_PASS;
}

/* XXX supid null to workaround! */
mreturn mod_agents_shack(mapi m, void *arg)
{
    js_mapi_session(es_OUT,m->s,mod_agents_handler,NULL);
    return M_PASS;
}

void mod_agents(jsmi si)
{
    log_debug("mod_agents","init");
    js_mapi_register(si,e_SERVER, mod_agents_handler, NULL);
    js_mapi_register(si,e_SESSION, mod_agents_shack, NULL);
}
