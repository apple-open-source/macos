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
#include <jsm.h>

mreturn mod_auth_digest_yum(mapi m, void *arg)
{
    spool s;
    char *sid;
    char *digest;
    char *mydigest;

    log_debug("mod_auth_digest","checking");

    if(jpacket_subtype(m->packet) == JPACKET__GET)
    { /* type=get means we flag that the server can do digest auth */
        if(m->user->pass != NULL)
            xmlnode_insert_tag(m->packet->iq,"digest");
        return M_PASS;
    }

    if((digest = xmlnode_get_tag_data(m->packet->iq,"digest")) == NULL)
        return M_PASS;

    sid = xmlnode_get_attrib(xmlnode_get_tag(m->packet->iq,"digest"), "sid");

    /* Concat the stream id and password */
    /* SHA it up */
    log_debug("mod_auth_digest", "Got SID: %s", sid);
    s = spool_new(m->packet->p);
    spooler(s,sid,m->user->pass,s);

    mydigest = shahash(spool_print(s));

    log_debug("mod_auth_digest","comparing %s %s",digest,mydigest);

    if(m->user->pass == NULL || sid == NULL || mydigest == NULL)
        jutil_error(m->packet->x, TERROR_NOTIMPL);
    else if(j_strcasecmp(digest, mydigest) != 0)
        jutil_error(m->packet->x, TERROR_AUTH);
    else
        jutil_iqresult(m->packet->x);

    return M_HANDLED;
}

int mod_auth_digest_reset(mapi m, jid id, xmlnode pass)
{
    log_debug("mod_auth_digest","resetting password");

    xmlnode_put_attrib(pass,"xmlns",NS_AUTH);
    return xdb_set(m->si->xc, id, NS_AUTH, pass);
}

/* handle saving the password for registration */
mreturn mod_auth_digest_reg(mapi m, void *arg)
{
    jid id;
    xmlnode pass;

    if(jpacket_subtype(m->packet) == JPACKET__GET)
    { /* type=get means we flag that the server can do plain-text regs */
        xmlnode_insert_tag(m->packet->iq,"password");
        return M_PASS;
    }

    if(jpacket_subtype(m->packet) != JPACKET__SET || (pass = xmlnode_get_tag(m->packet->iq,"password")) == NULL) return M_PASS;

    /* get the jid of the user, depending on how we were called */
    if(m->user == NULL)
        id = jid_user(m->packet->to);
    else
        id = m->user->id;

    /* tuck away for a rainy day */
    if(mod_auth_digest_reset(m,id,pass))
    {
        jutil_error(m->packet->x,(terror){500,"Password Storage Failed"});
        return M_HANDLED;
    }

    return M_PASS;
}

/* handle password change requests from a session */
mreturn mod_auth_digest_server(mapi m, void *arg)
{
    mreturn ret;

    /* pre-requisites */
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(m->user == NULL) return M_PASS;
    if(!NSCHECK(m->packet->iq,NS_REGISTER)) return M_PASS;

    /* just do normal reg process, but deliver afterwards */
    ret = mod_auth_digest_reg(m,arg);
    if(ret == M_HANDLED)
        js_deliver(m->si, jpacket_reset(m->packet));

    return ret;
}

void mod_auth_digest(jsmi si)
{
    log_warn(NULL,"mod_auth_digest in jsm_apple.so is diabled");
#if 0
   log_debug("mod_auth_digest","init");
    js_mapi_register(si,e_AUTH, mod_auth_digest_yum, NULL);
    js_mapi_register(si,e_SERVER, mod_auth_digest_server, NULL);
    if (js_config(si,"register") != NULL) js_mapi_register(si, e_REGISTER, mod_auth_digest_reg, NULL);
#endif
}
