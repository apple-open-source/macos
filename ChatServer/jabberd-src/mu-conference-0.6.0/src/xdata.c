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

xmlnode add_xdata_boolean(char *label, char *var, int data)
{
    xmlnode node;
    char value[4];

    snprintf(value, 4, "%i", data);
    node = xmlnode_new_tag("field");
    xmlnode_put_attrib(node,"type","boolean");
    xmlnode_put_attrib(node,"label", label);
    xmlnode_put_attrib(node,"var", var);
    xmlnode_insert_cdata(xmlnode_insert_tag(node,"value"), value, -1);

    return node;
}

xmlnode add_xdata_text(char *label, int type, char *var, char *data)
{
    xmlnode node;

    node = xmlnode_new_tag("field");

    if(type > 1)
	    xmlnode_put_attrib(node,"type","text-multi");
    else if(type == 1)
	    xmlnode_put_attrib(node,"type","text-single");
    else if(type == -1)
	    xmlnode_put_attrib(node,"type","text-private");
    else
	    xmlnode_put_attrib(node,"type","hidden");

    if(label != NULL)
	    xmlnode_put_attrib(node,"label", label);

    xmlnode_put_attrib(node,"var", var);
    xmlnode_insert_cdata(xmlnode_insert_tag(node,"value"), data, -1);

    return node;
}

xmlnode add_xdata_desc(char *label)
{
    xmlnode node;

    node = xmlnode_new_tag("field");
    xmlnode_put_attrib(node,"type","fixed");
    xmlnode_insert_cdata(xmlnode_insert_tag(node,"value"), label, -1);

    return node;
}

void xdata_handler(cnr room, cnu user, jpacket packet)
{
    xmlnode results, element, value, current, node, message;

    pool tp = pool_new();
    spool sp = spool_new(tp);
    int visible = room->visible;
    char namespace[100];
    
    log_debug(NAME, "[%s] xdata handler", FZONE);
    results = xmlnode_get_tag(packet->x,"x");

    /* Can't find xdata - trying NS_MUC_ADMIN namespace */
    if(results == NULL)
    {
        snprintf(namespace, 100, "?xmlns=%s", NS_MUC_ADMIN);
        element = xmlnode_get_tag(packet->x, namespace);
	results = xmlnode_get_tag(element,"x");
    }

    /* Still no data, try NS_MUC_OWNER namespace */
    if(results == NULL)
    {
        snprintf(namespace, 100, "?xmlns=%s", NS_MUC_OWNER);
        element = xmlnode_get_tag(packet->x, namespace);
	results = xmlnode_get_tag(element,"x");
    }

    /* Still no data, try NS_MUC_USER namespace */
    if(results == NULL)
    {
        snprintf(namespace, 100, "?xmlns=%s", NS_MUC_USER);
        element = xmlnode_get_tag(packet->x, namespace);
	results = xmlnode_get_tag(element,"x");
    }

    /* Still no xdata, just leave */
    if(results == NULL)
    {
	log_debug(NAME, "[%s] No xdata results found", FZONE);
	pool_free(tp);
	return;
    }

    if(j_strcmp(xmlnode_get_attrib(results, "type"), "cancel") == 0)
    {
	log_debug(NAME, "[%s] xdata form was cancelled", FZONE);

	/* If form cancelled and room locked, this is declaration of room destroy request */
        if(room->locked == 1)
        {
	    if(room->persistent == 1)
	        xdb_room_clear(room);

	    g_hash_table_foreach(room->remote, con_room_leaveall, (void*)NULL);
            con_room_zap(room);
        }
	pool_free(tp);
        return;
    }

    value = xmlnode_get_tag(results,"?var=form");
    log_debug(NAME, "[%s] Form type: %s", FZONE, xmlnode_get_tag_data(value,"value"));

    if(is_admin(room, user->realid))
    {
        log_debug(NAME, "[%s] Processing configuration form", FZONE);

	/* Clear any room locks */
        if(room->locked == 1)
        {
	    message = jutil_msgnew("groupchat", jid_full(jid_fix(user->realid)), NULL, spools(packet->p, "Configuration confirmed: This room is now unlocked.", packet->p));
	    xmlnode_put_attrib(message,"from", jid_full(jid_fix(room->id)));
	    deliver(dpacket_new(message), NULL);

	    room->locked = 0;
	}

	/* Protect text forms from broken clients */
	if(xmlnode_get_tag(results,"?var=muc#owner_roomname") != NULL)
	{
	    free(room->name);
  	    room->name = j_strdup(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_roomname"),"value"));
	}
	if(xmlnode_get_tag(results,"?var=leave") != NULL)
	{
	    free(room->note_leave);
	    room->note_leave = j_strdup(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=leave"),"value"));
	}
	if(xmlnode_get_tag(results,"?var=join") != NULL)
	{
	    free(room->note_join);
	    room->note_join = j_strdup(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=join"),"value"));
	}
	if(xmlnode_get_tag(results,"?var=rename") != NULL)
	{
	    free(room->note_rename);
	    room->note_rename = j_strdup(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=rename"),"value"));
	}

	/* Handle text-multi */
	if((node = xmlnode_get_tag(results,"?var=muc#owner_roomdesc")) != NULL)
	{
            for(current = xmlnode_get_firstchild(node); current != NULL; current = xmlnode_get_nextsibling(current))
	    {
		spooler(sp, xmlnode_get_data(current), sp);
	    }

            free(room->description);
	    room->description = j_strdup(spool_print(sp));
	}

	/* Update with results from form if available. If unable, simply use the original value */
	room->subjectlock = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_changesubject"),"value"), room->subjectlock);
	room->private = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=privacy"),"value"),room->private);
	room->public = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_publicroom"),"value"),room->public);
	room->maxusers = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_maxusers"),"value"),room->maxusers);

	if(room->master->dynamic == 0 || is_sadmin(room->master, user->realid))
	    room->persistent = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_persistentroom"),"value"),room->persistent);

	room->moderated = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_moderatedroom"),"value"),room->moderated);
	room->defaulttype = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=defaulttype"),"value"),room->defaulttype);
	room->privmsg = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=privmsg"),"value"),room->privmsg);
	room->invitation = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_inviteonly"),"value"),room->invitation);
	room->invites = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_allowinvites"),"value"),room->invites);
	room->legacy = j_atoi(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=legacy"),"value"),room->legacy);

	/* Protection against broken clients */
	if(xmlnode_get_tag(results,"?var=muc#owner_passwordprotectedroom") != NULL && xmlnode_get_tag(results,"?var=muc#owner_roomsecret") != NULL)
	{
	    /* Is both password set and active? */
	    if(j_strcmp(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_passwordprotectedroom"),"value"), "1") == 0 && xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_roomsecret"),"value") != NULL)
	    {
		free(room->secret);
		room->secret = j_strdup(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_roomsecret"),"value"));
		log_debug(NAME ,"[%s] Switching on room password: %s", FZONE, room->secret);
	    }
	    else 
	    {
		log_debug(NAME, "[%s] Deactivating room password: %s %s", FZONE, xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_passwordprotectedroom"),"value"), xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_roomsecret"),"value"));
		free(room->secret);
		room->secret = NULL;
	    }
	}

	if(j_strcmp(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_whois"),"value"), "anyone") == 0)
		room->visible = 1;
	else
		room->visible = 0;

	/* Send Status Alert */
	if(room->visible == 1 && room->visible != visible)
		con_send_room_status(room, STATUS_MUC_SHOWN_JID);
	else if(room->visible == 0 && room->visible != visible)
		con_send_room_status(room, STATUS_MUC_HIDDEN_JID);

	/* Set up log format and restart logging if necessary */
	if(xmlnode_get_tag(results,"?var=logformat"))
	{
	    if(j_strcmp(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=logformat"),"value"), "xml") == 0)
	    {
		if(room->logfile != NULL && room->logformat != LOG_XML)
		{
		    fclose(room->logfile);
                    room->logformat = LOG_XML;
		    con_room_log_new(room);
		}
		else
		{
		    room->logformat = LOG_XML;
		}
	    }
	    else if(j_strcmp(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=logformat"),"value"), "xhtml") == 0)
	    {
		if(room->logfile != NULL && room->logformat != LOG_XHTML)
		{
		    fclose(room->logfile);
                    room->logformat = LOG_XHTML;
		    con_room_log_new(room);
		}
		else
		{
		    room->logformat = LOG_XHTML;
		}
	    }
	    else
	    {
		if(room->logfile != NULL && room->logformat != LOG_TEXT)
		{
		    fclose(room->logfile);
                    room->logformat = LOG_TEXT;
		    con_room_log_new(room);
		}
		else
		{
		    room->logformat = LOG_TEXT;
		}
	    }
	}

	/* Set up room logging */
	if(room->logfile == NULL && j_strcmp(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_enablelogging"),"value"),"1") == 0)
	{

            con_room_log_new(room);

	    if (room->logfile == NULL)
	        log_alert(NULL, "cannot open log file for room %s", jid_full(jid_fix(room->id)));
	    else
		con_room_log(room, NULL, "LOGGING STARTED");

	}

	if(room->logfile != NULL && j_strcmp(xmlnode_get_tag_data(xmlnode_get_tag(results,"?var=muc#owner_enablelogging"),"value"),"0") == 0)
	{
	    con_room_log(room, NULL, "LOGGING STOPPED");
	    con_room_log_close(room);
	}

	if(room->persistent == 1)
	{
	    xdb_room_set(room);
	}
	else
	{
	    xdb_room_clear(room);
	}
    }
    pool_free(tp);
}

void xdata_room_config(cnr room, cnu user, int new, xmlnode query)
{
    xmlnode msg, iq, element, field, x;
    char value[4];
   
    if(user == NULL)
    {
        log_warn(NAME, "[%s] NULL attribute found", FZONE);
        return;
    }

    log_debug(NAME, "[%s] Configuration form requested by %s", FZONE, jid_full(jid_fix(user->realid)));
    
    if(!is_owner(room, user->realid))
    {
        log_debug(NAME, "[%s] Configuration form request denied", FZONE);
        
        if(query != NULL)
        {
            jutil_error(query,TERROR_MUC_CONFIG);
            deliver(dpacket_new(query),NULL);
        }   
        
        return;
    }   
   
    /* Lock room for IQ Registration method. Will release lock when config received */
    if(new == 1)
         room->locked = 1;

    /* Catchall code, for creating a standalone form */
    if( query == NULL )
    {
        msg = xmlnode_new_tag("message");
        xmlnode_put_attrib(msg, "to", jid_full(jid_fix(user->realid)));
        xmlnode_put_attrib(msg,"from",jid_full(jid_fix(room->id)));
        xmlnode_put_attrib(msg,"type","normal");
        
        xmlnode_insert_cdata(xmlnode_insert_tag(msg,"subject"),"Please setup your room",-1);
        
        element = xmlnode_insert_tag(msg,"body");
        xmlnode_insert_cdata(element,"Channel ",-1);
        xmlnode_insert_cdata(element,room->id->user,-1);
        
        if(new == 1)
                xmlnode_insert_cdata(element," has been created",-1);
        else    
                xmlnode_insert_cdata(element," configuration setting",-1);

        x = xmlnode_insert_tag(msg,"x");
    }           
    else
    {
        msg = xmlnode_dup(query);
        jutil_iqresult(msg);
	iq = xmlnode_insert_tag(msg,"query");
	xmlnode_put_attrib(iq, "xmlns", NS_MUC_OWNER);
    
	x = xmlnode_insert_tag(iq,"x");
    }

    xmlnode_put_attrib(x,"xmlns",NS_DATA);
    xmlnode_put_attrib(x,"type","form");
    xmlnode_insert_cdata(xmlnode_insert_tag(x,"title"),"Room configuration",-1);

    if(new == 1)
    {
        field = xmlnode_insert_tag(x,"instructions");
        xmlnode_insert_cdata(field,"Your room \"",-1);
        xmlnode_insert_cdata(field,room->id->user,-1);
        xmlnode_insert_cdata(field,"\" has been created! The default configuration is as follows:\n", -1);

	if(room->logfile == NULL)
		xmlnode_insert_cdata(field,"- No logging\n", -1);
        else
		xmlnode_insert_cdata(field,"- logging\n", -1);

	if(room->moderated == 1)
		xmlnode_insert_cdata(field,"- Room moderation\n", -1);
	else
		xmlnode_insert_cdata(field,"- No moderation\n", -1);

	if(room->maxusers > 0)
	{
	    snprintf(value, 4, "%i", room->maxusers);
	    xmlnode_insert_cdata(field,"- Up to ", -1);
	    xmlnode_insert_cdata(field, value, -1); 
	    xmlnode_insert_cdata(field, " participants\n", -1);
	}
	else
	{
	    xmlnode_insert_cdata(field,"- Unlimited room size\n", -1);
	}

	if(room->secret == NULL)
		xmlnode_insert_cdata(field,"- No password required\n", -1);
	else
		xmlnode_insert_cdata(field,"- Password required\n", -1);

	if(room->invitation == 0)
		xmlnode_insert_cdata(field,"- No invitation required\n", -1);
	else
		xmlnode_insert_cdata(field,"- Invitation required\n", -1);

	if(room->persistent == 0)
		xmlnode_insert_cdata(field,"- Room is not persistent\n", -1);
	else
		xmlnode_insert_cdata(field,"- Room is persistent\n", -1);

	if(room->subjectlock == 0)
		xmlnode_insert_cdata(field,"- Only admins may change the subject\n", -1);
	else
		xmlnode_insert_cdata(field,"- Anyone may change the subject\n", -1);

        xmlnode_insert_cdata(field,"To accept the default configuration, click OK. To select a different configuration, please complete this form", -1);
    }
    else
	    xmlnode_insert_cdata(xmlnode_insert_tag(x,"instructions"),"Complete this form to make changes to the configuration of your room.",-1);

    xmlnode_insert_node(x,add_xdata_text(NULL, 0, "form", "config"));
    xmlnode_insert_node(x,add_xdata_text("Natural-Language Room Name", 1, "muc#owner_roomname", room->name));
    xmlnode_insert_node(x,add_xdata_text("Short Description of Room", 2, "muc#owner_roomdesc", room->description));

    xmlnode_insert_node(x,add_xdata_desc("The following messages are sent to legacy clients."));
    xmlnode_insert_node(x,add_xdata_text("Message for user leaving room", 1, "leave", room->note_leave));
    xmlnode_insert_node(x,add_xdata_text("Message for user joining room", 1, "join", room->note_join));
    xmlnode_insert_node(x,add_xdata_text("Message for user renaming nickname in room", 1, "rename", room->note_rename));

    xmlnode_insert_node(x,add_xdata_boolean("Allow Occupants to Change Subject", "muc#owner_changesubject", room->subjectlock));

    field = xmlnode_insert_tag(x,"field");
    xmlnode_put_attrib(field,"type","list-single");
    xmlnode_put_attrib(field,"label","Maximum Number of Room Occupants");
    xmlnode_put_attrib(field,"var","muc#owner_maxusers");
    snprintf(value, 4, "%i", room->maxusers);
    xmlnode_insert_cdata(xmlnode_insert_tag(field, "value"), value, -1);

    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "1");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "1", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "10");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "10", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "20");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "20", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "30");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "30", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "40");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "40", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "50");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "50", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "None");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "0", -1);

    xmlnode_insert_node(x,add_xdata_boolean("Allow Occupants to query other Occupants?", "privacy", room->private));
    xmlnode_insert_node(x,add_xdata_boolean("Allow Public Searching for Room", "muc#owner_publicroom", room->public));

    if(room->master->dynamic == 0 || is_sadmin(room->master, user->realid))
        xmlnode_insert_node(x,add_xdata_boolean("Make Room Persistent", "muc#owner_persistentroom", room->persistent));

    xmlnode_insert_node(x,add_xdata_boolean("Consider all Clients as Legacy (shown messages)", "legacy", room->legacy));
    xmlnode_insert_node(x,add_xdata_boolean("Make Room Moderated", "muc#owner_moderatedroom", room->moderated));
    xmlnode_insert_node(x,add_xdata_desc("By default, new users entering a moderated room are only visitors"));
    xmlnode_insert_node(x,add_xdata_boolean("Make Occupants in a Moderated Room Default to Participant", "defaulttype", room->defaulttype));
    xmlnode_insert_node(x,add_xdata_boolean("Ban Private Messages between Occupants", "privmsg", room->privmsg));
    xmlnode_insert_node(x,add_xdata_boolean("An Invitation is Required to Enter", "muc#owner_inviteonly", room->invitation));
    xmlnode_insert_node(x,add_xdata_desc("By default, only admins can send invites in an invite-only room"));
    xmlnode_insert_node(x,add_xdata_boolean("Allow Occupants to Invite Others", "muc#owner_allowinvites", room->invites));

    if(room->secret == NULL)
            xmlnode_insert_node(x,add_xdata_boolean("A Password is required to enter?", "muc#owner_passwordprotectedroom", 0));
    else
            xmlnode_insert_node(x,add_xdata_boolean("A Password required to enter", "muc#owner_passwordprotectedroom", 1));

    xmlnode_insert_node(x,add_xdata_desc("If a password is required to enter this room, you must specify the password below."));
    xmlnode_insert_node(x,add_xdata_text("The Room Password", -1, "muc#owner_roomsecret", room->secret));

    field = xmlnode_insert_tag(x,"field");
    xmlnode_put_attrib(field,"type","list-single");
    xmlnode_put_attrib(field,"label","Affiliations that May Discover Real JIDs of Occupants");
    xmlnode_put_attrib(field,"var","muc#owner_whois");
    if(room->visible == 0)
            xmlnode_insert_cdata(xmlnode_insert_tag(field, "value"),"admins", -1);
    else
            xmlnode_insert_cdata(xmlnode_insert_tag(field, "value"),"anyone", -1);

    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "Room Owner and Admins Only");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "admins", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "Anyone");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "anyone", -1);
    
    if(room->logfile == NULL)
            xmlnode_insert_node(x,add_xdata_boolean("Enable Logging of Room Conversations", "muc#owner_enablelogging", 0));
    else
            xmlnode_insert_node(x,add_xdata_boolean("Enable Logging of Room Conversations", "muc#owner_enablelogging", 1));

    field = xmlnode_insert_tag(x,"field");
    xmlnode_put_attrib(field,"type","list-single");
    xmlnode_put_attrib(field,"label","Logfile format");
    xmlnode_put_attrib(field,"var","logformat");

    if(room->logformat == LOG_XML)
            xmlnode_insert_cdata(xmlnode_insert_tag(field, "value"),"xml", -1);
    else if(room->logformat == LOG_XHTML)
	    xmlnode_insert_cdata(xmlnode_insert_tag(field, "value"),"xhtml", -1);
    else
            xmlnode_insert_cdata(xmlnode_insert_tag(field, "value"),"text", -1);

    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "XML");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "xml", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "XHTML");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "xhtml", -1);
    element = xmlnode_insert_tag(field, "option");
    xmlnode_put_attrib(element, "label", "Plain Text");
    xmlnode_insert_cdata(xmlnode_insert_tag(element, "value"), "text", -1);
    
    deliver(dpacket_new(msg),NULL);
}   

