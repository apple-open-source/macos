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

/* Generate extended presence entry */
xmlnode add_extended_presence(cnu from, cnu to, xmlnode presence, char *status, char *reason, char *actor)
{
    xmlnode tag;
    xmlnode element;
    xmlnode item;
    xmlnode output;

    taffil useraffil;
    trole userrole;
    jid user;
    cnr room;
    
    if(presence == NULL)
    {
	output = xmlnode_dup(from->presence);
    }
    else
    {
	output = xmlnode_dup(presence);
    }

    if(from == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing user variable in add_extended_presence", FZONE);
	return output;
    }

    user = from->realid;
    room = from->room;

    tag = xmlnode_insert_tag(output,"x");
    xmlnode_put_attrib(tag, "xmlns", NS_MUC_USER);

    item = xmlnode_insert_tag(tag, "item");

    if(room->visible == 1 || is_admin(room, to->realid))
    {
        xmlnode_put_attrib(item, "jid", jid_full(user));
    }
	
    useraffil = affiliation_level(room, user);
    xmlnode_put_attrib(item, "affiliation", useraffil.msg);

    userrole = role_level(room, user);
    xmlnode_put_attrib(item, "role", userrole.msg);

    log_debug(NAME, "[%s] status check: status >%s<", FZONE, status);

    /* If this is a nick change, include the new nick if available */
    if(j_strcmp(status, STATUS_MUC_CREATED) == 0)
    {
	room->locked = 1;
    }

    if(status != NULL)
    {
	log_debug(NAME, "[%s] Adding to epp: status >%s<, reason >%s<", FZONE, status, reason);

	/* If this is a nick change, include the new nick if available */
	if(j_strcmp(status, STATUS_MUC_NICKCHANGE) == 0)
	    if(xmlnode_get_data(from->nick) != NULL)
	        xmlnode_put_attrib(item, "nick", xmlnode_get_data(from->nick));

	/* Add reason if available */
        if(reason != NULL)
	{
            element = xmlnode_insert_tag(item, "reason");
	    xmlnode_insert_cdata(element, reason, -1);
	}

	/* Add actor if available */
        if(actor != NULL)
	{
            element = xmlnode_insert_tag(item, "actor");
	    xmlnode_put_attrib(element, "jid", actor);
	}

	/* Add status code if available */
        element = xmlnode_insert_tag(tag, "status");
	xmlnode_put_attrib(element,"code", status);
    }

    return output;
}

/* Is the user a Service Admin? */
int is_sadmin(cni master, jid user)
{
    char ujid[256];

    if(master == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_sadmin", FZONE);
	return 0;
    }

    snprintf(ujid, 256, "%s@%s", user->user, user->server);
    log_debug(NAME, "[%s] Is sadmin? >%s/%s<", FZONE, jid_full(user), ujid);

    if(g_hash_table_lookup(master->sadmin, ujid) != NULL )
        return 1;
    else
        return 0;
}

/* Is the user an owner for that room */
int is_owner(cnr room, jid user)
{
    char ujid[256];
    char cjid[256];

    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_owner", FZONE);
	return 0;
    }

    snprintf(ujid, 256, "%s@%s", user->user, user->server);
    if(room->creator)
    {
        snprintf(cjid, 256, "%s@%s", room->creator->user, room->creator->server);
    }
    else
    {
        snprintf(cjid, 256, "@");
    }

    log_debug(NAME, "[%s] Is Owner? >%s<", FZONE, jid_full(user));

    /* Server admin can override */
    if(is_sadmin(room->master, user))
	    return 2;
    else if(j_strcmp(cjid, ujid) == 0)
	    return 1;
    else if(g_hash_table_lookup(room->owner, ujid) != NULL )
	    return 1;
    else
	    return 0;

}

/* Is the user in the admin affiliation list for that room */
int is_admin(cnr room, jid user)
{
    char ujid[256];

    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_admin", FZONE);
	return 0;
    }

    snprintf(ujid, 256, "%s@%s", user->user, user->server);
    log_debug(NAME, "[%s] Is Admin? >%s<", FZONE, jid_full(user));

    if(is_owner(room, user))
	    return 2;

    if(g_hash_table_lookup(room->admin, ujid) != NULL )
	    return 1;
    else if(g_hash_table_lookup(room->admin, user->server) != NULL )
	    return 1;
    else
	    return 0;
}

/* Is the user in the moderator role list for that room */
int is_moderator(cnr room, jid user)
{
    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_moderator", FZONE);
	return 0;
    }

    if(is_owner(room, user))
    {
        log_debug(NAME, "[%s] Is Moderator? >%s< - Owner", FZONE, jid_full(user));
	return 2;
    }

    if(g_hash_table_lookup(room->moderator, jid_full(user)) != NULL )
    {
        log_debug(NAME, "[%s] Is Moderator? >%s< - Moderator", FZONE, jid_full(user));
	return 1;
    }
    else
    {
        log_debug(NAME, "[%s] Is Moderator? >%s< - No", FZONE, jid_full(user));
	return 0;
    }
}

/* Is the user in the participant role list for that room */
int is_participant(cnr room, jid user)
{
    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_participant", FZONE);
	return 0;
    }

    /* Every non-admin has voice in a non-moderated room */
    if(room->moderated == 0) 	
	return 1;

    /* Moderator has voice intrinsic */
    if(is_moderator(room, user))
	return 2;

    /* If moderated, check the voice list */
    if(g_hash_table_lookup(room->participant, jid_full(user)) != NULL )
	return 1;
    else
	return 0;
}

/* Is the user in the member affiliation list for that room */
int is_member(cnr room, jid user)
{
    char ujid[256];

    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_member", FZONE);
	return 0;
    }

    snprintf(ujid, 256, "%s@%s", user->user, user->server);

    /* Owner is automatically a member */
    if(is_owner(room, user))
    {
        log_debug(NAME, "[%s] Is Member? >%s< - Owner", FZONE, jid_full(user));
	return 1;
    }

    /* Admin is automatically a member */
    if(is_admin(room, user))
    {
        log_debug(NAME, "[%s] Is Member? >%s< - Admin", FZONE, jid_full(user));
	return 1;
    }

    if(g_hash_table_lookup(room->member, ujid) != NULL )
    {
        log_debug(NAME, "[%s] Is Member? >%s< - Yes (case 1)", FZONE, jid_full(user));
	    return 1;
    }
    else if(g_hash_table_lookup(room->member, user->server) != NULL )
    {
        log_debug(NAME, "[%s] Is Member? >%s< - Yes (case 2)", FZONE, jid_full(user));
	    return 1;
    }
    else
    {
        log_debug(NAME, "[%s] Is Member? >%s< - No", FZONE, jid_full(user));
	    return 0;
    }
}

/* Is the user in the outcast affiliation list for that room */
int is_outcast(cnr room, jid user)
{
    char ujid[256];

    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_outcast", FZONE);
	return 0;
    }

    snprintf(ujid, 256, "%s@%s", user->user, user->server);

    if(g_hash_table_lookup(room->outcast, ujid) != NULL )
	    return 1;
    else if(g_hash_table_lookup(room->outcast, user->server) != NULL )
	    return 1;
    else
	    return 0;
}

/* Only return 1 if visitor */
int is_visitor(cnr room, jid user)
{
    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_visitor", FZONE);
	return 0;
    }

    if(is_moderator(room, user))
	    return 0;
    else if(is_participant(room, user))
	    return 0;
    else if(g_hash_table_lookup(room->remote, jid_full(user)) != NULL )
	    return 1;
    else
	    return 0;
}

/* Is user in the room? */
int in_room(cnr room, jid user)
{
    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in in_room", FZONE);
	return 0;
    }

    if(g_hash_table_lookup(room->remote, jid_full(user)) != NULL )
	    return 1;
    else
	    return 0;
}

/* Is this a legacy client? */
int is_legacy(cnu user)
{
    cnr room;

    if(user == NULL)
    {
	log_warn(NAME, "[%s] ERR: Missing variable in is_legacy", FZONE);
	return 0;
    }

    room = user->room;

    if(room->legacy)
	return 1;
    else if(user->legacy)
	return 1;
    else
	return 0;
}

/* Is user leaving the room? */
int is_leaving(cnr room, jid user)
{
    cnu target;

    if(room == NULL || user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_leaving", FZONE);
	return 0;
    }

    target = g_hash_table_lookup(room->remote, jid_full(user));

    if(target != NULL )
    {
	if(target->leaving == 1)    
        {		
            return 1;
	}
    }

    return 0;
}

/* Check if user is already registered */
int is_registered(cni master, char *user, char *nick)
{
    xmlnode results;

    if(user == NULL || nick == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable in is_registered", FZONE);
	return 0;
    }

    results = get_data_bynick(master, nick);

    if(results != NULL)
    {
        log_debug(NAME, "[%s] Found %s in Registered Nicks - checking [%s/%s]", FZONE, nick, user, xmlnode_get_attrib(results, "jid"));

        if(j_strcmp(user, xmlnode_get_attrib(results, "jid")) != 0)
        {
	    /* User taken by someone else */
            xmlnode_free(results);
            return -1;
        }
	else
	{
	    /* Nick belongs to me */
            xmlnode_free(results);
	    return 1;
	}
    }
    else
    {
	/* Nick is free */
        xmlnode_free(results);
	return 0;
    }
}

/* Generic alert function for user/room */
xmlnode _con_send_alert(cnu user, char *text, char *subject, char *status)
{
    xmlnode msg;
    xmlnode element;
    char body[256];
    char reason[128];
    char *type = NULL;
    cnr room;
    char *room_id;

    if(user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
	return NULL;
    }

    room = user->room;
    room_id = jid_full(room->id);

    if(is_legacy(user) == 0)
    {
	return NULL;
    }

    if(status == NULL)
    {
	    snprintf(body, 256, "%s", text);
    }
    else 
    {
	if(text == NULL)
		strcpy(reason, "None given");
	else
		snprintf(reason, 128, "%s", text);

        if(j_strcmp(status, STATUS_MUC_KICKED) == 0)
	{
	    type = "normal";
	    snprintf(body, 256, "You have been kicked from the room %s. \n Reason: %s", room_id, reason);
	}

        if(j_strcmp(status, STATUS_MUC_BANNED) == 0)
	{
	    type = "normal";
	    snprintf(body, 256, "You have been kicked and outcast from the room %s. \n Reason: %s", room_id, reason);
	}

        if(j_strcmp(status, STATUS_MUC_SHOWN_JID) == 0)
	{
	    type = "groupchat";
	    snprintf(body, 256, "This room (%s) is not anonymous", room_id);
	}

        if(j_strcmp(status, STATUS_MUC_HIDDEN_JID) == 0)
	{
	    type = "groupchat";
	    snprintf(body, 256, "This room (%s) is anonymous, except for admins", room_id);
	    status = STATUS_MUC_SHOWN_JID;
	}
    }
        
    msg = jutil_msgnew(type, jid_full(user->realid) , subject, body);
    xmlnode_put_attrib(msg, "from", room_id);
    
    if(status != NULL)
    {
        element = xmlnode_insert_tag(msg,"x");
        xmlnode_put_attrib(element, "xmlns", NS_MUC_USER);
        xmlnode_put_attrib(xmlnode_insert_tag(element, "status"), "code", status);

    }

    return msg;
}

/* User alert wrapper */
void con_send_alert(cnu user, char *text, char *subject, char *status)
{
    xmlnode msg = _con_send_alert(user, text, subject, status);

    if(msg)
    {
        deliver(dpacket_new(msg), NULL);
    }
}

/* Room status/alert wrapper */
void _con_send_room_status(gpointer key, gpointer data, gpointer arg)
{
    char *status = (char*)arg;
    cnu user = (cnu)data;
    xmlnode msg = _con_send_alert(user, NULL, NULL, status);

    if(msg)
    {
        deliver(dpacket_new(msg), NULL);
    }
}

/* Send status change to a room */
void con_send_room_status(cnr room, char *status)
{
    if(room == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
	return;
    }

    g_hash_table_foreach(room->local, _con_send_room_status, (void*)status);
}   

/* Splice \n-delimited line into array */
char *linesplit(char **entry) 
{
    char *line;
    char *point;
    char *str = "\n";

    if(!(*entry)) 
        return NULL;

    line = *entry;

    if((point = strstr(*entry, str)))
    {
        *point = 0;
        *entry = point+strlen(str);
    } 
    else
        *entry = NULL;

    return line;
}

/* Integer to String conversion */
char *itoa(int number, char *result)
{
    sprintf(result, "%d", number);
    return result;
}
		
/* Custom Debug message */
char *funcstr(char *file, char *function, int line)
{           
    static char buff[128];
    int i;
		            
    i = snprintf(buff,127,"%s:%d (%s)",file,line,function);
    buff[i] = '\0';
		            
    return buff;
}       

/* Return current date for logfile system */
int minuteget(time_t tin)
{
    time_t timef;
    char timestr[50];
    size_t timelen = 49;
    int results;

    if(tin)
	timef = tin;
    else 
        timef = time(NULL);

    strftime(timestr, timelen, "%M", localtime(&timef));
    results = j_atoi(timestr, -1);

    return results;
}

/* Return current date for logfile system */
char *timeget(time_t tin)
{
    time_t timef;
    char timestr[50];
    size_t timelen = 49;

    if(tin)
	timef = tin;
    else 
        timef = time(NULL);

    strftime(timestr, timelen, "%H:%M", localtime(&timef));

    return j_strdup(timestr);
}

/* Return current date for logfile system */
char *dateget(time_t tin)
{
    time_t timef;
    char timestr[50];
    size_t timelen = 49;

    if(tin)
	timef = tin;
    else 
        timef = time(NULL);

    strftime(timestr, timelen, "%Y-%m-%d", localtime(&timef));

    return j_strdup(timestr);
}

/* Send presence update for a user to the room */
void update_presence(cnu user)
{
    xmlnode result;
    cnr room;

    if(user == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
	return;
    }

    room = user->room;
		    
    /* Send updated presence packet */
    result = xmlnode_dup(user->presence);
    xmlnode_put_vattrib(result,"cnu",(void*)user);

    g_hash_table_foreach(room->local, con_room_sendwalk, (void*)result);
    xmlnode_free(result);
    return;
}

/* Generate custom errors for multi-item handler */
void insert_item_error(xmlnode node, char *code, char *msg)
{
    xmlnode element;

    element = xmlnode_insert_tag(node, "error");
    xmlnode_put_attrib(element, "code", code);
    xmlnode_insert_cdata(element, msg, -1);
}

/* Add user into the room roster hash */
int add_roster(cnr room, jid userid)
{
    xmlnode store;
    xmlnode node;
    xmlnode old;
    char *key;
    char ujid[256];

    if(room == NULL || userid == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
        return -1;
    }

    snprintf(ujid, 256, "%s@%s", userid->user, userid->server);
    key = j_strdup(ujid);
    old = g_hash_table_lookup(room->roster, key);

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
	    log_debug(NAME, "[%s] DBG: Already in node, ignoring\n", FZONE);
	    xmlnode_free(store);
            free(key);
            return 0;
        }
    }

    if(userid->resource != NULL)
    {
        log_debug(NAME, "[%s] adding entry (%s) for jid (%s)", FZONE, jid_full(userid), ujid);
        node = xmlnode_new_tag("item");
        xmlnode_put_attrib(node, "jid", jid_full(userid));

        xmlnode_insert_node(store, node);
        xmlnode_free(node);
    }

    g_hash_table_insert(room->roster, key, store);

    return 1;
}

/* Remove a user from the room roster hash */
int remove_roster(cnr room, jid userid)
{
    xmlnode store;
    xmlnode old;
    xmlnode node;
    char *key;
    char ujid[256];

    if(room == NULL || userid == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
        return -1;
    }

    snprintf(ujid, 256, "%s@%s", userid->user, userid->server);

    key = j_strdup(ujid);
    old = g_hash_table_lookup(room->roster, key);

    if(old == NULL)
    {
        free(key);
        return 1;
    }
    store = xmlnode_dup(old);

    node = xmlnode_get_tag(store, spools(xmlnode_pool(store), "item?jid=", jid_full(userid), xmlnode_pool(store)));

    if(node == NULL)
    {
	log_debug(NAME, "[%s] DBG: Already removed from node, ignoring\n", FZONE);
        xmlnode_free(store);
        free(key);
        return 1;
    }

    xmlnode_hide(node);

    node = xmlnode_get_tag(store, "item");

    if(node == NULL)
    {
        log_debug(NAME, "[%s] Removing empty entry for jid (%s)", FZONE, ujid);
	g_hash_table_remove(room->roster, key);
	xmlnode_free(store);
        free(key);
    }
    else
    {
        log_debug(NAME, "[%s] Removing entry (%s) for jid (%s)", FZONE, jid_full(userid), ujid);

        g_hash_table_insert(room->roster, key, store);
    }

    return 1;
}

/* Get the entries from the room roster hash */
xmlnode get_roster(cnr room, jid userid)
{
    xmlnode store;
    char *key;
    char ujid[256];

    if(room == NULL || userid == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
        return NULL;
    }

    snprintf(ujid, 256, "%s@%s", userid->user, userid->server);

    key = j_strdup(ujid);
    store = g_hash_table_lookup(room->roster, key);
    free(key);

    return store;
}

char *extractAction(char *origin, pool p)
{
    int i;
    int end;
    spool sp;
    char *output;
    char in[2];

    if(origin == NULL || p == NULL)
    {
        log_warn(NAME, "[%s] ERR: Missing variable", FZONE);
	return NULL;
    }

    sp = spool_new(p);

    end = j_strlen(origin);

    for (i = 3 ; i <= end ; i++)
    {
	in[0] = origin[i];
	in[1] = '\0';

	log_debug(NAME, "[%s] >%s< saved", FZONE, in);
	spooler(sp, in, sp);
    }
    output = spool_print(sp);

    return output;
}

/* Check Primeness for hash functions */
int isPrime(unsigned long n)
{
    int prime = 1;
    unsigned long p1,p2 , s;

    if(n > 3)
    {
        p1 = 3;
        p2 = n - 3;
        s = 9;

        while((prime = p2 % p1 ) && (s <= p2))
	{
            p1 += 2;
            p2 -= 2;
            s <<= 2;
            s++;
        }
    }
    return prime;
}

/* Used to check jids and fix case. */
jid jid_fix(jid id)
{
    unsigned char *str;

    if(id == NULL)
    {
        log_warn(NAME, "[%s] ERR - id NULL", FZONE);
        return NULL;
    }

    if(id->server == NULL || j_strlen(id->server) == 0 || j_strlen(id->server) > 255)
        return NULL;

    /* lowercase the hostname, make sure it's valid characters */
    for(str = id->server; *str != '\0'; str++)
    {
        *str = tolower(*str);
    }

    /* cut off the user */
    //if(id->user != NULL && j_strlen(id->user) > 64)
    //    id->user[64] = '\0';

    /* check for low and invalid ascii characters in the username */
    //if(id->user != NULL)
    //    for(str = id->user; *str != '\0'; str++)
    //	{
    //        *str = tolower(*str);
    //	}

    return id;
}
