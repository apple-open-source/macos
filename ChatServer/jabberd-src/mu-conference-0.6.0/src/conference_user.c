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
extern int deliver__flag;

cnu con_user_new(cnr room, jid id)
{
    pool p;
    cnu user;
    char *key;

    log_debug(NAME, "[%s] adding user %s to room %s", FZONE, jid_full(jid_fix(id)), jid_full(jid_fix(room->id)));

    p = pool_new(); /* Create pool for user struct */
    user = pmalloco(p, sizeof(_cnu));

    user->p = p;
    user->realid = jid_new(user->p, jid_full(jid_fix(id)));
    user->room = room;
    user->presence = jutil_presnew(JPACKET__AVAILABLE, NULL, NULL);

    key = j_strdup(jid_full(user->realid));
    g_hash_table_insert(room->remote, key, (void*)user);

    /* Add this user to the room roster */
    add_roster(room, user->realid);

    /* If admin, switch */
    if(is_admin(room, user->realid) && !is_moderator(room, user->realid))
    {
	log_debug(NAME, "[%s] Adding %s to moderator list", FZONE, jid_full(jid_fix(user->realid)));

	/* Update affiliate info */
	add_affiliate(room->admin, user->realid, NULL);
	add_role(room->moderator, user);
    }
    else if(is_member(room, user->realid) && !is_admin(room, user->realid))
    {
	/* Update affiliate information */
	log_debug(NAME, "[%s] Updating %s in the member list", FZONE, jid_full(user->realid));

	add_affiliate(room->member, user->realid, NULL);
	add_role(room->participant, user);
    }
    else if(room->moderated == 1 && room->defaulttype == 1)
    {
        /* Auto-add to participant list if moderated and participant type is default */
	add_role(room->participant, user);
    }

    return user;
}

void _con_user_history_send(cnu to, xmlnode node)
{

    if(to == NULL || node == NULL)
    {
        return;
    }

    xmlnode_put_attrib(node, "to", jid_full(to->realid));
    deliver(dpacket_new(node), NULL);
    return;
}

void _con_user_nick(gpointer key, gpointer data, gpointer arg)
{
    cnu to = (cnu)data;
    cnu from = (cnu)arg;
    char *old, *status, *reason, *actor;
    xmlnode node;
    xmlnode result;
    xmlnode element;
    jid fullid;

    /* send unavail pres w/ old nick */
    if((old = xmlnode_get_attrib(from->nick,"old")) != NULL)
    {
	
	if(xmlnode_get_data(from->nick) != NULL)
	{
            node = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(to->realid), NULL);
	}
	else
	{
            node = xmlnode_dup(from->presence);
	    xmlnode_put_attrib(node, "to", jid_full(to->realid));
	}

        fullid = jid_new(xmlnode_pool(node), jid_full(from->localid));
        jid_set(fullid, old, JID_RESOURCE);
        xmlnode_put_attrib(node, "from", jid_full(fullid));

	status = xmlnode_get_attrib(from->nick,"status");
	log_debug(NAME, "[%s] status = %s", FZONE, status);

	reason = xmlnode_get_attrib(from->nick,"reason");
	actor = xmlnode_get_attrib(from->nick,"actor");
	
	if(xmlnode_get_data(from->nick) != NULL)
        { 
            log_debug(NAME, "[%s] Extended presence - Nick Change", FZONE);
            result = add_extended_presence(from, to, node, STATUS_MUC_NICKCHANGE, NULL, NULL);
        }
        else
        {
            log_debug(NAME, "[%s] Extended presence", FZONE);

	    result = add_extended_presence(from, to, node, status, reason, actor);
        }

	deliver(dpacket_new(result), NULL);
        xmlnode_free(node);
    }

    /* if there's a new nick, broadcast that too */
    if(xmlnode_get_data(from->nick) != NULL)
    {
	status = xmlnode_get_attrib(from->nick,"status");
	log_debug(NAME, "[%s] status = %s/%s", FZONE, status, STATUS_MUC_CREATED);

	if(j_strcmp(status, STATUS_MUC_CREATED) == 0)
	    node = add_extended_presence(from, to, NULL, status, NULL, NULL);
	else
	    node = add_extended_presence(from, to, NULL, NULL, NULL, NULL);

	/* Hide x:delay, not needed */
	element = xmlnode_get_tag(node, "x?xmlns=jabber:x:delay");
	if(element)
	    xmlnode_hide(element);

        xmlnode_put_attrib(node, "to", jid_full(to->realid));

        fullid = jid_new(xmlnode_pool(node), jid_full(from->localid));
        jid_set(fullid, xmlnode_get_data(from->nick), JID_RESOURCE);
        xmlnode_put_attrib(node, "from", jid_full(fullid));

        deliver(dpacket_new(node), NULL);
    }
}

void con_user_nick(cnu user, char *nick, xmlnode data)
{
    xmlnode node;
    char *status, *reason, *actor;
    cnr room = user->room;

    log_debug(NAME, "[%s] in room %s changing nick for user %s to %s from %s", FZONE, jid_full(room->id), jid_full(user->realid), nick, xmlnode_get_data(user->nick));

    node = xmlnode_new_tag("n");
    xmlnode_put_attrib(node, "old", xmlnode_get_data(user->nick));

    if (data)
    {
	status = xmlnode_get_attrib(data, "status");
	reason = xmlnode_get_data(data);
	actor = xmlnode_get_attrib(data, "actor");

        if(status)
            xmlnode_put_attrib(node, "status", status);

        if(reason)
	    xmlnode_put_attrib(node, "reason", reason);

	if(actor)
	    xmlnode_put_attrib(node, "actor", actor);

	log_debug(NAME, "[%s] status = %s", FZONE, status);
    }

    xmlnode_insert_cdata(node,nick,-1);

    xmlnode_free(user->nick);
    user->nick = node;

    deliver__flag = 0;
    g_hash_table_foreach(room->local, _con_user_nick, (void*)user);
    deliver__flag = 1;
    deliver(NULL, NULL);

    /* send nick change notice if availble */
    if(room->note_rename != NULL && nick != NULL && xmlnode_get_attrib(node, "old") != NULL && j_strlen(room->note_rename) > 0)
        con_room_send(room, jutil_msgnew("groupchat", NULL, NULL, spools(xmlnode_pool(node), xmlnode_get_attrib(node, "old"), " ", room->note_rename, " ", nick, xmlnode_pool(node))), SEND_LEGACY);
}

void _con_user_enter(gpointer key, gpointer data, gpointer arg)
{
    cnu from = (cnu)data;
    cnu to = (cnu)arg;
    xmlnode node;
    jid fullid;

    /* mirror */
    if(from == to)
        return;

    node = add_extended_presence(from, to, NULL, NULL, NULL, NULL);

    xmlnode_put_attrib(node, "to", jid_full(to->realid));

    fullid = jid_new(xmlnode_pool(node), jid_full(from->localid));
    jid_set(fullid, xmlnode_get_data(from->nick), JID_RESOURCE);
    xmlnode_put_attrib(node, "from", jid_full(fullid));

    deliver(dpacket_new(node), NULL);
}

void con_user_enter(cnu user, char *nick, int created)
{
    xmlnode node;
    xmlnode message;
    char *key;
    int h, tflag = 0;
    cnr room = user->room;

    user->localid = jid_new(user->p, jid_full(room->id));
    jid_set(user->localid, shahash(jid_full(user->realid)), JID_RESOURCE);

    key = j_strdup(user->localid->resource);
    g_hash_table_insert(room->local, key, (void*)user);

    room->count++;

    log_debug(NAME, "[%s] officiating user %s in room (created = %d) %s as %s/%s", FZONE, jid_full(user->realid), created, jid_full(room->id), nick, user->localid->resource);

    /* Send presence back to user to confirm presence received */
    if(created == 1)
    {
	/* Inform if room just created */
        node = xmlnode_new_tag("reason");
	xmlnode_put_attrib(node, "status", STATUS_MUC_CREATED);
        con_user_nick(user, nick, node); /* pushes to everyone (including ourselves) our entrance */
	xmlnode_free(node);
    }
    else
    {
        con_user_nick(user, nick, NULL); /* pushes to everyone (including ourselves) our entrance */
    }

    /* Send Room MOTD */
    if(j_strlen(room->description) > 0)
    {
        message = jutil_msgnew("groupchat", jid_full(user->realid), NULL, room->description);
	xmlnode_put_attrib(message,"from", jid_full(room->id));
	deliver(dpacket_new(message), NULL);
    }

    /* Send Room protocol message to legacy clients */
    if(is_legacy(user))
    {
        message = jutil_msgnew("groupchat", jid_full(user->realid), NULL, spools(user->p, "This room supports the MUC protocol.", user->p));
	xmlnode_put_attrib(message,"from", jid_full(room->id));
	deliver(dpacket_new(message), NULL);
    }

    /* Send Room Lock warning if necessary */
    if(room->locked > 0)
    {
        message = jutil_msgnew("groupchat", jid_full(user->realid), NULL, spools(user->p, "This room is locked from entry until configuration is confirmed.", user->p));
	xmlnode_put_attrib(message,"from", jid_full(room->id));
	deliver(dpacket_new(message), NULL);
    }

    /* Update my roster with current users */
    g_hash_table_foreach(room->local, _con_user_enter, (void*)user);

    /* Switch to queue mode */
    deliver__flag = 0;

    /* XXX Require new history handler */

    /* loop through history and send back */
    if(room->master->history > 0)
    {

       //if (xmlnode_get_attrib(user->history, "maxchars") != NULL )
       
       h = room->hlast;
       while(1)
       {
          h++;

          if(h == room->master->history)
              h = 0;

          _con_user_history_send(user, xmlnode_dup(room->history[h]));

          if(xmlnode_get_tag(room->history[h],"subject") != NULL)
              tflag = 1;

          if(h == room->hlast)
              break;
       }
    }

    /* Re-enable delivery */
    deliver__flag = 1;
    /* Send queued messages */
    deliver(NULL, NULL);

    /* send last know topic */
    if(tflag == 0 && room->topic != NULL)
    {
        node = jutil_msgnew("groupchat", jid_full(user->realid), xmlnode_get_attrib(room->topic,"subject"), xmlnode_get_data(room->topic));
        xmlnode_put_attrib(node, "from", jid_full(room->id));
        deliver(dpacket_new(node), NULL);
    }

    /* send entrance notice if available */
    if(room->note_join != NULL && j_strlen(room->note_join) > 0)
        con_room_send(room, jutil_msgnew("groupchat", NULL, NULL, spools(user->p, nick, " ", room->note_join, user->p)), SEND_LEGACY);

    /* Send 'non-anonymous' message if necessary */
    if(room->visible == 1)
        con_send_alert(user, NULL, NULL, STATUS_MUC_SHOWN_JID);
}

void con_user_process(cnu to, cnu from, jpacket jp)
{
    xmlnode node, element;
    cnr room = to->room;
    char str[10];
    int t;

    /* we handle all iq's for this id, it's *our* id */
    if(jp->type == JPACKET_IQ)
    {
        if(NSCHECK(jp->iq,NS_BROWSE))
        {
            jutil_iqresult(jp->x);
            node = xmlnode_insert_tag(jp->x, "item");

            xmlnode_put_attrib(node, "category", "user");
            xmlnode_put_attrib(node, "xmlns", NS_BROWSE);
            xmlnode_put_attrib(node, "name", xmlnode_get_data(to->nick));

            element = xmlnode_insert_tag(node, "item");
            xmlnode_put_attrib(element, "category", "user");

            if(room->visible == 1 || is_moderator(room, from->realid))
                xmlnode_put_attrib(element, "jid", jid_full(to->realid));
	    else
                xmlnode_put_attrib(element, "jid", jid_full(to->localid));
	   
	    if(is_legacy(to))
                xmlnode_insert_cdata(xmlnode_insert_tag(node, "ns"), NS_GROUPCHAT, -1);
	    else
                xmlnode_insert_cdata(xmlnode_insert_tag(node, "ns"), NS_MUC, -1);

            deliver(dpacket_new(jp->x), NULL);
            return;
        }

        if(NSCHECK(jp->iq,NS_LAST))
        {
            jutil_iqresult(jp->x);

            node = xmlnode_insert_tag(jp->x, "query");
            xmlnode_put_attrib(node, "xmlns", NS_LAST);

            t = time(NULL) - to->last;
            sprintf(str,"%d",t);

            xmlnode_put_attrib(node ,"seconds", str);

            deliver(dpacket_new(jp->x), NULL);
            return;
        }

        /* deny any other iq's if it's private */
        if(to->private == 1)
        {
            jutil_error(jp->x, TERROR_FORBIDDEN);
            deliver(dpacket_new(jp->x), NULL);
            return;
        }

        /* if not, fall through and just forward em on I guess! */
    }

    /* Block possibly faked groupchat messages - groupchat is not meant for p2p chats */
    if(jp->type == JPACKET_MESSAGE)
    {
        if(jp->subtype == JPACKET__GROUPCHAT)
	{
	    jutil_error(jp->x, TERROR_BAD);
	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}

	if(room->privmsg == 1 && !is_admin(room, from->realid))
	{
	    /* Only error on messages with body, otherwise just drop */
	    if(xmlnode_get_tag(jp->x, "body") != NULL)
            {	
	        jutil_error(jp->x, TERROR_MUC_PRIVMSG);
	        deliver(dpacket_new(jp->x), NULL);
	        return;
	    }
	    else
	    {
		xmlnode_free(jp->x);
		return;
	    }
	}
	
    }

    con_user_send(to, from, jp->x);
}

void con_user_send(cnu to, cnu from, xmlnode node)
{
    jid fullid;

    if(to == NULL || from == NULL || node == NULL)
    {
        return;
    }

    fullid = jid_new(xmlnode_pool(node), jid_full(from->localid));
    xmlnode_put_attrib(node, "to", jid_full(to->realid));

    if(xmlnode_get_attrib(node, "cnu") != NULL)
        xmlnode_hide_attrib(node, "cnu");

    jid_set(fullid, xmlnode_get_data(from->nick), JID_RESOURCE);

    xmlnode_put_attrib(node, "from", jid_full(fullid));
    deliver(dpacket_new(node), NULL);
}

void con_user_zap(cnu user, xmlnode data)
{
    cnr room;
    char *reason;
    char *status;
    char *key;

    if(user == NULL || data == NULL)
    {
        log_warn(NAME, "Aborting: NULL attribute found", FZONE);

	if(data != NULL)
            xmlnode_free(data);

        return;
    }
    
    user->leaving = 1;

    key = pstrdup(user->p, jid_full(user->realid));

    status = xmlnode_get_attrib(data, "status");
    reason = xmlnode_get_data(data);

    room = user->room;

    if(room == NULL)
    {
	log_warn(NAME, "[%s] Unable to zap user %s <%s-%s> : Room does not exist", FZONE, jid_full(user->realid), status, reason);
        xmlnode_free(data);
        return;
    }
    
    log_debug(NAME, "[%s] zapping user %s <%s-%s>", FZONE, jid_full(user->realid), status, reason);

    if(user->localid != NULL)
    {
        con_user_nick(user, NULL, data); /* sends unavailve */

        log_debug(NAME, "[%s] Removing entry from local list", FZONE);
        g_hash_table_remove(room->local, user->localid->resource);
        room->count--;

        /* send departure notice if available*/
	if(room->note_leave != NULL && j_strlen(room->note_leave) > 0)
	{
	    if(reason != NULL)
            {
		if(j_strcmp(status, STATUS_MUC_KICKED) == 0)
		{
	            con_room_send(room,jutil_msgnew("groupchat",NULL,NULL,spools(user->p, xmlnode_get_attrib(user->nick,"old")," ",room->note_leave,": [Kicked] ", reason, user->p)), SEND_LEGACY);
	        }
		else if(j_strcmp(status, STATUS_MUC_BANNED) == 0)
		{
	            con_room_send(room,jutil_msgnew("groupchat",NULL,NULL,spools(user->p,xmlnode_get_attrib(user->nick,"old")," ",room->note_leave,": [Banned] ", reason, user->p)), SEND_LEGACY);
		}
	        else
	        {
	            con_room_send(room,jutil_msgnew("groupchat",NULL,NULL,spools(user->p,xmlnode_get_attrib(user->nick,"old")," ",room->note_leave,": ", reason, user->p)), SEND_LEGACY);
		}
	    }
            else 
	    {
	        con_room_send(room,jutil_msgnew("groupchat",NULL,NULL,spools(user->p,xmlnode_get_attrib(user->nick,"old")," ",room->note_leave,user->p)), SEND_LEGACY);
	    }
	}
    }

    xmlnode_free(data);

    log_debug(NAME, "[%s] Removing any affiliate info from admin list", FZONE);
    log_debug(NAME, "[%s] admin list size [%d]", FZONE, g_hash_table_size(room->admin));
    remove_affiliate(room->admin, user->realid);

    log_debug(NAME, "[%s] Removing any affiliate info from member list", FZONE);
    log_debug(NAME, "[%s] member list size [%d]", FZONE, g_hash_table_size(room->member));
    remove_affiliate(room->member, user->realid);

    log_debug(NAME, "[%s] Removing any role info from moderator list", FZONE);
    log_debug(NAME, "[%s] moderator list size [%d]", FZONE, g_hash_table_size(room->moderator));
    revoke_role(room->moderator, user);
    log_debug(NAME, "[%s] Removing any role info from participant list", FZONE);
    log_debug(NAME, "[%s] participant list size [%d]", FZONE, g_hash_table_size(room->participant));
    revoke_role(room->participant, user);

    log_debug(NAME, "[%s] Removing any roster info from roster list", FZONE);
    remove_roster(room, user->realid);

    log_debug(NAME, "[%s] Un-alloc presence xmlnode", FZONE);
    xmlnode_free(user->presence);
    log_debug(NAME, "[%s] Un-alloc nick xmlnode", FZONE);
    xmlnode_free(user->nick);
    log_debug(NAME, "[%s] Un-alloc history xmlnode", FZONE);
    xmlnode_free(user->history);

    log_debug(NAME, "[%s] Removing from remote list and un-alloc cnu", FZONE);
    g_hash_table_remove(room->remote, jid_full(user->realid));
}

