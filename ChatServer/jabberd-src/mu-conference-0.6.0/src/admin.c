/*
 * MU-Conference - Multi-User Conference Service
 * Copyright (c) 2002 David Sutton
 *
 *
 * This program is free software; you can redistribute it and/or drvify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#include "conference.h"

void con_get_role_list(gpointer key, gpointer data, gpointer arg)
{
    xmlnode node;
    xmlnode result;
    char *jabberid;
    taffil affiliation;
    trole role;
    jid userid;
    cnr room;

    result = (xmlnode)arg;

    if(result == NULL)
    {
        log_warn(NAME, "[%s] Aborting: NULL result - <%s>", FZONE, key);
        return;
    }

    room = (cnr)xmlnode_get_vattrib(result ,"cnr");

    if(room == NULL)
    {
        log_warn(NAME, "[%s] Aborting: NULL room - <%s>", FZONE, key);
        return;
    }

    node = xmlnode_new_tag("item");
    jabberid = pstrdup(xmlnode_pool(node), key);
    userid = jid_new(xmlnode_pool(node), jabberid);

    xmlnode_put_attrib(node, "jid", jabberid);

    affiliation = affiliation_level(room, userid);
    role = role_level(room, userid);

    xmlnode_put_attrib(node, "role", role.msg);
    xmlnode_put_attrib(node, "affiliation", affiliation.msg);

    xmlnode_insert_node(result, node);
    xmlnode_free(node);
}

void con_get_affiliate_list(gpointer key, gpointer data, gpointer arg)
{
    xmlnode node;
    cnr room;
    taffil affiliation;
    jid userid;
    char *jabberid;
    char *actor;
    char *reason;


    xmlnode result = (xmlnode)arg;
    xmlnode item = (xmlnode)data; 

    if(result == NULL || item == NULL)
    {
        log_warn(NAME, "[%s] Aborting: NULL attribute(s) - <%s>", FZONE, key);
        return;
    }

    actor = xmlnode_get_attrib(item, "actor");
    reason = xmlnode_get_data(item);
    room = (cnr)xmlnode_get_vattrib(result ,"cnr");

    node = xmlnode_new_tag("item");

    jabberid = pstrdup(xmlnode_pool(node), key);
    userid = jid_new(xmlnode_pool(node), jabberid);

    xmlnode_put_attrib(node, "jid", jabberid);

    affiliation = affiliation_level(room, userid);

    xmlnode_put_attrib(node, "affiliation", affiliation.msg);

    if(reason != NULL)
        xmlnode_insert_cdata(xmlnode_insert_tag(node, "reason"), reason, -1);

    if(actor != NULL)
        xmlnode_insert_cdata(xmlnode_insert_tag(node, "actor"), actor, -1);

    xmlnode_insert_node(result, node);
    xmlnode_free(node);
}


/* Generate x:data form for configuring lists */
xmlnode con_gen_list(cnr room, char *namespace, char *type)
{
    xmlnode result;

    result = xmlnode_new_tag("query");
    xmlnode_put_attrib(result,"xmlns", namespace);

    if (room == NULL)
    {
        log_warn(NAME, "[%s] NULL room attribute", FZONE);
        return result;
    }

    xmlnode_put_vattrib(result, "cnr", (void*)room);

    if(j_strcmp(type, "owner") == 0)
	g_hash_table_foreach(room->owner, con_get_affiliate_list, (void*)result);
    else if(j_strcmp(type, "admin") == 0)
	g_hash_table_foreach(room->admin, con_get_affiliate_list, (void*)result);
    else if(j_strcmp(type, "moderator") == 0)
	g_hash_table_foreach(room->moderator, con_get_role_list, (void*)result);
    else if(j_strcmp(type, "member") == 0)
    {
        log_debug(NAME, "[%s] member list size [%d]", FZONE, g_hash_table_size(room->member));
	g_hash_table_foreach(room->member, con_get_affiliate_list, (void*)result);
    }
    else if(j_strcmp(type, "participant") == 0)
	g_hash_table_foreach(room->participant, con_get_role_list, (void*)result);
    else if(j_strcmp(type, "outcast") == 0)
	g_hash_table_foreach(room->outcast, con_get_affiliate_list, (void*)result);

    xmlnode_hide_attrib(result, "cnr");

    return result;
}

void adm_user_kick(cnu user, cnu target, char *reason)
{
    cnr room;
    xmlnode data;
    xmlnode pres;
    char *status;

    if(user == NULL || target == NULL || reason == NULL)
    {
        log_warn(NAME, "[%s] Aborting: NULL attribute found", FZONE);
	return;
    }

    room = target->room;

    data = xmlnode_new_tag("reason");

    if(is_outcast(room, target->realid))
        status = pstrdup(xmlnode_pool(data), STATUS_MUC_BANNED);
    else
	status = pstrdup(xmlnode_pool(data), STATUS_MUC_KICKED);

    xmlnode_put_attrib(data, "status", status);
    xmlnode_put_attrib(data, "actor", jid_full(jid_user(user->realid)));
    xmlnode_insert_cdata(data, reason, -1);

    pres = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(target->realid), NULL);
    target->presence = pres;
    log_debug(NAME, "[%s] Kick/Ban requested. Status code=%s", FZONE, status);

    con_send_alert(target, reason, NULL, status);
    con_user_zap(target, data);

    return;
}

void con_parse_item(cnu sender, jpacket jp)
{
    xmlnode current;
    xmlnode result;
    xmlnode node;
    jid target;
    jid from;
    char *xmlns;
    char *role;
    char *jabberid;
    char *nick;
    char *reason;
    char *affiliation;
    cnu user;
    cnr room;

    int error = 0;

    if(sender == NULL)
    {
	log_warn(NAME, "[%s] Aborting - NULL sender", FZONE);
	return;
    }

    user = NULL;
    from = sender->realid;
    room = sender->room;
    node = xmlnode_get_tag(jp->x, "query");
    xmlns = xmlnode_get_attrib(node, "xmlns");

    /* Check for configuration request */
    if(j_strcmp(xmlns, NS_MUC_OWNER) == 0 && xmlnode_get_tag(node, "item") == NULL)
    {
	if(is_owner(room, from))
	{
	    user = g_hash_table_lookup(room->remote, jid_full(from));
	    if(user)
	    {
                xdata_room_config(room, user, room->locked, jp->x);
	        xmlnode_free(jp->x);
	    }
	    else
	    {
	        jutil_error(jp->x, TERROR_BAD);
	        deliver(dpacket_new(jp->x), NULL);
	    }
	    return;
        }
        else
        {
	    jutil_error(jp->x, TERROR_NOTALLOWED);
	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}
    }
    
    /* Parse request for errors */
    for(current = xmlnode_get_firstchild(node); current != NULL; current = xmlnode_get_nextsibling(current))
    {
	/* Extract data */
	jabberid = xmlnode_get_attrib(current, "jid");
	nick = xmlnode_get_attrib(current, "nick");
	role = xmlnode_get_attrib(current, "role");
	affiliation = xmlnode_get_attrib(current, "affiliation");
	reason = xmlnode_get_tag_data(current, "reason");
    
	if(jabberid == NULL && nick == NULL && role == NULL && affiliation == NULL)
	{
	    error = 1;
	    log_debug(NAME, "[%s] Skipping - Badly formed request (%s)", FZONE, xmlnode2str(current));
	    insert_item_error(current, "400", "Badly formed request");
	    continue;
	}

	if(jpacket_subtype(jp) == JPACKET__GET)
        {
	    if(jabberid == NULL && nick == NULL)
	    {
		if(!is_admin(room, from))
		{
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - Insufficent level to request admin list", FZONE);
		    insert_item_error(jp->x, "403", "Forbidden list retrieval");
	            continue;
		}

		if(role != NULL && affiliation != NULL)
	        {
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - Badly formed request (%s)", FZONE, xmlnode2str(current));
		    insert_item_error(current, "400", "Badly formatted list request");
	            continue;
	        }

		if(j_strcmp(affiliation, "admin") != 0 && j_strcmp(role, "participant") != 0 && j_strcmp(affiliation, "member") != 0 && j_strcmp(role, "moderator") != 0 && j_strcmp(affiliation, "outcast") != 0 && j_strcmp(affiliation, "owner") != 0)
	        {
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - No such list (%s)", FZONE, xmlnode2str(current));
		    insert_item_error(current, "400", "No such list");
	            continue;
	        }

	        if(j_strcmp(affiliation, "admin") == 0 || j_strcmp(affiliation, "owner") == 0)
	        {
		    if(j_strcmp(xmlns, NS_MUC_OWNER) != 0)
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - Badly formed namespace (%s)", FZONE, xmlnode2str(current));
			insert_item_error(current, "400", "Invalid Namespace");
	                continue;
		    }

		    if(!is_owner(room, from))
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - Insufficent level to request %s list", FZONE, affiliation);
			insert_item_error(jp->x, "403", "Forbidden list retrieval");
	                continue;
		    }
	        }
		else if(j_strcmp(xmlns, NS_MUC_ADMIN) != 0)
	        {
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - Badly formed namespace (%s)", FZONE, xmlnode2str(current));
		    insert_item_error(current, "400", "Invalid Namespace");
	            continue;
	        }
	    }
	    else
	    {
	        error = 1;
	        log_debug(NAME, "[%s] Skipping - Badly formed request (%s)", FZONE, xmlnode2str(current));
		insert_item_error(current, "400", "Badly formed request - extra attributes found");
                continue;
            }
	}
	else if(jpacket_subtype(jp) == JPACKET__SET)
	{
	    if(role == NULL && affiliation == NULL)
	    {
	        error = 1;
	        log_debug(NAME, "[%s] Skipping - no role or affiliation given (%s)", FZONE, xmlnode2str(current));
		insert_item_error(current, "400", "Badly formed request - no role or affiliation attribute");
	        continue;
	    }

	    if(jabberid == NULL)
	    {
	        user = con_room_usernick(room, nick);

		if(user)
		{
		    jabberid = jid_full(user->realid);
		}
		else
	        {
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - can't find jid (%s)", FZONE, xmlnode2str(current));
		    insert_item_error(current, "400", "Nick not present in room");
		    continue;
	        }
	    }

	    target = jid_new(jp->p, jabberid);

	    if(target->user == NULL && role != NULL)
	    {
	        error = 1;
	        log_debug(NAME, "[%s] Skipping - Bad jid (%s)", FZONE, jabberid);
		insert_item_error(current, "400", "Badly formed JID");
	        continue;
            }

	    if(role != NULL && affiliation != NULL)
	    {
		/* Check requesting user has minimum affiliation level */
	        error = 1;
	        log_debug(NAME, "[%s] Skipping - Attempting to change role and affiliation (%s)", FZONE, jabberid);
		insert_item_error(current, "400", "Bad request - trying to change role and affiliation");
	        continue;
            }

	    /* Affiliation changes */
	    if(affiliation != NULL)
	    {
		if(!is_admin(room, from))
	        { 
		    /* Check requesting user has minimum affiliation level */
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - affiliation role requested by non-admin(%s)", FZONE, jabberid);
		    insert_item_error(current, "403", "Forbidden - No affiliation requested by non-admin");
	            continue;
                }
                else if(!is_owner(room, from) && is_admin(room, target))
	        {  
		    /* Stop admins altering other admins */
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - affiliation role requested by non-admin(%s)", FZONE, jabberid);
	            insert_item_error(current, "403", "Forbidden - No affiliation request between admins");
	            continue;
                }
                else if(j_strcmp(affiliation, "owner") == 0)
	        {
		    if(!is_owner(room, from))
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - affiliation role requested by non-owner(%s)", FZONE, jabberid);
		        insert_item_error(current, "403", "Forbidden - Owner requested");
	                continue;
		    }
                }
		else if(j_strcmp(affiliation, "admin") == 0)
	        { 
		    if(!is_owner(room, from))
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - affiliation role requested by non-owner(%s)", FZONE, jabberid);
		        insert_item_error(current, "403", "Forbidden - Admin requested");
	                continue;
		    }
                }
		else if(j_strcmp(affiliation, "outcast") == 0)
	        { 
		    if(is_admin(room, target))
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - affiliation role requested by non-owner(%s)", FZONE, jabberid);
		        insert_item_error(current, "403", "Forbidden - Admin requested");
	                continue;
		    }
                }
		else if(j_strcmp(affiliation, "member") != 0 && j_strcmp(affiliation, "none") != 0)
		{
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - affiliation unknown(%s/%s)", FZONE, jabberid, affiliation);
		    insert_item_error(current, "400", "Unknown affiliation");
		    continue;
		}
	    }

	    /* role changes */
	    if(role != NULL)
	    {

	        if(!is_admin(room, from) && !is_moderator(room, from))
		{
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - Forbidden role change (%s)", FZONE, jabberid);
		    insert_item_error(current, "403", "Forbidden role change request by non-admin");
		    continue;
		}
		else if(j_strcmp(role, "moderator") == 0)
		{
		    if(!is_admin(room, from))
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - Forbidden moderater request (%s)", FZONE, jabberid);
		        insert_item_error(current, "403", "Forbidden moderator request by non-admin");
			continue;
		    }
		}
		else if(j_strcmp(role, "none") == 0)
		{
		    if(is_admin(room, target) || is_moderator(room, target))
		    {
	                error = 1;
	                log_debug(NAME, "[%s] Skipping - Forbidden kick request (%s)", FZONE, jabberid);
		        insert_item_error(current, "403", "Forbidden Kick request against admin");
			continue;
		    }
		}
		else if(j_strcmp(role, "participant") != 0 && j_strcmp(role, "visitor") != 0)
		{
	            error = 1;
	            log_debug(NAME, "[%s] Skipping - role unknown(%s)", FZONE, jabberid);
		    insert_item_error(current, "400", "Unknown role");
		    continue;
		}
            }
	} 

	log_debug(NAME, "[%s] Ok (%s)", FZONE, xmlnode2str(current));
    }

    /* If theres an error, return */
    if(error == 1)
    {
        jutil_iqresult(jp->x);
        xmlnode_put_attrib(jp->x, "type", "error");

	xmlnode_insert_node(jp->x, node);

        deliver(dpacket_new(jp->x), NULL);
	return;
    }

    /* Now process the checked results */
    result = xmlnode_new_tag("query");
    xmlnode_put_attrib(result, "xmlns", xmlns);

    for(current = xmlnode_get_firstchild(node); current != NULL; current = xmlnode_get_nextsibling(current))
    {
	jabberid = xmlnode_get_attrib(current, "jid");
	nick = xmlnode_get_attrib(current, "nick");
	role = xmlnode_get_attrib(current, "role");
	affiliation = xmlnode_get_attrib(current, "affiliation");
	reason = xmlnode_get_tag_data(current, "reason");


	if(jpacket_subtype(jp) == JPACKET__GET)
        {
	    if(role != NULL)
	        result = con_gen_list(room, xmlns, role);
	    else
	        result = con_gen_list(room, xmlns, affiliation);
	    break;
	}
	else
	{
	    /* Find jabberid for this user */
	    if(jabberid == NULL)
	    {
	        user = con_room_usernick(room, nick);
                jabberid = jid_full(user->realid);
	    }
	    else if(user == NULL)
	    {
	        user = g_hash_table_lookup(room->remote, jabberid);
	    }

	    /* Convert jabberid into a jid struct */	
	    target = jid_new(jp->p, jabberid);

	    if(role == NULL && affiliation != NULL)
	    {
	        log_debug(NAME, "[%s] Requesting affiliation change for %s to (%s)", FZONE, jabberid, affiliation);

		change_affiliate(affiliation, sender, target, reason, from);

		if(target->user != NULL)
		{
		    if(j_strcmp(affiliation, "owner") == 0)
		    {
		        change_role("moderator", sender, target, reason);
	            }
		    else if(j_strcmp(affiliation, "admin") == 0)
		    {
		        change_role("moderator", sender, target, reason);
	            }
		    else if(j_strcmp(affiliation, "outcast") == 0)
		    {
		        change_role("none", sender, target, reason);
	            }
		    else
		    {
		        change_role("participant", sender, target, reason);
	            }
		}
	    }
	    else if(role != NULL && affiliation == NULL)
	    {
	        log_debug(NAME, "[%s] Requesting role change for %s to (%s)", FZONE, jabberid, role);
		change_role(role, sender, target, reason);
	    }
	    else
	    {
	        log_debug(NAME, "[%s] Request: role %s, affiliation %s, for %s", FZONE, role, affiliation, jabberid);
	    }
	}
    }

    jutil_iqresult(jp->x);

    if(result)
    {
        xmlnode_insert_node(jp->x, result);
        xmlnode_free(result);
    }

    deliver(dpacket_new(jp->x), NULL);
    return;
}

