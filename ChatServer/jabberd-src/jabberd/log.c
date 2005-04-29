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
 * Portions (c) Copyright 2005 Apple Computer, Inc.
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

int debug_flag = 0;
int syslog_flag = 0;

extern HASHTABLE debug__zones, cmd__line;

char *debug_log_timestamp(void)
{
    time_t t;
    int sz;
    char *tmp_str;

    t = time(NULL);

    if(t == (time_t)-1)
        return NULL;

    tmp_str = ctime(&t);
    sz = strlen(tmp_str);
    /* chop off the \n */
    tmp_str[sz-1]=' ';

    return tmp_str;
}

void debug_log(char *zone, const char *msgfmt, ...)
{
    va_list ap;
    char message[MAX_LOG_SIZE];
    char *pos, c = '\0';
    int offset;
	int sendToSysLog = 1; // true
	
	if (zone != NULL && '.' == *zone)
	{ 	sendToSysLog = 0;
		zone = ZONE; // use the default zone
	}
	
	/* special per-zone filtering */
    if(zone != NULL && debug__zones != NULL)
    {
        pos = strchr(zone,'.');
        if(pos != NULL)
        {
            c = *pos;
            *pos = '\0'; /* chop */
        }
        if(ghash_get(debug__zones,zone) == NULL)
            return;
        if(pos != NULL)
            *pos = c; /* restore */
    }

	if (NULL == msgfmt)
		return;
		
    snprintf(message, MAX_LOG_SIZE, "%s %s ", debug_log_timestamp(), zone);
    for (pos = message; *pos != '\0'; pos++); //empty statement
     
    offset = pos - message;

    va_start(ap, msgfmt);
    vsnprintf(pos, MAX_LOG_SIZE - offset, msgfmt, ap);
    
    if (sendToSysLog)
    {	log_syslog(LOG_DEBUG | LOG_DAEMON, "%s", message);
    }
    
    fprintf(stderr,"%s", message);
    fprintf(stderr, "\n");
}

void logger(char *type, char *host, char *message, int type_priority)
{
    xmlnode log;

    if(type == NULL || message == NULL)
    {
        fprintf(stderr, "Unrecoverable: logger function called with illegal arguments!\n");
        return;
    }

    log = xmlnode_new_tag("log");
    xmlnode_put_attrib(log,"type",type);
    if(host != NULL)
        xmlnode_put_attrib(log,"from",host);
    else
        xmlnode_put_attrib(log,"from","-internal");
    xmlnode_insert_cdata(log,message,strlen(message));

    char *logMessage = xmlnode2str(log);
    log_debug(".","%s",logMessage); // special zone don't sys log
    log_syslog(type_priority | LOG_DAEMON , "%s", logMessage);
    
    deliver(dpacket_new(log), NULL);
}

void log_notice(char *host, const char *msgfmt, ...)
{
    va_list ap;
    char logmsg[512] = "";


    va_start(ap, msgfmt);
    vsnprintf(logmsg, 512,  msgfmt,  ap);

    logger("notice",host,logmsg, LOG_NOTICE);
}

void log_warn(char *host, const char *msgfmt, ...)
{
    va_list ap;
    char logmsg[512] = "";


    va_start(ap, msgfmt);
    vsnprintf(logmsg, 512, msgfmt, ap);

    logger("warn",host,logmsg, LOG_WARNING);
}

void log_alert(char *host, const char *msgfmt, ...)
{
    va_list ap;
    char logmsg[512] = "";


    va_start(ap, msgfmt);
    vsnprintf(logmsg, 512, msgfmt, ap);

    logger("alert",host, logmsg, LOG_ALERT);
}

/* generic log record support */
void log_record(char *id, char *type, char *action, const char *msgfmt, ...)
{
    va_list ap;
    char logmsg[512] = "";
    xmlnode log;

    va_start(ap, msgfmt);
    vsnprintf(logmsg, 512, msgfmt, ap);

    log = xmlnode_new_tag("log");
    xmlnode_put_attrib(log,"type","record");
    if(id != NULL)
        xmlnode_put_attrib(log,"from",id);
    else
        xmlnode_put_attrib(log,"from","-internal");

    /* make log record like "type action rest-of-data" */
    if(type != NULL)
        xmlnode_insert_cdata(log,type,strlen(type));
    else
        xmlnode_insert_cdata(log,"unknown",7);
    xmlnode_insert_cdata(log," ",1);
    if(action != NULL)
        xmlnode_insert_cdata(log,action,strlen(action));
    else
        xmlnode_insert_cdata(log,"unknown",7);
    xmlnode_insert_cdata(log," ",1);
    xmlnode_insert_cdata(log,logmsg,strlen(logmsg));

	char *logMessage = xmlnode2str(log);
	if (NULL == logMessage)
		return;
		
    log_debug(".","%s",logMessage);
    log_syslog(LOG_NOTICE | LOG_DAEMON, "%s", logMessage);

    deliver(dpacket_new(log), NULL);
}

int get_debug_flag()
{
    return debug_flag;
}

void set_debug_flag(int v)
{
    debug_flag = v;
}


int get_syslog_flag()
{
    return syslog_flag;
}

void set_syslog_flag(int v)
{
    syslog_flag = v;
}
