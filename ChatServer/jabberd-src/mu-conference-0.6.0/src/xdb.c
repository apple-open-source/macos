/*
 * MU-Conference - Multi-User Conference Service
 * Copyright (c) 2002 David Sutton
 * Portions (c) Copyright 2005 Apple Computer, Inc.
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

int xdb_room_config(cnr room)
{
    char *roomid;
    char *host;
    char temp[10];
    cni master;
    int status;
    jid store;
    xmlnode node;
    xmlnode element;

    if(room == NULL)
    {
	log_error(NAME, "[%s] Aborting: NULL room result", FZONE);
	return -1;
    }

    master = room->master;
    roomid = jid_full(room->id);
    host = room->id->server;

    log_debug(NAME, "[%s] Writing Room config.. - <%s>", FZONE, roomid);

    node = xmlnode_new_tag("room");
    store = jid_new(xmlnode_pool(node), spools(xmlnode_pool(node), shahash(roomid), "@", host, xmlnode_pool(node)));

    xmlnode_insert_cdata(xmlnode_insert_tag(node, "name"), room->name, -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "secret"), room->secret, -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "description"), room->description, -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "subject"), xmlnode_get_attrib(room->topic,"subject"), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "creator"), jid_full(room->creator), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "private"), itoa(room->private, temp), -1);

    element = xmlnode_insert_tag(node, "notice");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "leave"), room->note_leave, -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "join"), room->note_join, -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "rename"), room->note_rename, -1);

    xmlnode_insert_cdata(xmlnode_insert_tag(node, "public"), itoa(room->public, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "subjectlock"), itoa(room->subjectlock, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "maxusers"), itoa(room->maxusers, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "persistent"), itoa(room->persistent, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "moderated"), itoa(room->moderated, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "defaulttype"), itoa(room->defaulttype, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "privmsg"), itoa(room->privmsg, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "invitation"), itoa(room->invitation, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "invites"), itoa(room->invites, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "legacy"), itoa(room->legacy, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "visible"), itoa(room->visible, temp), -1);
    xmlnode_insert_cdata(xmlnode_insert_tag(node, "logformat"), itoa(room->logformat, temp), -1);

    if(room->logfile)
	xmlnode_insert_cdata(xmlnode_insert_tag(node, "logging"), "1", -1);
    else
	xmlnode_insert_cdata(xmlnode_insert_tag(node, "logging"), "0", -1);

    status = xdb_set(master->xdbc, store, "muc:room:config", node);
    
    xmlnode_free(node);

    return status;
}

void _xdb_put_list(gpointer key, gpointer data, gpointer arg)
{
    xmlnode result = (xmlnode)arg;
    xmlnode item;
    jid id;
    char *jabberid;

    jabberid = pstrdup(xmlnode_pool(result), key);

    /* cnu is only available if resource defined in jabber id */
    id = jid_new(xmlnode_pool(result), jabberid);

    if(id == NULL)
    {
        log_warn(NAME, "[%s] Somethings not right here - <%s>", FZONE, jabberid);
	return;
    }

    item = xmlnode_new_tag("item");
    xmlnode_put_attrib(item, "jid", jabberid);
    xmlnode_insert_node(result, item);
    xmlnode_free(item);
}

void _xdb_put_outcast_list(gpointer key, gpointer data, gpointer arg)
{
    xmlnode result = (xmlnode)arg;
    xmlnode info = (xmlnode)data;
    xmlnode item;
    jid id;
    char *jabberid;

    jabberid = pstrdup(xmlnode_pool(result), key);

    /* cnu is only available if resource defined in jabber id */
    id = jid_new(xmlnode_pool(result), jabberid);

    if(id == NULL)
    {
        log_warn(NAME, "[%s] Somethings not right here - <%s>", FZONE, jabberid);
	return;
    }

    item = xmlnode_new_tag("item");
    xmlnode_put_attrib(item, "jid", jabberid);
    xmlnode_insert_node(item, info);
    xmlnode_insert_node(result, item);
    xmlnode_free(item);
}


int xdb_room_lists_set(cnr room)
{
    char *roomid;
    char *host;
    cni master;
    jid store;
    xmlnode node;
    pool p;

    if(room == NULL)
    {
	return -1;
    }

    p = pool_new();
    master = room->master;
    roomid = jid_full(room->id);
    host = room->id->server;

    log_debug(NAME, "[%s] Writing Room lists.. - <%s>", FZONE, roomid);

    store = jid_new(p, spools(p, shahash(roomid), "@", host, p));

    node = xmlnode_new_tag("list");
    g_hash_table_foreach(room->owner, _xdb_put_list, (void*)node);
    xdb_set(master->xdbc, store, "muc:list:owner", node);
    xmlnode_free(node);
    
    node = xmlnode_new_tag("list");
    g_hash_table_foreach(room->admin, _xdb_put_list, (void*)node);
    xdb_set(master->xdbc, store, "muc:list:admin", node);
    xmlnode_free(node);
    
    node = xmlnode_new_tag("list");
    g_hash_table_foreach(room->member, _xdb_put_list, (void*)node);
    xdb_set(master->xdbc, store, "muc:list:member", node);
    xmlnode_free(node);

    node = xmlnode_new_tag("list");
    g_hash_table_foreach(room->outcast, _xdb_put_outcast_list, (void*)node);
    xdb_set(master->xdbc, store, "muc:list:outcast", node);
    xmlnode_free(node);

    pool_free(p);
    return 1;
}

void xdb_room_set(cnr room)
{
    pool p;
    char *host;
    jid fulljid;
    jid roomid;
    cni master;
    xmlnode node;
    xmlnode item;

    if(room == NULL)
    {
        return;
    }

    p = pool_new();
    master = room->master;
    host = room->id->server;

    fulljid = jid_new(p, spools(p, "rooms@", host, p));
    roomid = jid_new(p, spools(p, shahash(jid_full(room->id)),"@", host, p));

    node = xdb_get(master->xdbc, fulljid, "muc:room:list");

    if(node == NULL)
    {
        node = xmlnode_new_tag("registered");
    }

    item = xmlnode_get_tag(node, spools(p, "?jid=", jid_full(jid_fix(roomid)), p));

    if(item == NULL)
    {
	item = xmlnode_insert_tag(node, "item");
	xmlnode_put_attrib(item, "name", jid_full(room->id));
	xmlnode_put_attrib(item, "jid", jid_full(jid_fix(roomid)));
	xdb_set(master->xdbc, fulljid, "muc:room:list", node);
    }

    xdb_room_config(room);
    xdb_room_lists_set(room);

    xmlnode_free(node);
    pool_free(p);

    return;
}

void _xdb_add_list(GHashTable *hash, xmlnode node)
{
    char *user;
    xmlnode current;
    jid userid;

    if(node == NULL)
    {
	return;
    }

    for(current = xmlnode_get_firstchild(node); current != NULL; current = xmlnode_get_nextsibling(current))
    {
	user = xmlnode_get_attrib(current, "jid");

	if (user)
	{
	    userid = jid_new(xmlnode_pool(node), user);
            add_affiliate(hash, userid, xmlnode_get_tag(current, "reason"));
	}
    }

    xmlnode_free(current);
    return;
}

int xdb_room_lists_get(cnr room)
{
    char *roomid;
    char *host;
    cni master;
    jid store;
    xmlnode node;
    pool p;

    if(room == NULL)
    {
	return -1;
    }

    log_debug(NAME, "[%s] asked to restore rooms lists for %s ", FZONE, jid_full(room->id));

    p = pool_new();
    master = room->master;
    roomid = jid_full(room->id);
    host = room->id->server;

    store = jid_new(p, spools(p, shahash(roomid), "@", host, p));

    node = xdb_get(master->xdbc, store, "muc:list:owner");
    _xdb_add_list(room->owner, node);
    xmlnode_free(node);

    node = xdb_get(master->xdbc, store, "muc:list:admin");
    _xdb_add_list(room->admin, node);
    xmlnode_free(node);

    node = xdb_get(master->xdbc, store, "muc:list:member");
    _xdb_add_list(room->member, node);
    xmlnode_free(node);

    node = xdb_get(master->xdbc, store, "muc:list:outcast");
    _xdb_add_list(room->outcast, node);
    xmlnode_free(node);

    pool_free(p);
    return 1;
}

void xdb_rooms_get(cni master)
{
    char *file, *roomid, *subject;
    cnr room;
    jid jidroom;
    jid fulljid;
    xmlnode node = NULL;
    xmlnode current = NULL;
    xmlnode result = NULL;
    pool p;

    if(master == NULL)
    {
	return;
    }

    p = pool_new();

    fulljid = jid_new(p, spools(p, "rooms@", master->i->id, p));

    log_debug(NAME, "[%s] asked to get rooms from xdb ", FZONE);

    /* Get master room list */
    node = xdb_get(master->xdbc, fulljid, "muc:room:list");

    if(node != NULL)
    {
	xmlnode_free(current);

	for(current = xmlnode_get_firstchild(node); current != NULL; current = xmlnode_get_nextsibling(current))
        {
	    if(xmlnode_get_attrib(current, "name") == 0)
	    {
                log_debug(NAME, "[%s] skipping .. no name", FZONE);
		continue;
	    }

            roomid = xmlnode_get_attrib(current, "name");
            log_debug(NAME, "[%s] asked to get room %s from xdb ", FZONE, roomid);


	    file = xmlnode_get_attrib(current, "jid");

	    if(roomid == NULL || file == NULL)
	    {
                log_debug(NAME, "[%s] skipping .. no room/file", FZONE);
		continue;
	    }


	    fulljid = jid_new(xmlnode_pool(node), spools(xmlnode_pool(node), file, xmlnode_pool(node)));
	    jidroom = jid_new(xmlnode_pool(node), spools(xmlnode_pool(node), roomid, xmlnode_pool(node)));

	    result = xdb_get(master->xdbc, fulljid, "muc:room:config");

	    if(result == NULL)
	    {
                log_debug(NAME, "[%s] skipping .. no room config", FZONE);
		continue;
	    }

	    room = con_room_new(master, jidroom, NULL, xmlnode_get_tag_data(result,"name"), xmlnode_get_tag_data(result, "secret"), j_atoi(xmlnode_get_tag_data(result, "private"), 0), 0, 0);

	    room->subjectlock = j_atoi(xmlnode_get_tag_data(result, "subjectlock"), 0);
	    room->maxusers = j_atoi(xmlnode_get_tag_data(result, "maxusers"), 0);
	    room->moderated = j_atoi(xmlnode_get_tag_data(result, "moderated"), 0);
	    room->defaulttype = j_atoi(xmlnode_get_tag_data(result, "defaulttype"), 0);
	    room->privmsg = j_atoi(xmlnode_get_tag_data(result, "privmsg"), 0);
	    room->invitation = j_atoi(xmlnode_get_tag_data(result, "invitation"), 0);
	    room->invites = j_atoi(xmlnode_get_tag_data(result, "invites"), 0);
	    room->legacy = j_atoi(xmlnode_get_tag_data(result, "legacy"), 1);
	    room->public = j_atoi(xmlnode_get_tag_data(result, "public"), room->master->public);
	    room->visible = j_atoi(xmlnode_get_tag_data(result, "visible"), 0);
	    /* correct spelling overrides for old config files */
	    room->persistent = j_atoi(xmlnode_get_tag_data(result, "persistant"), 0);
	    room->persistent = j_atoi(xmlnode_get_tag_data(result, "persistent"), 0);
	    room->logformat = j_atoi(xmlnode_get_tag_data(result, "logformat"), LOG_TEXT);

	    if(j_strcmp(xmlnode_get_tag_data(result, "logging"), "1") == 0)
	    {
		con_room_log_new(room);

		if (room->logfile == NULL)
		    log_alert(NULL, "cannot open log file for room %s", jid_full(room->id));
		else
		    con_room_log(room, NULL, "LOGGING STARTED");

	    }

	    room->creator = jid_new(room->p, xmlnode_get_tag_data(result, "creator"));

	    free(room->description);
	    room->description = j_strdup(xmlnode_get_tag_data(result, "description"));
	    free(room->name);
	    room->name = j_strdup(xmlnode_get_tag_data(result, "name"));

	    free(room->note_join);
	    room->note_join = j_strdup(xmlnode_get_tag_data(result, "notice/join"));
	    free(room->note_rename);
	    room->note_rename = j_strdup(xmlnode_get_tag_data(result, "notice/rename"));
	    free(room->note_leave);
	    room->note_leave = j_strdup(xmlnode_get_tag_data(result, "notice/leave"));

	    subject = pstrdup(room->p, xmlnode_get_tag_data(result, "subject"));

	    xmlnode_free(room->topic);
	    room->topic = xmlnode_new_tag("topic");
	    xmlnode_put_attrib(room->topic, "subject", subject);
	    xmlnode_insert_cdata(room->topic, "The topic has been set to: ", -1);
	    xmlnode_insert_cdata(room->topic, subject, -1);

	    xdb_room_lists_get(room);

	    xmlnode_free(result);
	}
    }
    else
    {
        log_debug(NAME, "[%s] skipping .. no results", FZONE);

	/* Set XDB, just in case */
	xdb_set(master->xdbc, fulljid, "muc:room:list", NULL);
    }

    xmlnode_free(node);
    xmlnode_free(current);
    pool_free(p);
}

void xdb_room_clear(cnr room)
{
    char *roomid;
    char *host;
    cni master;
    jid store;
    jid fulljid;
    xmlnode node;
    xmlnode item;
    pool p;

    if(room == NULL)
    {
	return;
    }

    p = pool_new();
    master = room->master;
    roomid = jid_full(room->id);
    host = room->id->server;

    fulljid = jid_new(p, spools(p, "rooms@", host, p));
    store = jid_new(p, spools(p, shahash(roomid), "@", host, p));

    log_debug(NAME, "[%s] asked to clear a room from xdb (%s)", FZONE, jid_full(room->id));

    /* Remove from rooms db */
    node = xdb_get(master->xdbc, fulljid, "muc:room:list");

    if(node != NULL)
    {
	item = xmlnode_get_tag(node, spools(p, "?jid=", jid_full(jid_fix(store)), p));

	if(item)
	{
            log_debug(NAME, "[%s] Found (%s) in rooms.xml - removing", FZONE, jid_full(room->id), jid_full(jid_fix(store)));
	    xmlnode_hide(item);
	    xdb_set(master->xdbc, fulljid, "muc:room:list", node);
	}
	else
	{
            log_debug(NAME, "[%s] (%s) not found in rooms.xml - ignoring", FZONE, jid_full(room->id), jid_full(jid_fix(store)));
	}
    }

    /* Clear lists */
    xdb_set(master->xdbc, store, "muc:list:owner", NULL);
    xdb_set(master->xdbc, store, "muc:list:admin", NULL);
    xdb_set(master->xdbc, store, "muc:list:member", NULL);
    xdb_set(master->xdbc, store, "muc:list:outcast", NULL);

    /* Clear room config */
    xdb_set(master->xdbc, store, "muc:room:config", NULL);

    xmlnode_free(node);
    pool_free(p);

    return;
}

int set_data(cni master, char *nick, char *jabberid, xmlnode node, int remove)
{
    xmlnode item;
    xmlnode old;
    int status;
    jid fulljid, userjid;
    char *current = NULL;
    char *user = NULL;
    char *host = NULL;
    pool p;

    if(master == NULL || ( nick == NULL && remove != 1 ) || jabberid == NULL)
    {
        return 0;
    }

    p = pool_new();

    host = master->i->id;
    fulljid = jid_new(p, spools(p, "registration@", host, p));
    userjid = jid_new(p, jabberid);

    if(nick)
    {
        log_debug(NAME, "[%s] asked to manage xdb nick(%s)", FZONE, nick);
	user = pstrdup(p, nick);
    
	for(current = user; *current != '\0'; current++)
            *current = tolower(*current); /* lowercase the group name */
    }

    xmlnode_put_attrib(node, "xmlns", "muc:data");

    old = xdb_get(master->xdbc, fulljid, "muc:data");
    item = xmlnode_get_tag(old, spools(p, "?jid=", jid_full(jid_user(jid_fix(userjid))), p));

    if(old == NULL)
        old = xmlnode_new_tag("registered");

    if(remove == 1)
    {
        log_debug(NAME, "[%s] asked to remove xdb info \n>%s<\n>%s< \n ", FZONE, xmlnode2str(old), xmlnode2str(item));

        if(item)
	    xmlnode_hide(item);
    }
    else
    {
        log_debug(NAME, "[%s] asked to add xdb info \n>%s<\n>%s< \n ", FZONE, xmlnode2str(old), xmlnode2str(item));
        xmlnode_hide(item);

        item = xmlnode_new_tag("item");
        xmlnode_put_attrib(item, "nick", nick);
        xmlnode_put_attrib(item, "keynick", user);
        xmlnode_put_attrib(item, "jid", jid_full(jid_user(jid_fix(userjid))));

        if(node)
        {
            xmlnode_insert_node(item, node);
            xmlnode_free(node);
        }

        xmlnode_insert_node(old, item);
        xmlnode_free(item);

        log_debug(NAME, "[%s] asked to add xdb info \n>%s<\n>%s< \n ", FZONE, xmlnode2str(old), xmlnode2str(item));
    }

    status = xdb_set(master->xdbc, fulljid, "muc:data", old);

    log_debug(NAME, "[%s] xdb status(%d)", FZONE, status);

    xmlnode_free(old);
    pool_free(p);
    return status;
}

xmlnode get_data_bynick(cni master, char *nick)
{
    xmlnode node;
    xmlnode result;
    jid fulljid;
    char *current, *user, *host;
    pool p;

    log_debug(NAME, "[%s] asked to find xdb nick (%s)", FZONE, nick);
    if(master == NULL || nick == NULL)
    {
        return NULL;
    }

    log_debug(NAME, "[%s] xdb user registration disabled (%s)", FZONE, nick);
    return NULL;

    p = pool_new();
    user = pstrdup(p, nick);
    host = master->i->id;

    for(current = user; *current != '\0'; current++)
        *current = tolower(*current); /* lowercase the group name */

    fulljid = jid_new(p, spools(p, "registration@", host, p));

    node = xdb_get(master->xdbc, fulljid, "muc:data");

    /* Set blank data in case file doesn't exist */
    if(node == NULL)
    {
        log_debug(NAME, "[%s] DBG: blank data", FZONE);
	xdb_set(master->xdbc, fulljid, "muc:data", NULL);
	pool_free(p);
        return NULL;
    }

    result = xmlnode_dup(xmlnode_get_tag(node, spools(p, "?keynick=", user, p)));

    log_debug(NAME, "[%s] asked to find xdb nick for %s - (%s)", FZONE, user, xmlnode2str(result));

    xmlnode_free(node);
    pool_free(p);
    return result;
}

xmlnode get_data_byjid(cni master, char *jabberid)
{
    xmlnode node;
    xmlnode result;
    jid fulljid, userjid;
    char *host;
    pool p;

    log_debug(NAME, "[%s] asked to find xdb jid (%s)", FZONE, jabberid);

    if(master == NULL || jabberid == NULL)
    {
        return NULL;
    }

    log_debug(NAME, "[%s] xdb user registration disabled", FZONE);
    return NULL;

    p = pool_new();
    host = master->i->id;

    userjid = jid_new(p, jabberid);
    fulljid = jid_new(p, spools(p, "registration@", host, p));

    node = xdb_get(master->xdbc, fulljid, "muc:data");

    /* Set blank data in case file doesn't exist */
    if(node == NULL)
    {
	xdb_set(master->xdbc, fulljid, "muc:data", NULL);
        pool_free(p);
        return NULL;
    }

    result = xmlnode_dup(xmlnode_get_tag(node, spools(p, "?jid=", jid_full(jid_user(jid_fix(userjid))), p)));

    log_debug(NAME, "[%s] asked to find xdb jid for %s - (%s)", FZONE, jid_full(jid_user(jid_fix(userjid))), xmlnode2str(result));

    xmlnode_free(node);
    pool_free(p);
    return result;
}

