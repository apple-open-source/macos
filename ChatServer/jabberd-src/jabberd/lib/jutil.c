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

#include "lib.h"

/* util for making presence packets */
xmlnode jutil_presnew(int type, char *to, char *status)
{
    xmlnode pres;

    pres = xmlnode_new_tag("presence");
    switch(type)
    {
    case JPACKET__SUBSCRIBE:
        xmlnode_put_attrib(pres,"type","subscribe");
        break;
    case JPACKET__UNSUBSCRIBE:
        xmlnode_put_attrib(pres,"type","unsubscribe");
        break;
    case JPACKET__SUBSCRIBED:
        xmlnode_put_attrib(pres,"type","subscribed");
        break;
    case JPACKET__UNSUBSCRIBED:
        xmlnode_put_attrib(pres,"type","unsubscribed");
        break;
    case JPACKET__PROBE:
        xmlnode_put_attrib(pres,"type","probe");
        break;
    case JPACKET__UNAVAILABLE:
        xmlnode_put_attrib(pres,"type","unavailable");
        break;
    case JPACKET__INVISIBLE:
        xmlnode_put_attrib(pres,"type","invisible");
        break;
    }
    if(to != NULL)
        xmlnode_put_attrib(pres,"to",to);
    if(status != NULL)
        xmlnode_insert_cdata(xmlnode_insert_tag(pres,"status"),status,strlen(status));

    return pres;
}

/* util for making IQ packets */
xmlnode jutil_iqnew(int type, char *ns)
{
    xmlnode iq;

    iq = xmlnode_new_tag("iq");
    switch(type)
    {
    case JPACKET__GET:
        xmlnode_put_attrib(iq,"type","get");
        break;
    case JPACKET__SET:
        xmlnode_put_attrib(iq,"type","set");
        break;
    case JPACKET__RESULT:
        xmlnode_put_attrib(iq,"type","result");
        break;
    case JPACKET__ERROR:
        xmlnode_put_attrib(iq,"type","error");
        break;
    }
    xmlnode_put_attrib(xmlnode_insert_tag(iq,"query"),"xmlns",ns);

    return iq;
}

/* util for making message packets */
xmlnode jutil_msgnew(char *type, char *to, char *subj, char *body)
{
    xmlnode msg;

    msg = xmlnode_new_tag("message");
    xmlnode_put_attrib (msg, "type", type);
    xmlnode_put_attrib (msg, "to", to);

    if (subj)
    {
	xmlnode_insert_cdata (xmlnode_insert_tag (msg, "subject"), subj, strlen (subj));
    }

    xmlnode_insert_cdata (xmlnode_insert_tag (msg, "body"), body, strlen (body));

    return msg;
}

/* util for making stream packets */
xmlnode jutil_header(char* xmlns, char* server)
{
     xmlnode result;
     if ((xmlns == NULL)||(server == NULL))
	  return NULL;
     result = xmlnode_new_tag("stream:stream");
     xmlnode_put_attrib(result, "xmlns:stream", "http://etherx.jabber.org/streams");
     xmlnode_put_attrib(result, "xmlns", xmlns);
     xmlnode_put_attrib(result, "to", server);

     return result;
}

/* returns the priority on a presence packet */
int jutil_priority(xmlnode x)
{
    char *str;
    int p;

    if(x == NULL)
        return -1;

    if(xmlnode_get_attrib(x,"type") != NULL)
        return -1;

    x = xmlnode_get_tag(x,"priority");
    if(x == NULL)
        return 0;

    str = xmlnode_get_data((x));
    if(str == NULL)
        return 0;

    p = atoi(str);
    if(p >= 0)
        return p;
    else
        return 0;
}

void jutil_tofrom(xmlnode x)
{
    char *to, *from;

    to = xmlnode_get_attrib(x,"to");
    from = xmlnode_get_attrib(x,"from");
    xmlnode_put_attrib(x,"from",to);
    xmlnode_put_attrib(x,"to",from);
}

xmlnode jutil_iqresult(xmlnode x)
{
    xmlnode cur;

    jutil_tofrom(x);

    xmlnode_put_attrib(x,"type","result");

    /* hide all children of the iq, they go back empty */
    for(cur = xmlnode_get_firstchild(x); cur != NULL; cur = xmlnode_get_nextsibling(cur))
        xmlnode_hide(cur);

    return x;
}

char *jutil_timestamp(void)
{
    time_t t;
    struct tm *new_time;
    static char timestamp[18];
    int ret;

    t = time(NULL);

    if(t == (time_t)-1)
        return NULL;
    new_time = gmtime(&t);

    ret = snprintf(timestamp, 18, "%d%02d%02dT%02d:%02d:%02d", 1900+new_time->tm_year,
                   new_time->tm_mon+1, new_time->tm_mday, new_time->tm_hour,
                   new_time->tm_min, new_time->tm_sec);

    if(ret == -1)
        return NULL;

    return timestamp;
}

void jutil_error(xmlnode x, terror E)
{
    xmlnode err;
    char code[4];

    xmlnode_put_attrib(x,"type","error");
    err = xmlnode_insert_tag(x,"error");

    snprintf(code,4,"%d",E.code);
    xmlnode_put_attrib(err,"code",code);
    if(E.msg != NULL)
        xmlnode_insert_cdata(err,E.msg,strlen(E.msg));

    jutil_tofrom(x);
}

void jutil_delay(xmlnode msg, char *reason)
{
    xmlnode delay;

    delay = xmlnode_insert_tag(msg,"x");
    xmlnode_put_attrib(delay,"xmlns",NS_DELAY);
    xmlnode_put_attrib(delay,"from",xmlnode_get_attrib(msg,"to"));
    xmlnode_put_attrib(delay,"stamp",jutil_timestamp());
    if(reason != NULL)
        xmlnode_insert_cdata(delay,reason,strlen(reason));
}

#define KEYBUF 100

char *jutil_regkey(char *key, char *seed)
{
    static char keydb[KEYBUF][41];
    static char seeddb[KEYBUF][41];
    static int last = -1;
    char *str, strint[32];
    int i;

    /* blanket the keydb first time */
    if(last == -1)
    {
        last = 0;
        memset(&keydb,0,KEYBUF*41);
        memset(&seeddb,0,KEYBUF*41);
        srand(time(NULL));
    }

    /* creation phase */
    if(key == NULL && seed != NULL)
    {
        /* create a random key hash and store it */
        sprintf(strint,"%d",rand());
        strcpy(keydb[last],shahash(strint));

        /* store a hash for the seed associated w/ this key */
        strcpy(seeddb[last],shahash(seed));

        /* return it all */
        str = keydb[last];
        last++;
        if(last == KEYBUF) last = 0;
        return str;
    }

    /* validation phase */
    str = shahash(seed);
    for(i=0;i<KEYBUF;i++)
        if(j_strcmp(keydb[i],key) == 0 && j_strcmp(seeddb[i],str) == 0)
        {
            seeddb[i][0] = '\0'; /* invalidate this key */
            return keydb[i];
        }

    return NULL;
}

