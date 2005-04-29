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
#include <sys/utsname.h>

typedef struct
{
    pool p;
    char *name;
    char *version;
    char *os;
} _mod_version_i, *mod_version_i;

mreturn mod_version_reply(mapi m, void *arg)
{
    mod_version_i mi = (mod_version_i)arg;

    if(m->packet->type != JPACKET_IQ) return M_IGNORE;
    if(!NSCHECK(m->packet->iq,NS_VERSION) || m->packet->to->resource != NULL) return M_PASS;

    /* first, is this a valid request? */
    if(jpacket_subtype(m->packet) != JPACKET__GET)
    {
        js_bounce(m->si,m->packet->x,TERROR_NOTALLOWED);
        return M_HANDLED;
    }

    log_debug("mod_version","handling query from",jid_full(m->packet->from));

    jutil_iqresult(m->packet->x);
    xmlnode_put_attrib(xmlnode_insert_tag(m->packet->x,"query"),"xmlns",NS_VERSION);
    jpacket_reset(m->packet);
    xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->iq,"name"),mi->name,j_strlen(mi->name));
    xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->iq,"version"),mi->version,j_strlen(mi->version));
    xmlnode_insert_cdata(xmlnode_insert_tag(m->packet->iq,"os"),mi->os,j_strlen(mi->os));
    
    js_deliver(m->si,m->packet);

    return M_HANDLED;
}

mreturn mod_version_shutdown(mapi m, void *arg)
{
    mod_version_i mi = (mod_version_i)arg;
    pool_free(mi->p);
    
    return M_PASS;
}

void mod_version(jsmi si)
{
    char *from;
    xmlnode x, config, name, version, os;
    pool p;
    mod_version_i mi;
    struct utsname un;

    p = pool_new();
    mi = pmalloco(p,sizeof(_mod_version_i));
    mi->p = p;

    /* get the values that should be reported by mod_version */
    uname(&un);
    config = js_config(si,"mod_version");
    name = xmlnode_get_tag(config, "name");
    version = xmlnode_get_tag(config, "version");
    os = xmlnode_get_tag(config, "os");

    mi->name = pstrdup(p, name ? xmlnode_get_data(name) : "jabberd");
    if (version)
	mi->version = pstrdup(p, xmlnode_get_data(version));
    else
	/* knowing if the server has been compiled with IPv6 is very helpful
	 * for debugging dialback problems */
#ifdef WITH_IPV6
	mi->version = spools(p, VERSION, "-ipv6", p);
#else
    	mi->version = pstrdup(p, VERSION);
#endif
    if (os)
	mi->os = pstrdup(p, xmlnode_get_data(os));
    else if (xmlnode_get_tag(config, "no_os_version"))
	mi->os = pstrdup(p, un.sysname);
    else
	mi->os = spools(p, un.sysname, " ", un.release, p);


    js_mapi_register(si,e_SERVER,mod_version_reply,(void *)mi);
    js_mapi_register(si,e_SHUTDOWN,mod_version_shutdown,(void *)mi);

    /* check for updates */
    from = xmlnode_get_data(js_config(si,"update"));
    if(from == NULL) return;

    x = xmlnode_new_tag("presence");
    xmlnode_put_attrib(x,"from",from);
    xmlnode_put_attrib(x,"to","jsm@update.jabber.org/" VERSION);
    deliver(dpacket_new(x), si->i);
}

