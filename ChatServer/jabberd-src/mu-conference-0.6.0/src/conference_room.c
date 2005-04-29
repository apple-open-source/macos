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
extern int deliver__flag;

/* Handles logging for each room, simply returning if logfile is not defined */
void con_room_log(cnr room, char *nick, char *message)
{
    time_t t;
    xmlnode xml;
    jid user;
    char *output;
    char timestr[50];
    size_t timelen = 49;
    FILE *logfile;
    pool p;

    if(message == NULL || room == NULL) 
    {
	log_warn(NAME, "[%s] ERR: Aborting - NULL reference found - [%s][%s]", FZONE, message, room);
	return;
    }

    logfile = room->logfile;

    if(logfile == NULL) 
    {
	log_debug(NAME, "[%s] Logging not enabled for this room", FZONE);
	return;
    }

    p = pool_heap(1024);

    /* nicked from mod_time */
    t = time(NULL);
    strftime(timestr, timelen, "[%H:%M:%S]", localtime(&t));

    if(room->logformat == LOG_XML)
    {
	xml = jutil_msgnew("groupchat", jid_full(room->id) , NULL, strescape(p, message));

	user = jid_new(xmlnode_pool(xml), jid_full(room->id));
	jid_set(user, nick, JID_RESOURCE);
	xmlnode_put_attrib(xml, "from", jid_full(user));

	jutil_delay(xml, NULL);

	fprintf(logfile, "%s\n", xmlnode2str(xml));

        xmlnode_free(xml);
    }
    else if(room->logformat == LOG_XHTML)
    {
        if(nick)
        {
	    if(j_strncmp(message, "/me", 3) == 0)
	    {
		output = extractAction(strescape(p, message), p);
	        fprintf(logfile, "%s * %s%s<br />\n", timestr, nick, output);
	    }
	    else
	    {
	        fprintf(logfile, "%s &lt;%s&gt; %s<br />\n", timestr, nick, strescape(p, message));
	    }
        }
        else
        {
	    fprintf(logfile, "%s --- %s<br />\n", timestr, message);
        }
    }
    else
    {
        if(nick)
        {
	    if(j_strncmp(message, "/me", 3) == 0)
	    {
		output = extractAction(message, p);
	        fprintf(logfile, "%s * %s%s\n", timestr, nick, output);
	    }
	    else
	    {
	        fprintf(logfile, "%s <%s> %s\n", timestr, nick, message);
	    }
        }
        else
        {
	    fprintf(logfile, "%s --- %s\n", timestr, message);
        }
    }

    fflush(logfile);
    pool_free(p);
    return;
}

void con_room_log_new(cnr room)
{
    char *filename;
    char *curdate;
    char *dirname;
    struct stat fileinfo;
    time_t now = time(NULL);
    int type;
    pool p;
    spool sp;

    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL room", FZONE);
	return;
    }

    p = pool_heap(1024);
    type = room->logformat;
    dirname = jid_full(room->id);
    sp = spool_new(p);

    if(room->master->logdir)
    {
        spooler(sp, room->master->logdir, "/", dirname, sp);
    }
    else
    {
        spooler(sp, "./", dirname, sp);
    }

    filename = spool_print(sp);

    if(stat(filename,&fileinfo) < 0 && mkdir(filename, S_IRWXU) < 0)
    {
	log_warn(NAME, "[%s] ERR: unable to open log directory >%s<", FZONE, filename);
	return;
    }

    curdate = dateget(now);

    if(type == LOG_XML)
        spooler(sp, "/", curdate, ".xml", sp);
    else if(type == LOG_XHTML)
        spooler(sp, "/", curdate, ".html", sp);
    else
        spooler(sp, "/", curdate, ".txt", sp);


    filename = spool_print(sp);

    if(stat(filename,&fileinfo) < 0)
    {
	log_debug(NAME, "[%s] New logfile >%s<", FZONE, filename);

        room->logfile = fopen(filename, "a");

	if(type == LOG_XHTML && room->logfile != NULL)
	{
	    fprintf(room->logfile, "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n<head>\n<title>Logs for %s, %s</title>\n</head>\n<body>\n", jid_full(room->id), curdate);
            fflush(room->logfile);
	}
    }
    else
    {
        room->logfile = fopen(filename, "a");
    }


    if(room->logfile == NULL)
	log_warn(NAME, "[%s] ERR: unable to open log file >%s<", FZONE, filename);
    else
	log_debug(NAME, "[%s] Opened logfile >%s<", FZONE, filename);

    pool_free(p);
    free(curdate);
    return;
}

void con_room_log_close(cnr room)
{
    int type;
    FILE *logfile;

    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL room", FZONE);
	return;
    }

    type = room->logformat;
    logfile = room->logfile;

    if(logfile == NULL)
    {
	log_warn(NAME, "[%s] Aborting - NULL logfile", FZONE);
	return;
    }

    log_debug(NAME, "[%s] Closing logfile for room >%s<", FZONE, jid_full(room->id));

    if(type == LOG_XHTML)
    {
        fprintf(logfile, "</body>\n</html>\n");
        fflush(logfile);
    }

    fclose(room->logfile);
    room->logfile = NULL;
}

void con_room_send_invite(cnu sender, xmlnode node)
{
    xmlnode result;
    xmlnode element;
    xmlnode invite;
    char *body, *user, *reason, *inviter;
    cnr room;
    jid from;
    pool p;

    if(sender == NULL || node == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    log_debug(NAME, "[%s] Sending room invite", FZONE);

    room = sender->room;
    from = sender->realid;

    invite = xmlnode_get_tag(node, "invite");
    user = xmlnode_get_attrib(invite, "to");
    reason = xmlnode_get_tag_data(invite, "reason");

    if(room->public == 1)
    {
        inviter = jid_full(jid_user(jid_fix(from)));
    }
    else
    {
	inviter = xmlnode_get_data(sender->nick);
    }

    xmlnode_put_attrib(invite, "from", inviter);
    xmlnode_hide_attrib(invite, "to");

    p = xmlnode_pool(node);

    if(reason == NULL)
    {
	reason = spools(p, "None given", p);
    }

	// Apple Radar 3998957: change invitation to be more readable in iChat.
//    body = spools(p, "You have been invited to the ", jid_full(jid_fix(room->id)), " room by ", inviter, "\nReason: ", reason, p);
    body = spools(p, "You have been invited to a chat room by ", inviter, "\nReason: ", reason, p);

    result = jutil_msgnew("normal", user , "Invitation", body);
    xmlnode_put_attrib(result, "from", jid_full(jid_fix(room->id)));

    if(room->secret != NULL)
    {
	xmlnode_insert_cdata(xmlnode_insert_tag(invite, "password"), room->secret, -1);
    }

    xmlnode_insert_node(result, node);

    element = xmlnode_insert_tag(result, "x");
    xmlnode_put_attrib(element, "jid", jid_full(jid_fix(room->id)));
    xmlnode_put_attrib(element, "xmlns", NS_X_CONFERENCE);
    xmlnode_insert_cdata(element, reason, -1);

    log_debug(NAME, "[%s] >>>%s<<<", FZONE, xmlnode2str(result));

    deliver(dpacket_new(result), NULL);
    xmlnode_free(node);
    return;
    
}    

void con_room_leaveall(gpointer key, gpointer data, gpointer arg)
{
    cnu user = (cnu)data;
    xmlnode info = (xmlnode)arg;
    char *alt, *reason;
    xmlnode presence;
    xmlnode tag;
    xmlnode element;
    xmlnode node;
    xmlnode destroy;

    if(user == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL user attribute found", FZONE);
	return;
    }

    presence = jutil_presnew(JPACKET__UNAVAILABLE, NULL, NULL);
    tag = xmlnode_insert_tag(presence,"x");
    xmlnode_put_attrib(tag, "xmlns", NS_MUC_USER);

    element = xmlnode_insert_tag(tag, "item");
    xmlnode_put_attrib(element, "role", "none");
    xmlnode_put_attrib(element, "affiliation", "none");

    if (info != NULL)
    {
        destroy = xmlnode_insert_tag(tag, "destroy");
	reason = xmlnode_get_tag_data(info, "reason");
	node = xmlnode_insert_tag(destroy, "reason");

	if(reason != NULL)
	{
            xmlnode_insert_cdata(node, reason, -1);
	}

        alt = xmlnode_get_attrib(info, "jid");

	if (alt != NULL)
	{
	    xmlnode_put_attrib(destroy, "jid", alt);
        }
    }

    con_user_send(user, user, presence);
}

void _con_room_usernick(gpointer key, gpointer data, gpointer arg)
{
    cnu user = (cnu)data;
    xmlnode x = (xmlnode)arg;

    if(user == NULL || x == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    if(j_strcmp(xmlnode_get_data(x),xmlnode_get_data(user->nick)) == 0)
        xmlnode_put_vattrib(x, "u", (void*)user);
}

cnu con_room_usernick(cnr room, char *nick)
{
    cnu user;
    xmlnode node = xmlnode_new_tag("nick");

    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return NULL;
    }

    log_debug(NAME, "[%s] searching for nick %s in room %s", FZONE, nick, jid_full(jid_fix(room->id)));

    xmlnode_insert_cdata(node, nick, -1);
    g_hash_table_foreach(room->local, _con_room_usernick, (void *)node);

    user = (cnu)xmlnode_get_vattrib(node,"u");

    xmlnode_free(node);
    return user;
}

/* returns a valid nick from the list, x is the first <nick>foo</nick> xmlnode, checks siblings */
char *con_room_nick(cnr room, cnu user, xmlnode x)
{
    char *nick = NULL;
    xmlnode cur;
    int count = 1;

    if(room == NULL || user == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return NULL;
    }

    log_debug(NAME, "[%s] looking for valid nick in room %s from starter %s", FZONE, jid_full(jid_fix(room->id)), xmlnode2str(x));

    /* make-up-a-nick phase */
    if(x == NULL)
    {
        nick = pmalloco(user->p, j_strlen(user->realid->user) + 10);
        log_debug(NAME, "[%s] Malloc: Nick = %d", FZONE, j_strlen(user->realid->user) + 10);

        sprintf(nick, "%s", user->realid->user);
        while(con_room_usernick(room, nick) != NULL)
            sprintf(nick, "%s%d", user->realid->user,count++);
        return nick;
    }

    /* scan the list */
    for(cur = x; cur != NULL; cur = xmlnode_get_nextsibling(cur))
    {
        if(j_strcmp(xmlnode_get_name(cur),"nick") == 0 && (nick = xmlnode_get_data(cur)) != NULL)
            if(con_room_usernick(room, nick) == NULL)
                break;
    }

    if(is_registered(room->master, jid_full(jid_user(jid_fix(user->realid))), nick) == -1)
	nick = NULL;

    return nick;
}

void con_room_sendwalk(gpointer key, gpointer data, gpointer arg)
{
    xmlnode x = (xmlnode)arg;
    cnu to = (cnu)data;
    cnu from;
    xmlnode output;

    if(x == NULL || to == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    from = (cnu)xmlnode_get_vattrib(x,"cnu");

    if(j_strncmp(xmlnode_get_name(x),"presence",8) == 0)
    {
	output = add_extended_presence(from, to, x, NULL, NULL, NULL);
        con_user_send(to, from, output); 
    }
    else
    {
        con_user_send(to, from, xmlnode_dup(x)); /* Need to send duplicate */
    }
}

/* Browse for members of a room */
void con_room_browsewalk(gpointer key, gpointer data, gpointer arg)
{
    jid userjid;
    cnu user = (cnu)data;
    xmlnode q = (xmlnode)arg;
    xmlnode xml;

    if(user == NULL || q == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    xml = xmlnode_insert_tag(q, "item");
    userjid = jid_new(xmlnode_pool(xml), jid_full(user->room->id));
    jid_set(userjid, xmlnode_get_data(user->nick), JID_RESOURCE);

    xmlnode_put_attrib(xml, "category", "user");
    xmlnode_put_attrib(xml, "type", "client");
    xmlnode_put_attrib(xml, "name", xmlnode_get_data(user->nick));
    xmlnode_put_attrib(xml, "jid", jid_full(userjid));
}

void _con_room_discoinfo(cnr room, jpacket jp)
{
    xmlnode result;

    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL room attribute found", FZONE);
	return;
    }

    jutil_iqresult(jp->x);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"), "xmlns", NS_DISCO_INFO);
    jpacket_reset(jp);

    result = xmlnode_insert_tag(jp->iq,"identity");
    xmlnode_put_attrib(result, "category", "conference");
    xmlnode_put_attrib(result, "type", "text");
    xmlnode_put_attrib(result, "name", room->name);

    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq, "feature"), "var", NS_MUC);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_DISCO);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_BROWSE);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_VERSION);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_LAST);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_TIME);
    xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", NS_VCARD);

    if(j_strlen(room->secret) > 0)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_password");
    else
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_unsecure");

    if(room->public == 1)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_public");
    else
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_hidden");

    if(room->persistent == 1)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_persistent");
    else
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_temporary");

    if(room->invitation == 1)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_membersonly");
    else
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_open");

    if(room->moderated == 1)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_moderated");
    else
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_unmoderated");

    if(room->visible == 1)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_nonanonymous");
    else
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc_semianonymous");

    if(room->legacy == 1)
        xmlnode_put_attrib(xmlnode_insert_tag(jp->iq,"feature"), "var", "muc-legacy");

    deliver(dpacket_new(jp->x), NULL);
    return;
}

void _con_room_discoitem(gpointer key, gpointer data, gpointer arg)
{
    jid userjid;
    cnu user = (cnu)data;
    xmlnode query = (xmlnode)arg;
    xmlnode xml;

    if(user == NULL || query == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    xml = xmlnode_insert_tag(query, "item");

    userjid = jid_new(xmlnode_pool(xml), jid_full(user->room->id));
    jid_set(userjid, xmlnode_get_data(user->nick), JID_RESOURCE);

    xmlnode_put_attrib(xml, "jid", jid_full(userjid));
}

void con_room_outsider(cnr room, cnu from, jpacket jp)
{
    xmlnode q;
    int start;
    char nstr[10];
    time_t t;
    char *str;


    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found -%s- -%s-", FZONE, room);
	return;
    }

    log_debug(NAME, "[%s] handling request from outsider %s to room %s", FZONE, jid_full(jp->from), jid_full(room->id));

    /* any presence here is just fluff */
    if(jp->type == JPACKET_PRESENCE)
    {
	log_debug(NAME, "[%s] Dropping presence from outsider", FZONE);
        xmlnode_free(jp->x);
        return;
    }

    if(jp->type == JPACKET_MESSAGE)
    {
	log_debug(NAME, "[%s] Bouncing message from outsider", FZONE);
        jutil_error(jp->x, TERROR_FORBIDDEN);
        deliver(dpacket_new(jp->x), NULL);
        return;
    }

    /* public iq requests */
    if(jpacket_subtype(jp) == JPACKET__SET)
    {
	if(NSCHECK(jp->iq, NS_MUC_OWNER))
	{
	    log_debug(NAME, "[%s] IQ Set for owner function", FZONE);

	    if(from && is_owner(room, jp->from))
	    {       
	        xdata_room_config(room, from, room->locked, jp->x);

		jutil_iqresult(jp->x);
		deliver(dpacket_new(jp->x), NULL);
		return;
	    }   
	    else
	    {
	        log_debug(NAME, "[%s] IQ Set for owner disallowed", FZONE);
                jutil_error(jp->x, TERROR_NOTALLOWED);
                deliver(dpacket_new(jp->x), NULL);
		return;
	    }
	}
	else if(NSCHECK(jp->iq, NS_REGISTER))
	{
	    log_debug(NAME, "[%s] IQ Set for Registration function", FZONE);

	    jutil_error(jp->x, TERROR_NOTALLOWED);
	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}
    }

    if(jpacket_subtype(jp) == JPACKET__GET)
    {
	if(NSCHECK(jp->iq,NS_VERSION))
	{
	    jutil_iqresult(jp->x);
	    xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"),"xmlns",NS_VERSION);
	    jpacket_reset(jp);

	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"name"),NAME,-1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"version"),VERSION,-1);

	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}
        else if(NSCHECK(jp->iq, NS_BROWSE))
        {
            jutil_iqresult(jp->x);
            q = xmlnode_insert_tag(jp->x,"item");
            xmlnode_put_attrib(q,"category","conference");

            if(room->public && room->invitation == 0)
            {
                xmlnode_put_attrib(q,"type","public");
                g_hash_table_foreach(room->local, con_room_browsewalk, (void*)q);
            }
            else if(room->public && is_member(room, jp->from))
            {
                xmlnode_put_attrib(q,"type","public");
                g_hash_table_foreach(room->local, con_room_browsewalk, (void*)q);
            }
            else
            {
                xmlnode_put_attrib(q,"type","private");
            }

            xmlnode_put_attrib(q,"xmlns",NS_BROWSE);
            xmlnode_put_attrib(q,"name",room->name);
            xmlnode_put_attrib(q,"version",VERSION);

            xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_MUC,-1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_DISCO, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_BROWSE, -1);
	    /* xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_REGISTER, -1); */
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_VERSION, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_LAST, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_TIME, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_VCARD, -1);

            deliver(dpacket_new(jp->x), NULL);
            return;
        }
	else if(NSCHECK(jp->iq, NS_DISCO_INFO))
	{
	    log_debug(NAME, "[%s] Outside room packet - Disco Info Request", FZONE);

	    _con_room_discoinfo(room, jp);
	    return;
	}
	else if(NSCHECK(jp->iq, NS_DISCO_ITEMS))
	{
	    log_debug(NAME, "[%s] Outside room packet - Disco Items Request", FZONE);

	    jutil_iqresult(jp->x);
	    xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"), "xmlns", NS_DISCO_ITEMS);
	    jpacket_reset(jp);

	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}
        else if(NSCHECK(jp->iq, NS_LAST))
        {
            log_debug(NAME, "[%s] Outside room packet - Last Request", FZONE);
 
            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"),"xmlns",NS_LAST);
            jpacket_reset(jp);
 
            start = time(NULL) - room->start;
            sprintf(nstr,"%d",start);
            xmlnode_put_attrib(jp->iq,"seconds", pstrdup(jp->p, nstr));
 
            deliver(dpacket_new(jp->x),NULL);
	    return;
        }
        else if(NSCHECK(jp->iq,NS_TIME))
        {
            /* Compliant with JEP-0090 */
                                                                                                                  
            log_debug(NAME, "[%s] Server packet - Time Request", FZONE);
 
            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "query"), "xmlns", NS_TIME);
            jpacket_reset(jp);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "utc"), jutil_timestamp(), -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "tz"), tzname[0], -1);
 
            /* create nice display time */
            t = time(NULL);
            str = ctime(&t);
 
            str[strlen(str) - 1] = '\0'; /* cut off newline */
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "display"), pstrdup(jp->p, str), -1);
 
            free(str);
             
            deliver(dpacket_new(jp->x),NULL);
	    return;
        }
	else if(NSCHECK(jp->iq, NS_MUC_OWNER))
        {
            if(j_strcmp(xmlnode_get_name(jp->iq),"query") == 0)
            {
	        log_debug(NAME, "[%s] IQ Get for owner: configuration", FZONE);

	        if(!is_owner(room, from->realid))
	        {
		    jutil_error(jp->x, TERROR_BAD);
		    deliver(dpacket_new(jp->x), NULL);
	            return;
	        }

	        xdata_room_config(room, from, 0, jp->x);

		xmlnode_free(jp->x);
		return;
	    }
        }
	else if(NSCHECK(jp->iq, NS_REGISTER))
	{
	    log_debug(NAME, "[%s] IQ Get for Registration function", FZONE);

	    jutil_error(jp->x, TERROR_NOTALLOWED);
	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}
        else if(NSCHECK(jp->iq,NS_VCARD))
        {
            log_debug(NAME, "[%s] Outside room packet - VCard Request", FZONE);
                                                                                                                  
            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"vCard"),"xmlns",NS_VCARD);
            jpacket_reset(jp);
                                                                                                                  
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "DESC"), room->description, -1);
            deliver(dpacket_new(jp->x),NULL);
            return;
        }
    }

    log_debug(NAME, "[%s] Sending Not Implemented", FZONE);
    jutil_error(jp->x, TERROR_NOTIMPL);
    deliver(dpacket_new(jp->x), NULL);
    return;

}

void con_room_process(cnr room, cnu from, jpacket jp)
{
    char *nick = NULL;
    char *key;
    char *str;
    time_t t;
    int start;
    char nstr[10];
    xmlnode result, item, x, node;
    jid id;

    if(room == NULL || from == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    log_debug(NAME, "[%s] handling request from participant %s(%s/%s) to room %s", FZONE, jid_full(from->realid), from->localid->resource, xmlnode_get_data(from->nick), jid_full(room->id));

    /* presence is just stored and forwarded */
    if(jp->type == JPACKET_PRESENCE)
    {
        xmlnode_free(from->presence);
	from->presence = xmlnode_dup(jp->x);

	jutil_delay(from->presence, NULL);

        xmlnode_put_vattrib(jp->x, "cnu", (void*)from);
        g_hash_table_foreach(room->local, con_room_sendwalk, (void*)jp->x);

	xmlnode_free(jp->x);
        return;
    }

    if(jp->type == JPACKET_MESSAGE)
    {
	if(NSCHECK(xmlnode_get_tag(jp->x,"x"),NS_MUC_USER))
	{
	    /* Invite */
	    log_debug(NAME, "[%s] Found invite request", FZONE);

	    if(room->invitation == 1 && room->invites == 0 && !is_admin(room, from->realid))
	    {
	        log_debug(NAME, "[%s] Forbidden invitation request, returning error", FZONE);

	        jutil_error(jp->x,TERROR_FORBIDDEN);
		deliver(dpacket_new(jp->x),NULL);
		return;
	    }

	    item = xmlnode_dup(xmlnode_get_tag(jp->x,"x"));
	    nick = xmlnode_get_attrib(xmlnode_get_tag(item, "invite"), "to");

	    if( nick == NULL)
	    {
	        log_debug(NAME, "[%s] No receipient, returning error", FZONE);

	        jutil_error(jp->x,TERROR_BAD);
		deliver(dpacket_new(jp->x),NULL);

                xmlnode_free(item);
		return;
	    }

	    if(room->invitation == 1)
	    {
	        id = jid_new(xmlnode_pool(item), nick);

	        key = j_strdup(jid_full(jid_user(jid_fix(id))));
	        g_hash_table_insert(room->member, key, (void*)item);
	    }
            else
            {
                xmlnode_free(item);
            }

	    con_room_send_invite(from, xmlnode_get_tag(jp->x,"x"));
	    return;
	}

        /* if topic, store it */
        if((x = xmlnode_get_tag(jp->x,"subject")) != NULL )
        {

            /* catch the invite hack */
            if((nick = xmlnode_get_data(x)) != NULL && j_strncasecmp(nick,"invite:",7) == 0)
            {
                nick += 7;
                if((jp->to = jid_new(jp->p, nick)) == NULL)
                {
                    jutil_error(jp->x,TERROR_BAD);
                }else{
                    xmlnode_put_attrib(jp->x, "to", jid_full(jp->to));
                    jp->from = jid_new(jp->p, jid_full(jid_user(from->localid)));
                    jid_set(jp->from, xmlnode_get_data(from->nick), JID_RESOURCE);
                    xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));
                }
                deliver(dpacket_new(jp->x), NULL);
                return;
            }

	    /* Disallow subject change if user does not have high enough level */
	    if((!is_admin(room, from->realid) && room->subjectlock == 0) || is_visitor(room, from->realid) )
	    {
		jutil_error(jp->x,TERROR_FORBIDDEN);
		deliver(dpacket_new(jp->x),NULL);
		return;
	    }

	    /* Save room topic for new users */
            xmlnode_free(room->topic);
            room->topic = xmlnode_new_tag("topic");
            xmlnode_put_attrib(room->topic, "subject", xmlnode_get_data(x));
            xmlnode_insert_cdata(room->topic, xmlnode_get_data(from->nick), -1);
            xmlnode_insert_cdata(room->topic, " has set the topic to: ", -1);
            xmlnode_insert_cdata(room->topic, xmlnode_get_data(x), -1);

	    /* Save the room topic if room is persistent */
	    if(room->persistent == 1)
	    {
	        xdb_room_set(room);
	    }
        }

	/* Check if allowed to talk */
	if(room->moderated == 1 && !is_participant(room, from->realid))
	{
	    /* Attempting to talk in a moderated room without voice intrinsic */
            jutil_error(jp->x,TERROR_MUC_VOICE);
            deliver(dpacket_new(jp->x),NULL);
            return;
        }

        if(jp->subtype != JPACKET__GROUPCHAT)
        {                                   
            jutil_error(jp->x, TERROR_BAD);                                    
            deliver(dpacket_new(jp->x), NULL);                                 
            return;                            
        }                                                                     

        /* ensure type="groupchat" */
        xmlnode_put_attrib(jp->x,"type","groupchat");

	/* Save copy of packet for history */
	node = xmlnode_dup(jp->x);

        /* broadcast */
        xmlnode_put_vattrib(jp->x,"cnu",(void*)from);
        g_hash_table_foreach(room->local, con_room_sendwalk, (void*)jp->x);

        /* log */
        con_room_log(room, xmlnode_get_data(from->nick), xmlnode_get_tag_data(jp->x, "body"));

	/* Save from address */
	id = jid_new(xmlnode_pool(node), jid_full(from->localid));
	jid_set(id, xmlnode_get_data(from->nick), JID_RESOURCE);
	xmlnode_put_attrib(node, "from", jid_full(id));

        /* store in history */
        jutil_delay(node, jid_full(room->id));

        if(room->master->history > 0)
        {
            if(++room->hlast == room->master->history)
                room->hlast = 0;
            xmlnode_free(room->history[room->hlast]);
            room->history[room->hlast] = node;
        }
	else
	{
	    xmlnode_free(node);
	}
            
	xmlnode_free(jp->x);
        return;
    }

    /* public iq requests */

    if(jpacket_subtype(jp) == JPACKET__SET)
    {
	if(NSCHECK(jp->iq, NS_MUC_ADMIN))
	{ 
	    log_debug(NAME, "[%s] IQ Set for admin function: >%s<", FZONE, xmlnode_get_name(jp->iq));

	    if(!is_moderator(room, from->realid))
	    {
	        jutil_error(jp->x, TERROR_FORBIDDEN);
	        deliver(dpacket_new(jp->x), NULL);
	        return;
	    }

            if(j_strcmp(xmlnode_get_name(jp->iq), "query") == 0)
	    {
	        log_debug(NAME, "[%s] List set requested by admin...", FZONE);

		result = xmlnode_get_tag(jp->x, "query");

		if(NSCHECK(xmlnode_get_tag(result,"x"),NS_DATA))
		{
		    log_debug(NAME, "[%s] Received x:data", FZONE);
	            jutil_error(jp->x, TERROR_BAD);
		    deliver(dpacket_new(jp->x), NULL);
		    return;
		}
		else
		{
	            con_parse_item(from, jp);
		    return;
		}
            }
	}
	else if(NSCHECK(jp->iq, NS_MUC_OWNER))
	{
	    if(!is_owner(room, from->realid))
	    {   
		jutil_error(jp->x, TERROR_NOTALLOWED);
		deliver(dpacket_new(jp->x), NULL);
		return;
	    }

	    if(j_strcmp(xmlnode_get_name(jp->iq),"query") == 0)
            {

		result = xmlnode_get_tag(jp->x, "query");
		node = xmlnode_get_tag(result, "destroy");

		if(node)
		{
		    log_debug(NAME, "[%s] IQ Set for owner: destroy requested", FZONE);
		    /* Remove old xdb registration */
		    if(room->persistent == 1)
		    {
	                xdb_room_clear(room);
		    }

		    /* inform everyone they have left the room */
		    g_hash_table_foreach(room->remote, con_room_leaveall, node);

	            con_room_zap(room);
	            jutil_iqresult(jp->x);
		    deliver(dpacket_new(jp->x), NULL);
		    return;
		}
		else if(NSCHECK(xmlnode_get_tag(result,"x"),NS_DATA))
		{
		    log_debug(NAME, "[%s] Received x:data", FZONE);
		    xdata_handler(room, from, jp);

		    jutil_iqresult(jp->x);
		    deliver(dpacket_new(jp->x), NULL);
		    return;
		}
		else
		{
	            log_debug(NAME, "[%s] IQ Set for owner: configuration set", FZONE);
	            con_parse_item(from, jp);
		    return;
		}
	    }
	    else
            {
	        jutil_error(jp->x, TERROR_BAD);
	        deliver(dpacket_new(jp->x), NULL);
	        return;
	    }
	}
	else if(NSCHECK(jp->iq, NS_REGISTER))
	{
	    log_debug(NAME, "[%s] IQ Set for Registration function", FZONE);

	    jutil_error(jp->x, TERROR_NOTALLOWED);
	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}

        jutil_error(jp->x, TERROR_BAD);
        deliver(dpacket_new(jp->x), NULL);
        return;
    }

    if(jpacket_subtype(jp) == JPACKET__GET)
    {
	if(NSCHECK(jp->iq,NS_VERSION))
	{
	    jutil_iqresult(jp->x);
	    xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"),"xmlns",NS_VERSION);
	    jpacket_reset(jp);

	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"name"),NAME,-1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq,"version"),VERSION,-1);

	    deliver(dpacket_new(jp->x),NULL);
	    return;
	}
        else if(NSCHECK(jp->iq, NS_BROWSE))
        {
            jutil_iqresult(jp->x);
            result = xmlnode_insert_tag(jp->x,"item");
            xmlnode_put_attrib(result,"category", "conference");
            xmlnode_put_attrib(result,"xmlns", NS_BROWSE);
            xmlnode_put_attrib(result,"name", room->name);

            if(room->public)
            {
                xmlnode_put_attrib(result,"type", "public");
            }
            else
            {
                xmlnode_put_attrib(result,"type", "private");
            }

            xmlnode_insert_cdata(xmlnode_insert_tag(result,"ns"),NS_MUC,-1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_DISCO, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_BROWSE, -1);
	    /* xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_REGISTER, -1); */
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_VERSION, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_LAST, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_TIME, -1);
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "ns"), NS_VCARD, -1);

            g_hash_table_foreach(room->local, con_room_browsewalk, (void*)result);
            deliver(dpacket_new(jp->x), NULL);
            return;
        }
	else if(NSCHECK(jp->iq, NS_DISCO_INFO))
	{
	    log_debug(NAME, "[%s] room packet - Disco Info Request", FZONE);

	    _con_room_discoinfo(room, jp);
	    return;
	}
	else if(NSCHECK(jp->iq, NS_DISCO_ITEMS))
	{
	    log_debug(NAME, "[%s] room packet - Disco Items Request", FZONE);

	    jutil_iqresult(jp->x);
            result = xmlnode_insert_tag(jp->x, "query");
            xmlnode_put_attrib(result, "xmlns", NS_DISCO_ITEMS);

            g_hash_table_foreach(room->local, _con_room_discoitem, (void*)result);

	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}
        else if(NSCHECK(jp->iq, NS_LAST))
        {
            log_debug(NAME, "[%s] room packet - Last Request", FZONE);
                                                                                                                  
            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x,"query"),"xmlns", NS_LAST);
            jpacket_reset(jp);
                                                                                                                  
            start = time(NULL) - room->start;
            sprintf(nstr,"%d",start);
            xmlnode_put_attrib(jp->iq,"seconds", pstrdup(jp->p, nstr));
                                                                                                                  
            deliver(dpacket_new(jp->x),NULL);
	    return;
        }
        else if(NSCHECK(jp->iq,NS_TIME))
        {
            /* Compliant with JEP-0090 */
                                                                                                                  
            log_debug(NAME, "[%s] Server packet - Time Request", FZONE);
 
            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "query"), "xmlns", NS_TIME);
            jpacket_reset(jp);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "utc"), jutil_timestamp(), -1);
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "tz"), tzname[0], -1);
 
            /* create nice display time */
            t = time(NULL);
            str = ctime(&t);
 
            str[strlen(str) - 1] = '\0'; /* cut off newline */
            xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "display"), pstrdup(jp->p, str), -1);
 
            deliver(dpacket_new(jp->x),NULL);
            return;
        }
	else if(NSCHECK(jp->iq, NS_MUC_ADMIN))
        {
	    log_debug(NAME, "[%s] IQ Get for admin function, %s", FZONE, xmlnode_get_name(jp->iq));

	    if(!is_moderator(room, from->realid))
	    {
	        jutil_error(jp->x, TERROR_FORBIDDEN);
	        deliver(dpacket_new(jp->x), NULL);
	        return;
	    }

	    if(j_strcmp(xmlnode_get_name(jp->iq),"query") == 0)
            {
	        con_parse_item(from, jp);
	        return;
	    }
        }
	else if(NSCHECK(jp->iq, NS_MUC_OWNER))
        {
	    log_debug(NAME, "[%s] IQ Get for owner function, %s", FZONE, xmlnode_get_name(jp->iq));

	    if(!is_owner(room, from->realid))
	    {
	        jutil_error(jp->x, TERROR_FORBIDDEN);
	        deliver(dpacket_new(jp->x), NULL);
	        return;
	    }

	    if(j_strcmp(xmlnode_get_name(jp->iq),"query") == 0)
            {
	        con_parse_item(from, jp);
	        return;
	    }
        }
	else if(NSCHECK(jp->iq, NS_REGISTER))
	{
	    log_debug(NAME, "[%s] IQ Get for Registration function", FZONE);

	    jutil_error(jp->x, TERROR_NOTALLOWED);
	    deliver(dpacket_new(jp->x), NULL);
	    return;
	}
        else if(NSCHECK(jp->iq,NS_VCARD))
        {
            log_debug(NAME, "[%s] room packet - VCard Request", FZONE);
                                                                                                                  
            jutil_iqresult(jp->x);
            xmlnode_put_attrib(xmlnode_insert_tag(jp->x, "vCard"), "xmlns", NS_VCARD);
            jpacket_reset(jp);
                                                                                                                  
	    xmlnode_insert_cdata(xmlnode_insert_tag(jp->iq, "DESC"), room->description, -1);
            deliver(dpacket_new(jp->x),NULL);
            return;
        }
    }

    log_debug(NAME, "[%s] Sending Bad Request", FZONE);
    jutil_error(jp->x, TERROR_BAD);
    deliver(dpacket_new(jp->x), NULL);
    return;

}

cnr con_room_new(cni master, jid roomid, jid owner, char *name, char *secret, int private, int xdata, int persist)
{
    cnr room;
    pool p;
    cnu admin;
    char *key;
    time_t now = time(NULL);

    /* Create pool for room struct */
    p = pool_new(); 
    room = pmalloco(p, sizeof(_cnr));
    log_debug(NAME, "[%s] Malloc: _cnr = %d", FZONE, sizeof(_cnr));
    room->p = p;
    room->master = master;

    /* room jid */
    room->id = jid_new(p, jid_full(jid_fix(roomid)));

    /* room natural language name for browse/disco */
    if(name)
	room->name = j_strdup(name);
    else
        room->name = j_strdup(room->id->user);

    /* room password */
    room->secret = j_strdup(secret);
    room->private = private;

    /* Initialise room history */
    room->history = pmalloco(p, sizeof(xmlnode) * master->history); /* make array of xmlnodes */
    log_debug(NAME, "[%s] Malloc: history = %d", FZONE, sizeof(xmlnode) * master->history);

    /* Room time */
    room->start = now;
    room->created = now;

    /* user hashes */
    room->remote = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_cnu);
    room->local = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, NULL);
    room->roster = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_xmlnode);

    /* Affiliated hashes */
    room->owner = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_xmlnode);
    room->admin = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_xmlnode);
    room->member = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_xmlnode);
    room->outcast = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, ght_remove_xmlnode);

    /* Role hashes */
    room->moderator = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, NULL);
    room->participant = g_hash_table_new_full(g_str_hash,g_str_equal, ght_remove_key, NULL);

    /* Room messages */
    room->note_leave = j_strdup(xmlnode_get_tag_data(master->config,"notice/leave"));
    room->note_join = j_strdup(xmlnode_get_tag_data(master->config,"notice/join"));
    room->note_rename = j_strdup(xmlnode_get_tag_data(master->config,"notice/rename"));

    /* Room Defaults */
    room->public = master->public;
    room->subjectlock = 0;
    room->maxusers = 0;
    room->persistent = persist;
    room->moderated = 0;
    room->defaulttype = 0;
    room->privmsg = 0;
    room->invitation = 0;
    room->invites = 0;
    room->legacy = 0;
    room->visible = 1;
    room->logfile = NULL;
    room->logformat = LOG_TEXT;
    room->description = j_strdup(room->name);

    /* Assign owner to room */
    if(owner != NULL)
    {
        admin = (void*)con_user_new(room, owner);
	add_roster(room, admin->realid);

	room->creator = jid_new(room->p, jid_full(jid_user(admin->realid)));

	add_affiliate(room->owner, admin->realid, NULL);

	if(xdata > 0 )
		xdata_room_config(room,admin,1,NULL);

	log_debug(NAME, "[%s] Added new admin: %s to room %s", FZONE, jid_full(jid_fix(owner)), jid_full(room->id));
    }

    key = j_strdup(jid_full(room->id));
    g_hash_table_insert(master->rooms, key, (void*)room);

    log_debug(NAME,"[%s] new room %s (%s/%s/%d)", FZONE, jid_full(room->id),name,secret,private);

    /* Save room configuration if persistent */
    if(room->persistent == 1)
    {
	xdb_room_set(room);
    }

    return room;
}

void _con_room_send(gpointer key, gpointer data, gpointer arg)
{
    cnu user = (cnu)data;
    xmlnode x = (xmlnode)arg;
    xmlnode output;

    if(user == NULL || x == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    output = xmlnode_dup((xmlnode)x);

    xmlnode_put_attrib(output, "to", jid_full(user->realid));
    deliver(dpacket_new(output), NULL);
    return;
}

void _con_room_send_legacy(gpointer key, gpointer data, gpointer arg)
{
    cnu user = (cnu)data;
    xmlnode x = (xmlnode)arg;
    xmlnode output;

    if(user == NULL || x == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    output = xmlnode_dup((xmlnode)x);

    if(!is_legacy(user))
    {
        xmlnode_free(output);
        return;
    }

    xmlnode_put_attrib(output, "to", jid_full(user->realid));
    deliver(dpacket_new(output), NULL);
    return;
}


void con_room_send(cnr room, xmlnode x, int legacy)
{
    if(room == NULL || x == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL attribute found", FZONE);
	return;
    }

    log_debug(NAME,"[%s] Sending packet from room %s: %s", FZONE, jid_full(room->id), xmlnode2str(x));

    /* log */
    con_room_log(room, NULL, xmlnode_get_tag_data(x, "body"));

    xmlnode_put_attrib(x, "from", jid_full(room->id));

    deliver__flag = 0;

    if(legacy)
        g_hash_table_foreach(room->local, _con_room_send_legacy, (void*)x);
    else
        g_hash_table_foreach(room->local, _con_room_send, (void*)x);
    deliver__flag = 1;
    deliver(NULL, NULL);

    xmlnode_free(x);
    return;
}

/* Clear up room hashes */
void con_room_cleanup(cnr room)
{
    char *roomid;

    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL room attribute found", FZONE);
	return;
    }

    roomid = j_strdup(jid_full(room->id));

    log_debug(NAME, "[%s] cleaning room %s", FZONE, roomid);

    /* Clear old hashes */
    log_debug(NAME, "[%s] zapping list remote in room %s", FZONE, roomid);
    g_hash_table_destroy(room->remote);

    log_debug(NAME, "[%s] zapping list local in room %s", FZONE, roomid);
    g_hash_table_destroy(room->local);

    log_debug(NAME, "[%s] zapping list roster in room %s", FZONE, roomid);
    g_hash_table_destroy(room->roster);

    log_debug(NAME, "[%s] zapping list owner in room %s", FZONE, roomid);
    g_hash_table_destroy(room->owner);

    log_debug(NAME, "[%s] zapping list admin in room %s", FZONE, roomid);
    g_hash_table_destroy(room->admin);

    log_debug(NAME, "[%s] zapping list member in room %s", FZONE, roomid);
    g_hash_table_destroy(room->member);

    log_debug(NAME, "[%s] zapping list outcast in room %s", FZONE, roomid);
    g_hash_table_destroy(room->outcast);

    log_debug(NAME, "[%s] zapping list moderator in room %s", FZONE, roomid);
    g_hash_table_destroy(room->moderator);

    log_debug(NAME, "[%s] zapping list participant in room %s", FZONE, roomid);
    g_hash_table_destroy(room->participant);

    log_debug(NAME, "[%s] closing room log in room %s", FZONE, roomid);
    if(room->logfile && room->logfile != NULL)
	fclose(room->logfile);

    log_debug(NAME, "[%s] Clearing any history in room %s", FZONE, roomid);
    con_room_history_clear(room);

    log_debug(NAME, "[%s] Clearing topic in room %s", FZONE, roomid);
    xmlnode_free(room->topic);

    log_debug(NAME, "[%s] Clearing strings and legacy messages in room %s", FZONE, roomid);
    free(room->name);
    free(room->description);
    free(room->secret);
    free(room->note_join);
    free(room->note_rename);
    free(room->note_leave);

    free(roomid);
    return;
}

/* Zap room entry */
void con_room_zap(cnr room)
{
    if(room == NULL) 
    {
	log_warn(NAME, "[%s] Aborting - NULL room attribute found", FZONE);
	return;
    }

    log_debug(NAME, "[%s] cleaning up room %s", FZONE, jid_full(room->id));

    con_room_cleanup(room);

    log_debug(NAME, "[%s] zapping room %s from list", FZONE, jid_full(room->id));
    
    g_hash_table_remove(room->master->rooms, jid_full(room->id));

    return;
}

void con_room_history_clear(cnr room)
{
    int h;

    if(room->master->history > 0)
    {
                                                                                                                  
       h = room->hlast;
       while(1)
       {
          h++;
                                                                                                                  
          if(h == room->master->history)
              h = 0;
                                   
          xmlnode_free(room->history[h]);
                                                                                                                  
          if(h == room->hlast)
              break;
       }
    }

}
