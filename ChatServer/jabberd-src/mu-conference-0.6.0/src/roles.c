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

taffil affiliation_level(cnr room, jid user)
{
    log_debug(NAME, "[%s] Affiliation Check", FZONE);

    if(is_owner(room, user))
    {
        return TAFFIL_OWNER;
    }
    else if(is_admin(room, user))
    {
        return TAFFIL_ADMIN;
    }           
    else if(is_member(room, user))
    {           
        return TAFFIL_MEMBER;
    }   
    else if(is_outcast(room, user))
    {       
        return TAFFIL_OUTCAST;
    }   

    return TAFFIL_NONE;
}           

trole role_level(cnr room, jid user)
{
    log_debug(NAME, "[%s] Role Check", FZONE);

    if(is_leaving(room, user))
    {
	return TROLE_NONE;
    }
    else if(is_moderator(room, user))
    {   
        return TROLE_MODERATOR;
    }       
    else if(is_participant(room, user))
    {
        return TROLE_PARTICIPANT;
    }   
    else if(is_visitor(room, user))
    {       
        return TROLE_VISITOR;
    }   
	        
    return TROLE_NONE;
}   

int add_affiliate(GHashTable *hash, jid userid, xmlnode details)
{
    xmlnode old;
    xmlnode store;
    xmlnode node;
    char *key;
    char ujid[256];

    if(userid == NULL)
    {
	return -1;
    }

    snprintf(ujid, 256, "%s@%s", userid->user, userid->server);
    key = j_strdup(ujid);
    old = g_hash_table_lookup(hash, key);

    /* User not previously registered. Set up */
    if(old == NULL)
    {
	store = xmlnode_new_tag("users");
    }
    else
    {
        store = xmlnode_dup(old);
        node = xmlnode_get_tag(store, spools(xmlnode_pool(store), "item?jid=", jid_full(userid), xmlnode_pool(store))); 

        /* If already in the node, ignore */
        if(node != NULL)
	{
	    xmlnode_free(store);
            free(key);
	    return 0;
	}
    }

    if(details != NULL)
    {
        xmlnode_free(store);
	store = xmlnode_dup(details);
    }
    else if(userid->resource != NULL)
    {
        node = xmlnode_new_tag("item");

	xmlnode_put_attrib(node, "jid", jid_full(userid));
	
	xmlnode_insert_node(store, node);
        xmlnode_free(node);
    }

    g_hash_table_insert(hash, key, store);

    return 1;
}

int remove_affiliate(GHashTable *hash, jid userid)
{
    xmlnode old;
    xmlnode store;
    xmlnode node;
    char *key;
    char ujid[256];

    if(userid == NULL)
    {
	return -1;
    }

    snprintf(ujid, 256, "%s@%s", userid->user, userid->server);
    key = j_strdup(ujid);
    old = g_hash_table_lookup(hash, key);
    free(key);

    if(old == NULL)
	return 1;

    store = xmlnode_dup(old);

    node = xmlnode_get_tag(store, spools(xmlnode_pool(store), "item?jid=", jid_full(userid), xmlnode_pool(store))); 
    
    if(node == NULL)
    {
        xmlnode_free(store);
        return 1;
    }

    xmlnode_hide(node);

    key = j_strdup(ujid);
    g_hash_table_insert(hash, key, store);

    return 1;
}

xmlnode revoke_affiliate(cnr room, GHashTable *hash, jid userid)
{
    cnu user;
    jid jabberid;
    xmlnode store;
    xmlnode current;
    char *userjid;
    char *key;
    char ujid[256];

    if(userid == NULL)
    {
	return NULL;
    }

    snprintf(ujid, 256, "%s@%s", userid->user, userid->server);
    key = j_strdup(ujid);
    store = g_hash_table_lookup(hash, key);
    free(key);

    if(store == NULL)
	return NULL;



    current = xmlnode_get_tag(store, "item");

    if(current != NULL)
    {
        for(current = xmlnode_get_firstchild(store); current != NULL; current = xmlnode_get_nextsibling(current))    
        {
	    userjid = xmlnode_get_attrib(current, "jid");

	    if(userjid != NULL)
	    {
	        jabberid = jid_new(xmlnode_pool(store), userjid);
	        user = g_hash_table_lookup(room->remote, jid_full(jabberid)); 

	        if(user != NULL)
	        {
		    update_presence(user);
	        }
	    }
	}
    }

    key = j_strdup(ujid);
    g_hash_table_remove(hash, key);
    free(key);

    return NULL;
}

void change_affiliate(char *affiliation, cnu sender, jid user, char *reason, jid by)
{
    cnr room;
    cnu from;
    taffil current;
    xmlnode data, invite, x;
    char ujid[256];

    if(affiliation == NULL || sender == NULL || user == NULL)
    {
	log_warn(NAME, "[%s] Missing attributes", FZONE);
	return;
    }

    snprintf(ujid, 256, "%s@%s", user->user, user->server);

    room = sender->room;
    current = affiliation_level(room, user);

    /* if not changing affiliation, just return */
    if(j_strcmp(current.msg, affiliation) == 0)
    {
	log_debug(NAME, "[%s] Affiliation not changing - %s == %s ", FZONE, affiliation, current.msg);
	return;
    }

    /* Clear any old affiliation */
    if(j_strcmp(affiliation, "owner") != 0)
    {
	revoke_affiliate(room, room->owner, user);
    }

    if(j_strcmp(affiliation, "admin") != 0)
    {
	revoke_affiliate(room, room->admin, user);
    }

    if(j_strcmp(affiliation, "member") != 0)
    {
	revoke_affiliate(room, room->member, user);
    }

    if(j_strcmp(affiliation, "outcast") != 0)
    {
	revoke_affiliate(room, room->outcast, user);
    }

    /* Update to new affiliation */
    if(j_strcmp(affiliation, "owner") == 0)
    {
	add_affiliate(room->owner, user, NULL);
    }
    else if(j_strcmp(affiliation, "admin") == 0)
    {
	add_affiliate(room->admin, user, NULL);
    }
    else if(j_strcmp(affiliation, "member") == 0)
    {
	add_affiliate(room->member, user, NULL);

	if(room->invitation == 1 && !in_room(room, user))
	{
            x = xmlnode_new_tag("x");
	    xmlnode_put_attrib(x, "xmlns", NS_MUC_USER);
	    invite = xmlnode_insert_tag(x, "invite");
	    xmlnode_put_attrib(invite, "to", ujid);
	    xmlnode_insert_cdata(xmlnode_insert_tag(invite, "reason"), "Added as a member", -1);
	    con_room_send_invite(sender, x);
	}
    }
    else if(j_strcmp(affiliation, "outcast") == 0)
    {
	data = xmlnode_new_tag("reason");
	from = g_hash_table_lookup(room->remote, jid_full(jid_fix(by)));

	if(reason == NULL)
	{
	    xmlnode_insert_cdata(data, "None given", -1);
	}
	else
	{
	    xmlnode_insert_cdata(data, reason, -1);
	}

	if(from != NULL)
        {
	    xmlnode_put_attrib(data, "actor", jid_full(jid_user(jid_fix(from->realid))));
	    xmlnode_put_attrib(data, "nick", xmlnode_get_data(from->nick));
	}
	else
	{
	    xmlnode_put_attrib(data, "actor", jid_full(jid_fix(by)));
	}

	add_affiliate(room->outcast, user, data);
    }

    if(room->persistent == 1)
        xdb_room_lists_set(room);

    return;
}

void add_role(GHashTable *hash, cnu user)
{
    char *key;
    key = j_strdup(jid_full(user->realid));
    log_debug(NAME, "[%s] About to add role [%s]", FZONE, key);
    g_hash_table_insert(hash, key, (void*)user);
}

void revoke_role(GHashTable *hash, cnu user)
{
    char *key;
    key = j_strdup(jid_full(user->realid));
    log_debug(NAME, "[%s] About to revoke role [%s]", FZONE, key);
    g_hash_table_remove(hash, key);
    free(key);
}

void change_role(char *role, cnu sender, jid user, char *reason)
{
    char *key, *result;
    cnr room;
    cnu data;
    jid userid;
    trole current;
    xmlnode node, userlist;

    log_debug(NAME, "[%s] Role change request - %s to %s", FZONE, jid_full(user), role);

    if(role == NULL || user == NULL)
    {
	log_debug(NAME, "[%s] Missing attributes", FZONE);
	return;
    }

    room = sender->room;

    key = j_strdup(jid_full(user));
    data = g_hash_table_lookup(room->remote, key);
    free(key);

    if(data == NULL)
    {
	if(user->resource == NULL)
	{
	    userlist = get_roster(room, user);

	    if(userlist != NULL)
	    {
		for(node = xmlnode_get_firstchild(userlist); node != NULL; node = xmlnode_get_nextsibling(node))
		{
		    result = xmlnode_get_attrib(node, "jid");
		    userid = jid_new(xmlnode_pool(node), result);
		    change_role(role, sender, userid, reason);
		}
	    }
	    else
	    {
	        log_debug(NAME, "[%s] User not found", FZONE);
	    }
	    return;
	}
	else
	{
	    log_debug(NAME, "[%s] User not found", FZONE);
	    return;
	}
    }

    current = role_level(room, user);

    /* if not changing role, just return */
    if(j_strcmp(current.msg, role) == 0)
    {
	log_debug(NAME, "[%s] Role not changing", FZONE);
        update_presence(data);
	return;
    }

    /* Clear any old roles */
    if(j_strcmp(role, "moderator") != 0)
    {
	revoke_role(room->moderator, data);
    }

    if(j_strcmp(role, "participant") != 0)
    {
	revoke_role(room->participant, data);
    }

    /* Update to new role */
    if(j_strcmp(role, "moderator") == 0)
    {
	add_role(room->moderator, data);
	log_debug(NAME, "[%s] Added Moderator", FZONE);
    }
    else if(j_strcmp(role, "participant") == 0)
    {
	add_role(room->participant, data);
	log_debug(NAME, "[%s] Added Participant", FZONE);
    }
    else if(j_strcmp(role, "none") == 0)
    {
	if(reason == NULL)
	{
	    reason = pstrdup(user->p, "None given");
	}
	log_debug(NAME, "[%s] Call kick routine with reason %s", FZONE, reason);

	data->leaving = 1;
	adm_user_kick(sender, data, reason);
	return;
    }

    update_presence(data);
    return;
}
