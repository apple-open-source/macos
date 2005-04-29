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

/* * this was taken from wpjabber cvs - thanks guys :) */

#include "jsm.h"

#define NS_DISCO_INFO "http://jabber.org/protocol/disco#info"

mreturn mod_disco_server_info(mapi m, void *arg)
{
    xmlnode query, identity, disco;

    if((xmlnode_get_attrib(m->packet->x,"node")) != NULL) return M_PASS;
        
    log_debug("mod_disco","handling disco#info query");

    /* config get */
    disco = js_config(m->si,"disco");

	/* build the result IQ */		
    query = xmlnode_insert_tag(jutil_iqresult(m->packet->x),"query");
    xmlnode_put_attrib(query,"xmlns",NS_DISCO_INFO);

	/* if config */
	identity = NULL;
    if (disco != NULL) 
	  identity = xmlnode_get_tag(disco,"identity");
    
	/* if bad config , put identity */
    if (disco == NULL || identity == NULL){
	  identity = xmlnode_insert_tag(query,"identity");
	  xmlnode_put_attrib(identity,"category","services");
	  xmlnode_put_attrib(identity,"type","jabber");
	  xmlnode_put_attrib(identity,"name",
						 xmlnode_get_data(js_config(m->si,"vCard/FN"))); 
    }
    
	/* put disco info if exist */
    if (disco != NULL) 
	  xmlnode_insert_node(query, xmlnode_get_firstchild(disco));

    jpacket_reset(m->packet);
    js_deliver(m->si,m->packet);

    return M_HANDLED;
}


#define NS_DISCO_ITEMS "http://jabber.org/protocol/disco#items"

mreturn mod_disco_server_items(mapi m, void *arg)
{
  xmlnode browse, query, cur;
  
  if((xmlnode_get_attrib(m->packet->x,"node")) != NULL) return M_PASS;

  /* config get */        
  if((browse = js_config(m->si,"browse")) == NULL)
	return M_PASS;
  
  log_debug("mod_disco","handling disco#items query");

  /* build the result IQ */
  query = xmlnode_insert_tag(jutil_iqresult(m->packet->x),"query");
  xmlnode_put_attrib(query,"xmlns",NS_DISCO_ITEMS);

  /* copy in the configured services */
  for(cur = xmlnode_get_firstchild(browse);
	  cur != NULL;
	  cur = xmlnode_get_nextsibling(cur)){
	xmlnode item;
	const char *jid,*name;

	jid = xmlnode_get_attrib(cur,"jid");
	if (!jid) continue;

	item = xmlnode_insert_tag(query,"item");
	xmlnode_put_attrib(item,"jid",jid);
	name = xmlnode_get_attrib(cur,"name");
	if (name) xmlnode_put_attrib(item,"name",name);
  }

  jpacket_reset(m->packet);
  js_deliver(m->si,m->packet);
  
  return M_HANDLED;
}

mreturn mod_disco_server(mapi m, void *arg)
{
    if (m->packet->type != JPACKET_IQ) return M_IGNORE;
    if (jpacket_subtype(m->packet) != JPACKET__GET) return M_PASS;
	if (m->packet->to->resource != NULL) return M_PASS;
    if (NSCHECK(m->packet->iq,NS_DISCO_ITEMS)) return mod_disco_server_items(m,arg);
    if (NSCHECK(m->packet->iq,NS_DISCO_INFO)) return mod_disco_server_info(m,arg);
    return M_PASS;
}

/* mod disco based on JEP-030 */
void mod_disco(jsmi si)
{
    js_mapi_register(si,e_SERVER,mod_disco_server,NULL);
}

