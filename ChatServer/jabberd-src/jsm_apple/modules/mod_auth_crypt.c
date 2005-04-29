/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "License").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the License.  You
 * may obtain a copy of the License at http://www.jabber.com/license/ or at
 * http://www.opensource.org/.  
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyrights
 * 
 * Portions created by or assigned to Jabber.com, Inc. are 
 * Copyright (c) 1999-2000 Jabber.com, Inc.  All Rights Reserved.  Contact
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
 * --------------------------------------------------------------------------*/
#include "jsm.h"
//jkm
//#include "crypt.h"
#include <unistd.h>
//jkm

#define HASH_CRYPT 1
#define HASH_SHA1  2

/**
 * this function hashes the given password with the SHA1 and formats the
 * result to be usable for password storrage
 *
 * @param 
 * @return 1 on success, 0 otherwise
 */
int mod_auth_crypt_sha1(char *password, char *buf, size_t buflen)
{
    unsigned char hash[20];

    /* our result is 34 characters long and we need a terminating '\0' */
    if (buflen < 35)
	return 0;

    /* the pointers have to be valid */
    if (password == NULL || buf == NULL)
	return 0;

    /* calculate the hash */
    shaBlock(password, j_strlen(password), hash);

    /* write the result */
    strcpy(buf, "{SHA}");
    return base64_encode(hash, sizeof(hash), buf+5, buflen-5);
}

mreturn mod_auth_crypt_jane(mapi m, void *arg)
{
    char *passA, *passB;
    char salt[3];
    char shahash[35];
    xmlnode xdb;

    log_debug("mod_auth_crypt","checking");

    if(jpacket_subtype(m->packet) == JPACKET__GET)
    { /* type=get means we flag that the server can do plain-text auth */
        xmlnode_insert_tag(m->packet->iq,"password");
        return M_PASS;
    }

    if((passA = xmlnode_get_tag_data(m->packet->iq, "password")) == NULL)
        return M_PASS;

    /* make sure we can get the auth packet and that it contains a password */
    xdb = xdb_get(m->si->xc, m->user->id, NS_AUTH_CRYPT);
    if(xdb == NULL || (passB = xmlnode_get_data(xdb)) == NULL)
    {
        xmlnode_free(xdb);
        return M_PASS;
    }

    /* check which hashing algoithm has been used */
    if (j_strncmp(passB, "{SHA}", 5) == 0)
    {
	/* it is SHA-1 */
	mod_auth_crypt_sha1(passA, shahash, sizeof(shahash));
	passA = shahash;
	log_debug("mod_auth_crypt","comparing %s %s",shahash,passB);
    }
    else
    {
	/* it is traditional crypt() */
	strncpy(salt, passB, 2);
	salt[2] = '\0';
	passA = crypt(passA, salt);
	log_debug("mod_auth_crypt","comparing %s %s",passA,passB);
    }

    if(strcmp(passA, passB) != 0)
	jutil_error(m->packet->x, TERROR_AUTH);
    else
	jutil_iqresult(m->packet->x);

    xmlnode_free(xdb); /* free xdb results */

    return M_HANDLED;
}

static char* _get_salt()
{
    static char result[3] = { '\0', '\0', '\0'};
    int i;
    if (!result[0]) srand(time(NULL));
    i = 0;
    for (i = 0; i < 2; i++)
    {
        result[i] = (char)(rand() % 64) + '.';
        if (result[i] <= '9') continue;
        result[i] += 'A' - '9' - 1;
        if (result[i] <= 'Z') continue;
        result[i] += 'a' - 'Z' - 1;
    }
    return result;
}

int mod_auth_crypt_reset(mapi m, jid id, xmlnode pass)
{
    char shahash[35];
    char* password;
    xmlnode newpass;
    char* hashalgo;
    int usedhashalgo;

    log_debug("mod_auth_crypt","resetting password");

    hashalgo = xmlnode_get_tag_data(js_config(m->si, "mod_auth_crypt"), "hash");
    if (j_strcasecmp(hashalgo, "SHA1") == 0)
    {
	usedhashalgo = HASH_SHA1;
    }
    else
    {
	usedhashalgo = HASH_CRYPT;
    }

    password = xmlnode_get_data(pass);
    if(password == NULL) return 1;
    newpass = xmlnode_new_tag("crypt");

    switch (usedhashalgo)
    {
	case HASH_SHA1:
	    mod_auth_crypt_sha1(password, shahash, sizeof(shahash));
	    log_debug("mod_auth_crypt", "SHA1 hash is %s", shahash);
	    if (xmlnode_insert_cdata(newpass, shahash, -1) == NULL)
		return -1;
	    break;
	default:
	    if (xmlnode_insert_cdata(newpass, crypt(password, _get_salt()), -1) == NULL)
		return -1;
    }
    
    xmlnode_put_attrib(newpass,"xmlns",NS_AUTH_CRYPT);
    return xdb_set(m->si->xc, jid_user(id), NS_AUTH_CRYPT, newpass);
}

/* handle saving the password for registration */
mreturn mod_auth_crypt_reg(mapi m, void *arg)
{
    if(jpacket_subtype(m->packet) != JPACKET__SET) return M_PASS;

    if(mod_auth_crypt_reset(m,m->packet->to,xmlnode_get_tag(m->packet->iq,"password")))
    {
        jutil_error(m->packet->x,(terror){500,"Password Storage Failed"});
        return M_HANDLED;
    }

    return M_PASS;
}

/* handle password change requesdts from a session */
mreturn mod_auth_crypt_server(mapi m, void *arg)
{
    xmlnode pass;

    /* pre-requisites */
    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(jpacket_subtype(m->packet) != JPACKET__SET || !NSCHECK(m->packet->iq,NS_REGISTER)) return M_PASS;
    if(m->user == NULL) return M_PASS;
    if((pass = xmlnode_get_tag(m->packet->iq,"password")) == NULL) return M_PASS;

    if(mod_auth_crypt_reset(m,m->user->id,pass))
    {
        js_bounce(m->si,m->packet->x,(terror){500,"Password Storage Failed"});
        return M_HANDLED;
    }
    return M_PASS;
}

void mod_auth_crypt(jsmi si)
{
    log_warn(NULL,"mod_auth_crypt in jsm_apple.so is diabled");
#if 0
    log_debug("mod_auth_crypt","init");

    js_mapi_register(si, e_AUTH, mod_auth_crypt_jane, NULL);
    js_mapi_register(si, e_SERVER, mod_auth_crypt_server, NULL);
    if (js_config(si,"register") != NULL) js_mapi_register(si, e_REGISTER, mod_auth_crypt_reg, NULL);
#endif
}
