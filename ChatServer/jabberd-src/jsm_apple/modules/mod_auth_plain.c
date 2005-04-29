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
 * --------------------------------------------------------------------------*/
 
#include "jsm.h"
#include "apple_authorize.h"
#include "apple_authenticate.h"


void register_jid(mapi m, void *arg)
{
    jid id;

    // get the jid of the user, depending on how we were called 
    if(m->user == NULL)
        id = jid_user(m->packet->to);
    else
        id = m->user->id;
 
    /* save the registration data */
    jutil_delay(m->packet->iq,"registered");
    
    
    /* don't store password in clear text in the NS_REGISTER namespace */
    xmlnode_hide(xmlnode_get_tag(m->packet->iq,"password"));
    xdb_set(m->si->xc, id, NS_REGISTER, m->packet->iq);


}

void welcome(mapi m, void *arg)
{
    xmlnode reg, x;

    if((reg = js_config(m->si, "welcome")) != NULL)
    {
        x = xmlnode_new_tag("message");
        xmlnode_put_attrib(x, "from", m->packet->to->server);
        xmlnode_put_attrib(x, "to", jid_full(m->packet->to));
        xmlnode_insert_node(x, xmlnode_get_firstchild(reg));
        js_deliver(m->si,jpacket_new(x));
    }
    
}


mreturn mod_auth_plain_jane(mapi m, void *arg)
{
    char *pass;
    xmlnode reg; 
    
    log_debug(ZONE,"mod_auth_plain_jane checking %s", m->user->user);

    if(jpacket_subtype(m->packet) == JPACKET__GET)
    { /* type=get means we flag that the server can do plain-text auth */
        xmlnode_insert_tag(m->packet->iq,"password");
        return M_PASS;
    }

    if((pass = xmlnode_get_tag_data(m->packet->iq, "password")) == NULL)
     {   
        return M_PASS;
     }

    if (kAuthenticated != apple_checkpw(m->user->user, pass)) // not authenticated
    {     
        jutil_error(m->packet->x, TERROR_AUTH);
        return M_HANDLED; // no other 
    }
    
    if (kAuthorized != apple_check_sacl(m->user->user)) // not authorized
    {     
        jutil_error(m->packet->x, TERROR_AUTH);
        return M_HANDLED; // no other 
    }

  
    jutil_iqresult(m->packet->x);
         
    // see if the user is registered
    if((reg = xdb_get(m->si->xc, m->user->id, NS_REGISTER)) == NULL)
    {
        // not registered so register and welcome just this once
        register_jid(m, arg);
        welcome(m, arg);
    }
    
   
    return M_HANDLED; // no other 


}

int mod_auth_plain_reset(mapi m, jid id, xmlnode pass)
{
     jutil_error(m->packet->x,TERROR_UNAVAIL);
     return M_HANDLED;
}

/* handle saving the password for registration */
mreturn mod_auth_plain_reg(mapi m, void *arg)
{
     jutil_error(m->packet->x,TERROR_UNAVAIL);
     return M_HANDLED;
}

/* handle password change requests from a session */
mreturn mod_auth_plain_server(mapi m, void *arg)
{
    mreturn ret;

    /* pre-requisites */
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(m->user == NULL) return M_PASS;
    if(!NSCHECK(m->packet->iq,NS_REGISTER)) return M_PASS;

    /* just do normal reg process, but deliver afterwards */
    ret = mod_auth_plain_reg(m,arg);
    if(ret == M_HANDLED)
        js_deliver(m->si, jpacket_reset(m->packet));

    return ret;
}

void mod_auth_plain(jsmi si)
{
    log_debug("mod_auth_plain","init");

    js_mapi_register(si, e_AUTH, mod_auth_plain_jane, NULL);
    js_mapi_register(si, e_SERVER, mod_auth_plain_server, NULL);
    if (js_config(si,"register") != NULL) 
        js_mapi_register(si, e_REGISTER, mod_auth_plain_reg, NULL);
}
