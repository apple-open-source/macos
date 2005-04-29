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

int mod_auth_0k_set(mapi m, jid id, char *hash, char *token, char *sequence)
{
    xmlnode x;

    if(id == NULL || hash == NULL || token == NULL || sequence == NULL) return 1;

    log_debug(ZONE,"saving 0k data");

    /* when this is a new registration and in case there is no mod_auth_plain, we need to ensure the NS_AUTH flag exists */
    if(m->user == NULL)
    {
        if((x = xdb_get(m->si->xc, id, NS_AUTH)) != NULL)
        { /* cool, they exist */
            xmlnode_free(x);
        }else{ /* make them exist with an empty password */
            log_debug(ZONE,"NS_AUTH flag doesn't exist, creating");
            x = xmlnode_new_tag_pool(m->packet->p,"password");
            xmlnode_put_attrib(x,"xmlns",NS_AUTH);
            if(xdb_set(m->si->xc, id, NS_AUTH, x))
                return 1; /* uhoh */
        }
    }

    /* save the 0k vars */
    x = xmlnode_new_tag_pool(m->packet->p,"zerok");
    xmlnode_put_attrib(x,"xmlns",NS_AUTH_0K);
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"hash"),hash,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"token"),token,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"sequence"),sequence,-1);
    return xdb_set(m->si->xc, id, NS_AUTH_0K, x);
}

int mod_auth_0k_reset(mapi m, jid id, char *pass)
{
    char token[10];
    char seqs_default[] = "500";
    int sequence, i;
    char *seqs, hash[41];

    if(pass == NULL) return 1;

    log_debug(ZONE,"resetting 0k variables");

    /* figure out how many sequences to generate */
    seqs = xmlnode_get_tag_data(js_config(m->si, "mod_auth_0k"),"sequences");
    if(seqs == NULL)
        seqs = seqs_default;

    sequence = atoi(seqs);

    /* generate new token */
    sprintf(token,"%X",(int)time(NULL));

    /* first, hash the pass */
    shahash_r(pass,hash);
    /* next, hash that and the token */
    shahash_r(spools(m->packet->p,hash,token,m->packet->p),hash);
    /* we've got hash0, now make as many as the sequence is */
    for(i = 0; i < sequence; i++, shahash_r(hash,hash));

    return mod_auth_0k_set(m, id, hash, token, seqs);
}

mreturn mod_auth_0k_go(mapi m, void *enable)
{
    char *token, *hash, *seqs, *pass;
    char *c_hash = NULL;
    int sequence = 0, i;
    xmlnode xdb;

    if(   jpacket_subtype(m->packet) == JPACKET__SET && 
          (c_hash = xmlnode_get_tag_data(m->packet->iq,"hash")) == NULL &&
          (pass = xmlnode_get_tag_data(m->packet->iq,"password")) == NULL)
        return M_PASS;

    log_debug(ZONE,"checking");

    /* first we need to see if this user is using or can use 0k */
    if((xdb = xdb_get(m->si->xc, m->user->id, NS_AUTH_0K)) == NULL)
    {
        /* if there's no password or we can't set our own vars, we're doomed for failure */
        if(mod_auth_0k_reset(m,m->user->id,m->user->pass))
            return M_PASS;
        xdb = xdb_get(m->si->xc, m->user->id, NS_AUTH_0K);
    }

    /* extract data */
    seqs = xmlnode_get_tag_data(xdb,"sequence");
    if(seqs != NULL)
    { /* get the current sequence as an int for the logic, and the client sequence as a decrement */
        sequence = atoi(seqs);
        if(sequence > 0)
            sprintf(seqs,"%d",sequence - 1);
    }
    token = xmlnode_get_tag_data(xdb,"token");
    hash = xmlnode_get_tag_data(xdb,"hash");

    if(jpacket_subtype(m->packet) == JPACKET__GET)
    { /* type=get, send back current 0k stuff if we've got it */
        if(hash != NULL && token != NULL && sequence > 0)
        {
            xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->iq,"sequence"),seqs,-1);
            xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->iq,"token"),token,-1);
        }
        xmlnode_free(xdb);
        return M_PASS;
    }

    /* by this point if there's no c_hash, then there is a pass, and we need to generate the right c_hash */
    if(c_hash == NULL && enable)
    {
        log_debug(ZONE,"generating our own 0k from the plaintext password to match the stored vars");
        c_hash = pmalloc(m->packet->p,sizeof(char)*41);
        /* first, hash the pass */
        shahash_r(pass,c_hash);
        /* next, hash that and the token */
        shahash_r(spools(m->packet->p,c_hash,token,m->packet->p),c_hash);
        /* we've got hash0, now count up to just less than the sequence */
        for(i = 1; i < sequence; i++, shahash_r(c_hash,c_hash));
    }

    log_debug("mod_auth_0k","got client hash %s for sequence %d and token %s",c_hash,sequence,token);

    /* only way this passes is if they got a valid get result from above, and had the pass to generate this new hash */
    if(j_strcmp(shahash(c_hash), hash) != 0)
    {
        jutil_error(m->packet->x, TERROR_AUTH);
    }else{
        /* store the new current hash/sequence */
        xmlnode_hide(xmlnode_get_tag(xdb,"sequence"));
        xmlnode_insert_cdata(xmlnode_insert_tag(xdb,"sequence"),seqs,-1);
        xmlnode_hide(xmlnode_get_tag(xdb,"hash"));
        xmlnode_insert_cdata(xmlnode_insert_tag(xdb,"hash"),c_hash,-1);

        xmlnode_put_attrib(xdb,"xmlns",NS_AUTH_0K);
        if(xdb_set(m->si->xc, m->user->id, NS_AUTH_0K, xdb))
            jutil_error(m->packet->x, TERROR_REQTIMEOUT);
        else
            jutil_iqresult(m->packet->x);
    }

    xmlnode_free(xdb); /* free xdb results */

    return M_HANDLED;
}

/* handle saving the 0k data for registrations */
mreturn mod_auth_0k_reg(mapi m, void *arg)
{
    int disable = 1;
    jid id;

    /* only do 0k regs when told to */
    if(js_config(m->si, "mod_auth_0k/enable_registration") != NULL)
        disable = 0;

    /* type=get means we flag that the server can do 0k regs */
    if(jpacket_subtype(m->packet) == JPACKET__GET)
    {
        if(!disable)
            xmlnode_insert_tag(m->packet->iq,"hash");
        return M_PASS;
    }

    /* get the jid of the user, depending on how we were called */
    if(m->user == NULL)
        id = jid_user(m->packet->to);
    else
        id = m->user->id;

    if(jpacket_subtype(m->packet) != JPACKET__SET) return M_PASS;

    /* if the password is to be changed, just remove the old 0k auth vars, they'll get reset on next auth get */
    if(xmlnode_get_tag_data(m->packet->iq,"password") != NULL)
        xdb_set(m->si->xc, id, NS_AUTH_0K, NULL);

    /* if we can, set the 0k vars to what the client told us to */
    if(!disable && xmlnode_get_tag_data(m->packet->iq,"hash") != NULL && mod_auth_0k_set(m,id,xmlnode_get_tag_data(m->packet->iq,"hash"),xmlnode_get_tag_data(m->packet->iq,"token"),xmlnode_get_tag_data(m->packet->iq,"sequence")))
    {
        jutil_error(m->packet->x,(terror){500,"Authentication Storage Failed"});
        return M_HANDLED;
    }

    return M_PASS;
}

/* handle password change requests from a session */
mreturn mod_auth_0k_server(mapi m, void *arg)
{
    mreturn ret;

    /* pre-requisites if we're within a session */
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(m->user == NULL) return M_PASS;
    if(!NSCHECK(m->packet->iq,NS_REGISTER)) return M_PASS;

    /* just do normal reg process, but deliver afterwards */
    ret = mod_auth_0k_reg(m,arg);
    if(ret == M_HANDLED)
        js_deliver(m->si, jpacket_reset(m->packet));

    return ret;
}

void mod_auth_0k(jsmi si)
{
    void *enable = 0;

    log_debug(ZONE,"there goes the neighborhood");

    /* check once for enabling plaintext->0k auth */
    if(js_config(si, "mod_auth_0k/enable_plaintext") != NULL)
        enable = (void*)1;

    js_mapi_register(si, e_AUTH, mod_auth_0k_go, enable);
    js_mapi_register(si, e_SERVER, mod_auth_0k_server, NULL);
    if (js_config(si,"register") != NULL) js_mapi_register(si, e_REGISTER, mod_auth_0k_reg, NULL);
}
