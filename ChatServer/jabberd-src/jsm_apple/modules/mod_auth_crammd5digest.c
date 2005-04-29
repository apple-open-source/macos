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
#include "apple_authorize.h"
#include "apple_authenticate.h"
#include <openssl/rand.h>

#define TEST_CODE 0
static int auto_detect_enabled = 0;

#if TEST_CODE
int testChallenge(mapi m, unsigned long *destChars)
{ // test code

    int numchars = 0;
    char randombytes[ 32 ];
    unsigned long sourcesize = sizeof(randombytes);
    int destsize = sizeof(m->user->authCookie);
    
    if (NULL != m->user->authCookie)
        m->user->authCookie[0] = 0;

    if (destsize < 5) // just in case it changes type or becomes too small
        return 0;
    
    if (0 == RAND_bytes(randombytes, sizeof(randombytes) ) ) // no random bytes available 
        return 0;

    
    numchars = ConvertBytesToHexChars( randombytes,  sourcesize - sourcesize, m->user->authCookie,(unsigned long) destsize - destsize, destChars);
    log_debug("testChallenge1", "numchars=%d, destChars=%lu sourcesize=%d destsize=%d\n",numchars, *destChars, sourcesize, destsize); 

    numchars = ConvertBytesToHexChars( randombytes,  sourcesize, m->user->authCookie,(unsigned long) destsize - destsize, destChars);
    log_debug("testChallenge2", "numchars=%d, destChars=%lu sourcesize=%d destsize=%d\n",numchars, *destChars, sourcesize, destsize); 

    numchars = ConvertBytesToHexChars( randombytes,  sourcesize -1, m->user->authCookie,(unsigned long) destsize -1, destChars);
    log_debug("testChallenge3", "numchars=%d, destChars=%lu sourcesize=%d destsize=%d\n",numchars, *destChars, sourcesize, destsize); 

    numchars = ConvertBytesToHexChars( randombytes,  sourcesize, m->user->authCookie,(unsigned long) destsize -2, destChars);
    log_debug("testChallenge4", "numchars=%d, destChars=%lu sourcesize=%d destsize=%d\n",numchars, *destChars, sourcesize, destsize); 

    numchars = ConvertBytesToHexChars( randombytes,  sourcesize, m->user->authCookie,(unsigned long) destsize, destChars);
    log_debug("testChallenge5-ok", "numchars=%d, destChars=%lu sourcesize=%d destsize=%d\n",numchars, *destChars, sourcesize, destsize); 

    if (numchars != sourcesize || *destChars > destsize) // failed 
        log_debug("testChallenge", "ConvertBytes failed\n"); 
 
    return numchars;
}
#endif

int createChallenge(mapi m)
{
    int numchars = 0;
    char randombytes[ 32 ];
    int destsize = sizeof(m->user->authCookie);
    unsigned long destChars = 0;
		
    if (NULL != m->user->authCookie)
        m->user->authCookie[0] = 0;

    if (destsize < 5) // just in case it changes type or becomes too small
        return 0;
    
    if (0 == RAND_bytes(randombytes, sizeof(randombytes) ) )
        return 0;
    
    // test routine for ConvertBytesToHexChars
    // numchars = testChallenge(m, &destChars);

    numchars = ConvertBytesToHexChars( randombytes,  sizeof(randombytes), m->user->authCookie,(unsigned long) destsize - 1, &destChars);
    if (numchars != sizeof(randombytes) || destChars >= destsize) // failed to convert all the bytes or converted too many bytes
        numchars = destChars = 0; 
        
    m->user->authCookie[destChars] = 0; // c string terminate
    return numchars;
}

mreturn mod_auth_cram_md5_digest(mapi m, void *arg)
{
    char *client_digest = NULL;
    char *challenge = NULL;

    log_debug("mod_auth_cram_md5_digest","checking");

    if(jpacket_subtype(m->packet) == JPACKET__GET)
    {   /* type=get means we flag that the server can do crammd5 digest auth */
    
    if (auto_detect_enabled && !ds_supports_cram_md5((const char *) m->user->user))
        return M_PASS;
  
        if (0 == createChallenge(m))
        {
             log_error("mod_auth_cram_md5_digest","crammd5 challenge generation failed. crammd5 disabled.");
        }
        else
        { 
            xmlnode_insert_tag(m->packet->iq,"crammd5");
            xmlnode_put_attrib(xmlnode_get_tag(m->packet->iq, "crammd5"), "challenge", m->user->authCookie);
        }
        return M_PASS;
    }
    
    client_digest = xmlnode_get_tag_data(m->packet->iq,"crammd5");
    if( NULL == client_digest )
        return M_PASS;

    challenge = m->user->authCookie;
    if( NULL == challenge || 0 == challenge[0]  )
    {	
    	log_debug("mod_auth_cram_md5_digest","can't find crammd5 challenge");
	jutil_error(m->packet->x, TERROR_AUTH);
	return M_HANDLED;
    }
   
    log_debug("mod_auth_cram_md5_digest","have crammd5 digest:%s and challenge:%s", client_digest, challenge);
    if (kAuthenticated != authenticate_cram_md5 ( (const char *) m->user->user, (const char *) challenge, (const char *) client_digest  ) )
    {   
    	log_debug("mod_auth_cram_md5_digest","authenticate_cram_md5 failed");    
    	jutil_error(m->packet->x, TERROR_AUTH);
	return M_HANDLED; // no other 
    }
    
    if (kAuthorized != apple_check_sacl(m->user->user)) // not authorized
    {     
    	log_debug("mod_auth_cram_md5_digest","apple_check_sacl failed");
        jutil_error(m->packet->x, TERROR_AUTH);
        return M_HANDLED; // no other 
    }
    
   
    jutil_iqresult(m->packet->x);

    return M_HANDLED;
}

int mod_auth_cram_md5_digest_reset(mapi m, jid id, xmlnode pass)
{
     jutil_error(m->packet->x,TERROR_UNAVAIL);
     return M_HANDLED;
}

/* handle saving the password for registration */
mreturn mod_auth_cram_md5_digest_reg(mapi m, void *arg)
{
     jutil_error(m->packet->x,TERROR_UNAVAIL);
     return M_HANDLED;
}

/* handle password change requests from a session */
mreturn mod_auth_cram_md5_digest_server(mapi m, void *arg)
{
     jutil_error(m->packet->x,TERROR_UNAVAIL);
     return M_HANDLED;
}

void mod_auth_crammd5digest(jsmi si)
{
    log_debug("mod_auth_cram_md5_digest","init");
    js_mapi_register(si,e_AUTH, mod_auth_cram_md5_digest, NULL);
    js_mapi_register(si,e_SERVER, mod_auth_cram_md5_digest_server, NULL);
    if (js_config(si,"register") != NULL) js_mapi_register(si, e_REGISTER, mod_auth_cram_md5_digest_reg, NULL);

    /* setup the default built-in rule */
    if (NULL == js_config(si,"mod_auth_crammd5digest/auto_detect_password_support"))
    {   
        auto_detect_enabled = 0;
        log_debug("mod_auth_cram_md5_digest","didn't find autodetect pref: disabled");
    }
    else
    {   
        auto_detect_enabled = 1;
        log_debug("mod_auth_cram_md5_digest","found autodetect pref: enabled");
    }

}
