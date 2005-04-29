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
#include "jabberd.h"

/* worker thread max waiting pool size */
#define SESSION_WAITERS 10

/* set to a prime number larger then the average max # of hosts, and another for the max # of users for any single host */
#define HOSTS_PRIME 17
#define USERS_PRIME 3001

/* master event types */
typedef int event;
#define e_SESSION  0  /* when a session is starting up */
#define e_OFFLINE  1  /* data for an offline user */
#define e_SERVER   2  /* packets for the server.host */
#define e_DELIVER  3  /* about to deliver a packet to an mp */
#define e_SHUTDOWN 4  /* server is shutting down, last chance! */
#define e_AUTH     5  /* authentication handlers */
#define e_REGISTER 6  /* registration request */
/* always add new event types here, to maintain backwards binary compatibility */
#define e_LAST     7  /* flag for the highest */

/* session event types */
#define es_IN      0  /* for packets coming into the session */
#define es_OUT     1  /* for packets originating from the session */
#define es_END     2  /* when a session ends */
/* always add new event types here, to maintain backwards binary compatibility */
#define es_LAST    3  /* flag for the highest */

/* admin user account flags */
#define ADMIN_UNKNOWN   0x00
#define ADMIN_NONE      0x01
#define ADMIN_READ      0x02
#define ADMIN_WRITE     0x04


typedef enum {M_PASS,   /* we don't want this packet this tim */
              M_IGNORE, /* we don't want this packet ever */
              M_HANDLED /* stop mapi processing on this packet */
             } mreturn;

typedef struct udata_struct *udata, _udata;
typedef struct session_struct *session, _session;
typedef struct jsmi_struct *jsmi, _jsmi;

typedef struct mapi_struct
{
    jsmi si;
    jpacket packet;
    event e;
    udata user;
    session s;
} *mapi, _mapi;

typedef mreturn (*mcall)(mapi m, void *arg);

typedef struct mlist_struct
{
    mcall c;
    void *arg;
    unsigned char mask;
    struct mlist_struct *next;
} *mlist, _mlist;

/* globals for this instance of jsm */
struct jsmi_struct
{
    instance i;
    xmlnode config;
    HASHTABLE hosts;
    xdbcache xc;
    mlist events[e_LAST];
    pool p;
    jid gtrust;
};

struct udata_struct
{
    char *user;
    char *pass;
    jid id, utrust;
    jsmi si;
    session sessions;
    int scount, ref, admin;
    pool p;
    struct udata_struct *next;
};

xmlnode js_config(jsmi si, char *query);

udata js_user(jsmi si, jid id, HASHTABLE ht);
void js_deliver(jsmi si, jpacket p);


struct session_struct
{
    /* general session data */
    jsmi si;
    char *res;
    jid id;
    udata u;
    xmlnode presence;
    int priority, roster;
    int c_in, c_out;
    time_t started;

    /* mechanics */
    pool p;
    int exit_flag;
    mlist events[es_LAST];
    mtq q; /* thread queue */

    /* our routed id, and remote session id */
    jid route;
    jid sid;

    struct session_struct *next;
};

session js_session_new(jsmi si, dpacket p);
void js_session_end(session s, char *reason);
session js_session_get(udata user, char *res);
session js_session_primary(udata user);
void js_session_to(session s, jpacket p);
void js_session_from(session s, jpacket p);

void js_server_main(void *arg);
void js_offline_main(void *arg);
result js_users_gc(void *arg);

typedef struct jpq_struct
{
    jsmi si;
    jpacket p;
} _jpq, *jpq;

void js_psend(jsmi si, jpacket p, mtq_callback f); /* sends p to a function */

void js_bounce(jsmi si, xmlnode x, terror terr); /* logic to bounce packets w/o looping, eats x and delivers error */

void js_mapi_register(jsmi si, event e, mcall c, void *arg);
void js_mapi_session(event e, session s, mcall c, void *arg);
int js_mapi_call(jsmi si, event e, jpacket packet, udata user, session s);

void js_authreg(void *arg);

int js_admin(udata u, int flag);

result js_packet(instance i, dpacket p, void *arg);
int js_islocal(jsmi si, jid id);
int js_trust(udata u, jid id); /* checks if id is trusted by user u */
jid js_trustees(udata u); /* returns list of trusted jids */
int js_online(mapi m); /* logic to tell if this is a go-online call */

void jsm_shutdown(void *arg);
