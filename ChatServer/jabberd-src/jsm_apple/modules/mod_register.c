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

mreturn mod_register_new(mapi m, void *arg)
{
    xmlnode reg, x;

    if((reg = js_config(m->si, "register")) == NULL) return M_PASS;

    log_debug("mod_register","checking");

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__GET:

        /* copy in the registration fields from the config file */
        xmlnode_insert_node(m->packet->iq,xmlnode_get_firstchild(reg));

        break;

    case JPACKET__SET:

        log_debug(ZONE,"processing valid registration for %s",jid_full(m->packet->to));

        /* save the registration data */
        jutil_delay(m->packet->iq,"registered");
        /* don't store password in clear text in the NS_REGISTER namespace */
        xmlnode_hide(xmlnode_get_tag(m->packet->iq,"password"));
        xdb_set(m->si->xc, jid_user(m->packet->to), NS_REGISTER, m->packet->iq);

        /* if configured to, send admins a notice */
        if(xmlnode_get_attrib(reg,"notify") != NULL)
        {
            x = jutil_msgnew(NULL,
                m->packet->to->server,
                "Registration Notice",
                spools(m->packet->p,"The user ",jid_full(m->packet->to)," was just created with the following registration data: ",xmlnode2str(m->packet->iq),m->packet->p));
            xmlnode_put_attrib(x, "from", m->packet->to->server);
            js_deliver(m->si,jpacket_new(x));
        }

        /* if also configured, send the new user a welcome message */
        if((reg = js_config(m->si, "welcome")) != NULL)
        {
            x = xmlnode_new_tag("message");
            xmlnode_put_attrib(x, "from", m->packet->to->server);
            xmlnode_put_attrib(x, "to", jid_full(m->packet->to));
            xmlnode_insert_node(x, xmlnode_get_firstchild(reg));
            js_deliver(m->si,jpacket_new(x));
        }

        /* clean up and respond */
        jutil_iqresult(m->packet->x);
        break;

    default:
        return M_PASS;
    }

    return M_HANDLED;
}

mreturn mod_register_server(mapi m, void *arg)
{
    xmlnode reg, cur, check;

    /* pre-requisites */
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(!NSCHECK(m->packet->iq,NS_REGISTER)) return M_PASS;
    if(m->user == NULL) return M_PASS;
    if(js_config(m->si,"register") == NULL) return M_PASS;

    log_debug("mod_register","updating server: %s, user %s",m->user->id->server,jid_full(m->user->id));

    /* check for their registration */
    reg =  xdb_get(m->si->xc, m->user->id, NS_REGISTER);

    switch(jpacket_subtype(m->packet))
    {
    case JPACKET__GET:
        /* create reply to the get */
        xmlnode_put_attrib(m->packet->x,"type","result");
        jutil_tofrom(m->packet->x);

        /* copy in the registration fields from the config file */
        xmlnode_insert_node(m->packet->iq,xmlnode_get_firstchild(js_config(m->si,"register")));

        /* insert the key, we don't need to check it, but we'll send it :) */
        xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->iq,"key"),jutil_regkey(NULL,"foobar"),-1);

        /* replace fields with already-registered ones */
        for(cur = xmlnode_get_firstchild(m->packet->iq); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        {
            if(xmlnode_get_type(cur) != NTYPE_TAG) continue;

            check = xmlnode_get_tag(reg,xmlnode_get_name(cur));
            if(check == NULL) continue;

            xmlnode_insert_node(cur,xmlnode_get_firstchild(check));
        }

        /* add the registered flag */
        xmlnode_insert_tag(m->packet->iq,"registered");

        break;

    case JPACKET__SET:
        if(xmlnode_get_tag(m->packet->iq,"remove") != NULL)
        {
            log_notice(m->user->id->server,"User Unregistered: %s",m->user->user);

            /* XXX BRUTE FORCE: remove the registration and auth and any misc data */
            xdb_set(m->si->xc, m->user->id, NS_REGISTER, NULL);
            xdb_set(m->si->xc, m->user->id, NS_AUTH, NULL);
            xdb_set(m->si->xc, m->user->id, NS_AUTH_CRYPT, NULL);
            xdb_set(m->si->xc, m->user->id, NS_PRIVATE, NULL);
            xdb_set(m->si->xc, m->user->id, NS_ROSTER, NULL);
            xdb_set(m->si->xc, m->user->id, NS_VCARD, NULL);
            xdb_set(m->si->xc, m->user->id, NS_OFFLINE, NULL);
            xdb_set(m->si->xc, m->user->id, NS_FILTER, NULL);
        }else{
            log_debug(ZONE,"updating registration for %s",jid_full(m->user->id));

            /* update the registration data */
            xmlnode_hide(xmlnode_get_tag(m->packet->iq,"username")); /* hide the username/password from the reg db */
            xmlnode_hide(xmlnode_get_tag(m->packet->iq,"password"));
            jutil_delay(m->packet->iq,"updated");
            xdb_set(m->si->xc, m->user->id, NS_REGISTER, m->packet->iq);

        }
        /* clean up and respond */
        jutil_iqresult(m->packet->x);
        break;

    default:
        xmlnode_free(reg);
        return M_PASS;
    }

    xmlnode_free(reg);
    js_deliver(m->si, jpacket_reset(m->packet));
    return M_HANDLED;
}

void mod_register(jsmi si)
{
    log_debug("mod_register","init");
    js_mapi_register(si, e_REGISTER, mod_register_new, NULL);
    js_mapi_register(si, e_SERVER, mod_register_server, NULL);
}
