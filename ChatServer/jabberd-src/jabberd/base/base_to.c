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

#include "jabberd.h"

result base_to_deliver(instance id,dpacket p,void* arg)
{
    char* log_data = xmlnode_get_data(p->x);
    char* subject;
    xmlnode message;

    if(log_data == NULL)
        return r_ERR;

    message = xmlnode_new_tag("message");
    
    xmlnode_insert_cdata(xmlnode_insert_tag(message,"body"), log_data, -1);
    subject=spools(xmlnode_pool(message), "Log Packet from ", xmlnode_get_attrib(p->x, "from"), xmlnode_pool(message));
    xmlnode_insert_cdata(xmlnode_insert_tag(message, "thread"), shahash(subject), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(message, "subject"), subject, -1);
    xmlnode_put_attrib(message, "from", xmlnode_get_attrib(p->x, "from"));
    xmlnode_put_attrib(message, "to", (char*)arg);

    deliver(dpacket_new(message), id);
    pool_free(p->p);

    return r_DONE;
}

result base_to_config(instance id, xmlnode x, void *arg)
{
    if(id == NULL)
    {
        jid j = jid_new(xmlnode_pool(x), xmlnode_get_data(x));

        log_debug(ZONE,"base_to_config validating configuration\n");
        if(j == NULL)
        {
            xmlnode_put_attrib(x, "error", "'to' tag must contain a jid to send log data to");
            log_debug(ZONE, "Invalid Configuration for base_to");
            return r_ERR;
        }
        return r_PASS;
    }

    log_debug(ZONE, "base_to configuring instance %s", id->id);

    if(id->type != p_LOG)
    {
        log_alert(NULL, "ERROR in instance %s: <to>..</to> element only allowed in log sections", id->id);
        return r_ERR;
    }

    register_phandler(id, o_DELIVER, base_to_deliver, (void*)xmlnode_get_data(x));

    return r_DONE;
}

void base_to(void)
{
    log_debug(ZONE,"base_to loading...");
    register_config("to",base_to_config,NULL);
}
