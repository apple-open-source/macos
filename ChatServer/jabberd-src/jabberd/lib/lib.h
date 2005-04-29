/*
* Portions (c) Copyright 2005 Apple Computer, Inc.
*/

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <strings.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

#include <expat.h>

/*
**  Arrange to use either varargs or stdargs
*/

#define MAXSHORTSTR	203		/* max short string length */
#define QUAD_T	unsigned long long

#ifdef __STDC__

#include <stdarg.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap, f)
# define VA_END		va_end(ap)

#else /* __STDC__ */

# include <varargs.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap)
# define VA_END		va_end(ap)

#endif /* __STDC__ */


#ifndef INCL_LIB_H
#define INCL_LIB_H

#ifdef __cplusplus
extern "C" {
#endif


#ifndef HAVE_SNPRINTF
extern int ap_snprintf(char *, size_t, const char *, ...);
#define snprintf ap_snprintf
#endif

#ifndef HAVE_VSNPRINTF
extern int ap_vsnprintf(char *, size_t, const char *, va_list ap);
#define vsnprintf ap_vsnprintf
#endif

#define ZONE zonestr(__FILE__,__LINE__)
char *zonestr(char *file, int line);

/* --------------------------------------------------------- */
/*                                                           */
/* Pool-based memory management routines                     */
/*                                                           */
/* --------------------------------------------------------- */

#undef POOL_DEBUG
/*
 flip these, this should be a prime number for top # of pools debugging
#define POOL_DEBUG 40009 
*/

/* pheap - singular allocation of memory */
struct pheap
{
    void *block;
    int size, used;
};

/* pool_cleaner - callback type which is associated
   with a pool entry; invoked when the pool entry is 
   free'd */
typedef void (*pool_cleaner)(void *arg);

/* pfree - a linked list node which stores an
   allocation chunk, plus a callback */
struct pfree
{
    pool_cleaner f;
    void *arg;
    struct pheap *heap;
    struct pfree *next;
};

/* pool - base node for a pool. Maintains a linked list
   of pool entries (pfree) */
typedef struct pool_struct
{
    int size;
    struct pfree *cleanup;
    struct pheap *heap;
#ifdef POOL_DEBUG
    char name[8], zone[32];
    int lsize;
} _pool, *pool;
#define pool_new() _pool_new(ZONE) 
#define pool_heap(i) _pool_new_heap(i,ZONE) 
#else
} _pool, *pool;
#define pool_heap(i) _pool_new_heap(i,NULL) 
#define pool_new() _pool_new(NULL)
#endif

pool _pool_new(char *zone); /* new pool :) */
pool _pool_new_heap(int size, char *zone); /* creates a new memory pool with an initial heap size */
void *pmalloc(pool p, int size); /* wrapper around malloc, takes from the pool, cleaned up automatically */
void *pmalloc_x(pool p, int size, char c); /* Wrapper around pmalloc which prefils buffer with c */
void *pmalloco(pool p, int size); /* YAPW for zeroing the block */
char *pstrdup(pool p, const char *src); /* wrapper around strdup, gains mem from pool */
void pool_stat(int full); /* print to stderr the changed pools and reset */
char *pstrdupx(pool p, const char *src); /* temp stub */
void pool_cleanup(pool p, pool_cleaner f, void *arg); /* calls f(arg) before the pool is freed during cleanup */
void pool_free(pool p); /* calls the cleanup functions, frees all the data on the pool, and deletes the pool itself */
int pool_size(pool p); /* returns total bytes allocated in this pool */




/* --------------------------------------------------------- */
/*                                                           */
/* Socket helper stuff                                       */
/*                                                           */
/* --------------------------------------------------------- */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define NETSOCKET_SERVER 0
#define NETSOCKET_CLIENT 1
#define NETSOCKET_UDP 2

#ifndef WIN32
int make_netsocket(u_short port, char *host, int type);
struct in_addr *make_addr(char *host);
int set_fd_close_on_exec(int fd, int flag);
#ifdef WITH_IPV6
struct in6_addr *make_addr_ipv6(char *host);
#endif
#endif


/* --------------------------------------------------------- */
/*                                                           */
/* String management routines                                */
/*                                                           */
/* --------------------------------------------------------- */
char *j_strdup(const char *str); /* provides NULL safe strdup wrapper */
char *j_strcat(char *dest, char *txt); /* strcpy() clone */
int j_strcmp(const char *a, const char *b); /* provides NULL safe strcmp wrapper */
int j_strcasecmp(const char *a, const char *b); /* provides NULL safe strcasecmp wrapper */
int j_strncmp(const char *a, const char *b, int i); /* provides NULL safe strncmp wrapper */
int j_strncasecmp(const char *a, const char *b, int i); /* provides NULL safe strncasecmp wrapper */
int j_strlen(const char *a); /* provides NULL safe strlen wrapper */
int j_atoi(const char *a, int def); /* checks for NULL and uses default instead, convienence */
void str_b64decode(char *str); /* what it says */


/* --------------------------------------------------------- */
/*                                                           */
/* Base64 routines                                           */
/*                                                           */
/* --------------------------------------------------------- */
int base64_encode(unsigned char *source, size_t sourcelen, char *target, size_t targetlen);
size_t base64_decode(char *source, unsigned char *target, size_t targetlen);


/* --------------------------------------------------------- */
/*                                                           */
/* SHA calculations                                          */
/*                                                           */
/* --------------------------------------------------------- */
#if (SIZEOF_INT == 4)
typedef unsigned int uint32;
#elif (SIZEOF_SHORT == 4)
typedef unsigned short uint32;
#else
typedef unsigned int uint32;
#endif /* HAVEUINT32 */

char *shahash(char *str);	/* NOT THREAD SAFE */
void shahash_r(const char* str, char hashbuf[40]); /* USE ME */

int strprintsha(char *dest, int *hashval);


/* --------------------------------------------------------- */
/*                                                           */
/* Hashtable functions                                       */
/*                                                           */
/* --------------------------------------------------------- */
typedef struct xhn_struct
{
    struct xhn_struct *next;
    const char *key;
    void *val;
} *xhn, _xhn;

typedef struct xht_struct
{
    pool p;
    int prime;
    struct xhn_struct *zen;
} *xht, _xht;

xht xhash_new(int prime);
void xhash_put(xht h, const char *key, void *val);
void *xhash_get(xht h, const char *key);
void xhash_zap(xht h, const char *key);
void xhash_free(xht h);
typedef void (*xhash_walker)(xht h, const char *key, void *val, void *arg);
void xhash_walk(xht h, xhash_walker w, void *arg);

/* --------------------------------------------------------- */
/*                                                           */
/* DEPRECIATED Hashtable functions                           */
/*                                                           */
/* --------------------------------------------------------- */
typedef int (*KEYHASHFUNC)(const void *key);
typedef int (*KEYCOMPAREFUNC)(const void *key1, const void *key2);
typedef int (*TABLEWALKFUNC)(void *user_data, const void *key, void *data);

typedef void *HASHTABLE;

HASHTABLE ghash_create(int buckets, KEYHASHFUNC hash, KEYCOMPAREFUNC cmp);
HASHTABLE ghash_create_pool(pool p, int buckets, KEYHASHFUNC hash, KEYCOMPAREFUNC cmp);
void ghash_destroy(HASHTABLE tbl);
void *ghash_get(HASHTABLE tbl, const void *key);
int ghash_put(HASHTABLE tbl, const void *key, void *value);
int ghash_remove(HASHTABLE tbl, const void *key);
int ghash_walk(HASHTABLE tbl, TABLEWALKFUNC func, void *user_data);
int str_hash_code(const char *s);


/* --------------------------------------------------------- */
/*                                                           */
/* XML escaping utils                                        */
/*                                                           */
/* --------------------------------------------------------- */
char *strescape(pool p, char *buf); /* Escape <>&'" chars */
char *strunescape(pool p, char *buf);


/* --------------------------------------------------------- */
/*                                                           */
/* String pools (spool) functions                            */
/*                                                           */
/* --------------------------------------------------------- */
struct spool_node
{
    char *c;
    struct spool_node *next;
};

typedef struct spool_struct
{
    pool p;
    int len;
    struct spool_node *last;
    struct spool_node *first;
} *spool;

spool spool_new(pool p); /* create a string pool */
void spooler(spool s, ...); /* append all the char * args to the pool, terminate args with s again */
char *spool_print(spool s); /* return a big string */
void spool_add(spool s, char *str); /* add a single char to the pool */
char *spools(pool p, ...); /* wrap all the spooler stuff in one function, the happy fun ball! */


/* --------------------------------------------------------- */
/*                                                           */
/* xmlnodes - Document Object Model                          */
/*                                                           */
/* --------------------------------------------------------- */
#define NTYPE_TAG    0
#define NTYPE_ATTRIB 1
#define NTYPE_CDATA  2

#define NTYPE_LAST   2
#define NTYPE_UNDEF  -1

/* -------------------------------------------------------------------------- 
   Node structure. Do not use directly! Always use accessor macros 
   and methods!
   -------------------------------------------------------------------------- */
typedef struct xmlnode_t
{
     char*               name;
     unsigned short      type;
     char*               data;
     int                 data_sz;
     int                 complete;
     pool               p;
     struct xmlnode_t*  parent;
     struct xmlnode_t*  firstchild; 
     struct xmlnode_t*  lastchild;
     struct xmlnode_t*  prev; 
     struct xmlnode_t*  next;
     struct xmlnode_t*  firstattrib;
     struct xmlnode_t*  lastattrib;
} _xmlnode, *xmlnode;

/* Node creation routines */
xmlnode  xmlnode_wrap(xmlnode x,const char* wrapper);
xmlnode  xmlnode_new_tag(const char* name);
xmlnode  xmlnode_new_tag_pool(pool p, const char* name);
xmlnode  xmlnode_insert_tag(xmlnode parent, const char* name); 
xmlnode  xmlnode_insert_cdata(xmlnode parent, const char* CDATA, unsigned int size);
xmlnode  xmlnode_insert_tag_node(xmlnode parent, xmlnode node);
void     xmlnode_insert_node(xmlnode parent, xmlnode node);
xmlnode  xmlnode_str(char *str, int len);
xmlnode  xmlnode_file(char *file);
char*    xmlnode_file_borked(char *file); /* same as _file but returns the parsing error */
xmlnode  xmlnode_dup(xmlnode x); /* duplicate x */
xmlnode  xmlnode_dup_pool(pool p, xmlnode x);

/* Node Memory Pool */
pool xmlnode_pool(xmlnode node);
xmlnode _xmlnode_new(pool p, const char *name, unsigned int type);

/* Node editing */
void xmlnode_hide(xmlnode child);
void xmlnode_hide_attrib(xmlnode parent, const char *name);

/* Node deletion routine, also frees the node pool! */
void xmlnode_free(xmlnode node);

/* Locates a child tag by name and returns it */
xmlnode  xmlnode_get_tag(xmlnode parent, const char* name);
char* xmlnode_get_tag_data(xmlnode parent, const char* name);

/* Attribute accessors */
void     xmlnode_put_attrib(xmlnode owner, const char* name, const char* value);
char*    xmlnode_get_attrib(xmlnode owner, const char* name);
void     xmlnode_put_expat_attribs(xmlnode owner, const char** atts);

/* Bastard am I, but these are fun for internal use ;-) */
void     xmlnode_put_vattrib(xmlnode owner, const char* name, void *value);
void*    xmlnode_get_vattrib(xmlnode owner, const char* name);

/* Node traversal routines */
xmlnode  xmlnode_get_firstattrib(xmlnode parent);
xmlnode  xmlnode_get_firstchild(xmlnode parent);
xmlnode  xmlnode_get_lastchild(xmlnode parent);
xmlnode  xmlnode_get_nextsibling(xmlnode sibling);
xmlnode  xmlnode_get_prevsibling(xmlnode sibling);
xmlnode  xmlnode_get_parent(xmlnode node);

/* Node information routines */
char*    xmlnode_get_name(xmlnode node);
char*    xmlnode_get_data(xmlnode node);
int      xmlnode_get_datasz(xmlnode node);
int      xmlnode_get_type(xmlnode node);

int      xmlnode_has_children(xmlnode node);
int      xmlnode_has_attribs(xmlnode node);

/* Node-to-string translation */
char*    xmlnode2str(xmlnode node);

/* Node-to-terminated-string translation 
   -- useful for interfacing w/ scripting langs */
char*    xmlnode2tstr(xmlnode node);

int      xmlnode_cmp(xmlnode a, xmlnode b); /* compares a and b for equality */

int      xmlnode2file(char *file, xmlnode node); /* writes node to file */

/* Expat callbacks */
void expat_startElement(void* userdata, const char* name, const char** atts);
void expat_endElement(void* userdata, const char* name);
void expat_charData(void* userdata, const char* s, int len);

/***********************
 * XSTREAM Section
 ***********************/

#define XSTREAM_MAXNODE 1000000
#define XSTREAM_MAXDEPTH 100

#define XSTREAM_ROOT        0 /* root element */
#define XSTREAM_NODE        1 /* normal node */
#define XSTREAM_CLOSE       2 /* closed </stream:stream> */
#define XSTREAM_ERR         4 /* parser error */

typedef void (*xstream_onNode)(int type, xmlnode x, void *arg); /* xstream event handler */

typedef struct xstream_struct
{
    XML_Parser parser;
    xmlnode node;
    char *cdata;
    int cdata_len;
    pool p;
    xstream_onNode f;
    void *arg;
    int status;
    int depth;
} *xstream, _xstream;

xstream xstream_new(pool p, xstream_onNode f, void *arg); /* create a new xstream */
int xstream_eat(xstream xs, char *buff, int len); /* parse new data for this xstream, returns last XSTREAM_* status */

/* convience functions */
xmlnode xstream_header(char *namespace, char *to, char *from);
char *xstream_header_char(xmlnode x);


typedef struct {
  unsigned long H[5];
  unsigned long W[80];
  int lenW;
  unsigned long sizeHi,sizeLo;
} j_SHA_CTX;


void shaInit(j_SHA_CTX *ctx);
void shaUpdate(j_SHA_CTX *ctx, unsigned char *dataIn, int len);
void shaFinal(j_SHA_CTX *ctx, unsigned char hashout[20]);
void shaBlock(unsigned char *dataIn, int len, unsigned char hashout[20]);

/********** END OLD libxode.h BEGIN OLD jabber.h *************/

/* --------------------------------------------------------- */
/*                                                           */
/* JID structures & constants                                */
/*                                                           */
/* --------------------------------------------------------- */
#define JID_RESOURCE 1
#define JID_USER     2
#define JID_SERVER   4

typedef struct jid_struct
{ 
    pool               p;
    char*              resource;
    char*              user;
    char*              server;
    char*              full;
    struct jid_struct *next; /* for lists of jids */
} *jid;
  
jid     jid_new(pool p, char *idstr);	       /* Creates a jabber id from the idstr */
void    jid_set(jid id, char *str, int item);  /* Individually sets jid components */
char*   jid_full(jid id);		       /* Builds a string type=user/resource@server from the jid data */
int     jid_cmp(jid a, jid b);		       /* Compares two jid's, returns 0 for perfect match */
int     jid_cmpx(jid a, jid b, int parts);     /* Compares just the parts specified as JID_|JID_ */
jid     jid_append(jid a, jid b);	       /* Appending b to a (list), no dups */
xmlnode jid_xres(jid id);		       /* Returns xmlnode representation of the resource?query=string */
xmlnode jid_nodescan(jid id, xmlnode x);       /* Scans the children of the node for a matching jid attribute */
jid     jid_user(jid a);                       /* returns the same jid but just of the user@host part */


/* --------------------------------------------------------- */
/*                                                           */
/* JPacket structures & constants                            */
/*                                                           */
/* --------------------------------------------------------- */
#define JPACKET_UNKNOWN   0x00
#define JPACKET_MESSAGE   0x01
#define JPACKET_PRESENCE  0x02
#define JPACKET_IQ        0x04
#define JPACKET_S10N      0x08

#define JPACKET__UNKNOWN      0
#define JPACKET__NONE         1
#define JPACKET__ERROR        2
#define JPACKET__CHAT         3
#define JPACKET__GROUPCHAT    4
#define JPACKET__GET          5
#define JPACKET__SET          6
#define JPACKET__RESULT       7
#define JPACKET__SUBSCRIBE    8
#define JPACKET__SUBSCRIBED   9
#define JPACKET__UNSUBSCRIBE  10
#define JPACKET__UNSUBSCRIBED 11
#define JPACKET__AVAILABLE    12
#define JPACKET__UNAVAILABLE  13
#define JPACKET__PROBE        14
#define JPACKET__HEADLINE     15
#define JPACKET__INVISIBLE    16

typedef struct jpacket_struct
{
    unsigned char type;
    int           subtype;
    int           flag;
    void*         aux1;
    xmlnode       x;
    jid           to;
    jid           from;
    char*         iqns;
    xmlnode       iq;
    pool          p;
} *jpacket, _jpacket;
 
jpacket jpacket_new(xmlnode x);	    /* Creates a jabber packet from the xmlnode */
jpacket jpacket_reset(jpacket p);   /* Resets the jpacket values based on the xmlnode */
int     jpacket_subtype(jpacket p); /* Returns the subtype value (looks at xmlnode for it) */


/* --------------------------------------------------------- */
/*                                                           */
/* Presence Proxy DB structures & constants                  */
/*                                                           */
/* --------------------------------------------------------- */
typedef struct ppdb_struct
{			      
    jid     id;		       /* entry data */
    int     pri;
    xmlnode x;
    struct ppdb_struct* user;  /* linked list for user@server */
    pool                p;     /* db-level data */
    struct ppdb_struct* next;
} _ppdb, *ppdb;

ppdb    ppdb_insert(ppdb db, jid id, xmlnode x); /* Inserts presence into the proxy */
xmlnode ppdb_primary(ppdb db, jid id);		 /* Fetches the matching primary presence for the id */
void    ppdb_free(ppdb db);			 /* Frees the db and all entries */
xmlnode ppdb_get(ppdb db, jid id);		 /* Called successively to return each presence xmlnode */
						 /*   for the id and children, returns NULL at the end */


/* --------------------------------------------------------- */
/*                                                           */
/* Simple Jabber Rate limit functions                        */
/*                                                           */
/* --------------------------------------------------------- */
typedef struct jlimit_struct
{
    char *key;
    int start;
    int points;
    int maxt, maxp;
    pool p;
} *jlimit, _jlimit;
 
jlimit jlimit_new(int maxt, int maxp);
void jlimit_free(jlimit r);
int jlimit_check(jlimit r, char *key, int points);


// #define KARMA_DEBUG
// default to disable karma 
#define KARMA_READ_MAX(k) (abs(k)*100) /* how much you are allowed to read off the sock */
#define KARMA_INIT 5   /* internal "init" value */
#define KARMA_HEARTBEAT 2 /* seconds to register for heartbeat */
#define KARMA_MAX 10     /* total max karma you can have */
#define KARMA_INC 1      /* how much to increment every KARMA_HEARTBEAT seconds */
#define KARMA_DEC 0      /* how much to penalize for reading KARMA_READ_MAX in
                            KARMA_HEARTBEAT seconds */
#define KARMA_PENALTY -5 /* where you go when you hit 0 karma */
#define KARMA_RESTORE 5  /* where you go when you payed your penelty or INIT */
#define KARMA_RESETMETER 0 /* Reset byte meter on restore default is falst */

struct karma
{
    int init; /* struct initialized */
    int reset_meter; /* reset the byte meter on restore */
    int val; /* current karma value */
    long bytes; /* total bytes read (in that time period) */
    int max;  /* max karma you can have */
    int inc,dec; /* how much to increment/decrement */
    int penalty,restore; /* what penalty (<0) or restore (>0) */
    time_t last_update; /* time this was last incremented */
};

struct karma *karma_new(pool p); /* creates a new karma object, with default values */
void karma_copy(struct karma *new, struct karma *old); /* makes a copy of old in new */
void karma_increment(struct karma *k);          /* inteligently increments karma */
void karma_decrement(struct karma *k, long bytes_read); /* inteligently decrements karma */
int karma_check(struct karma *k,long bytes_read); /* checks to see if we have good karma */



/* --------------------------------------------------------- */
/*                                                           */
/* Error structures & constants                              */
/*                                                           */
/* --------------------------------------------------------- */
typedef struct terror_struct
{
    int  code;
    char msg[64];
} terror;

#define TERROR_BAD           (terror){400,"Bad Request"}
#define TERROR_AUTH          (terror){401,"Unauthorized"}
#define TERROR_PAY           (terror){402,"Payment Required"}
#define TERROR_FORBIDDEN     (terror){403,"Forbidden"}
#define TERROR_NOTFOUND      (terror){404,"Not Found"}
#define TERROR_NOTALLOWED    (terror){405,"Not Allowed"}
#define TERROR_NOTACCEPTABLE (terror){406,"Not Acceptable"}
#define TERROR_REGISTER      (terror){407,"Registration Required"}
#define TERROR_REQTIMEOUT    (terror){408,"Request Timeout"}
#define TERROR_CONFLICT      (terror){409,"Conflict"}

#define TERROR_INTERNAL   (terror){500,"Internal Server Error"}
#define TERROR_NOTIMPL    (terror){501,"Not Implemented"}
#define TERROR_EXTERNAL   (terror){502,"Remote Server Error"}
#define TERROR_UNAVAIL    (terror){503,"Service Unavailable"}
#define TERROR_EXTTIMEOUT (terror){504,"Remote Server Timeout"}
#define TERROR_DISCONNECTED (terror){510,"Disconnected"}

/* --------------------------------------------------------- */
/*                                                           */
/* Namespace constants                                       */
/*                                                           */
/* --------------------------------------------------------- */
#define NSCHECK(x,n) (j_strcmp(xmlnode_get_attrib(x,"xmlns"),n) == 0)

#define NS_CLIENT    "jabber:client"
#define NS_SERVER    "jabber:server"
#define NS_AUTH      "jabber:iq:auth"
#define NS_AUTH_CRYPT "jabber:iq:auth:crypt"
#define NS_REGISTER  "jabber:iq:register"
#define NS_ROSTER    "jabber:iq:roster"
#define NS_OFFLINE   "jabber:x:offline"
#define NS_AGENT     "jabber:iq:agent"
#define NS_AGENTS    "jabber:iq:agents"
#define NS_DELAY     "jabber:x:delay"
#define NS_VERSION   "jabber:iq:version"
#define NS_TIME      "jabber:iq:time"
#define NS_VCARD     "vcard-temp"
#define NS_PRIVATE   "jabber:iq:private"
#define NS_SEARCH    "jabber:iq:search"
#define NS_OOB       "jabber:iq:oob"
#define NS_XOOB      "jabber:x:oob"
#define NS_ADMIN     "jabber:iq:admin"
#define NS_FILTER    "jabber:iq:filter"
#define NS_AUTH_0K   "jabber:iq:auth:0k"
#define NS_BROWSE    "jabber:iq:browse"
#define NS_EVENT     "jabber:x:event"
#define NS_CONFERENCE "jabber:iq:conference"
#define NS_SIGNED    "jabber:x:signed"
#define NS_ENCRYPTED "jabber:x:encrypted"
#define NS_GATEWAY   "jabber:iq:gateway"
#define NS_LAST      "jabber:iq:last"
#define NS_ENVELOPE  "jabber:x:envelope"
#define NS_EXPIRE    "jabber:x:expire"
#define NS_XHTML     "http://www.w3.org/1999/xhtml"

#define NS_XDBGINSERT "jabber:xdb:ginsert"
#define NS_XDBNSLIST  "jabber:xdb:nslist"


/* --------------------------------------------------------- */
/*                                                           */
/* JUtil functions                                           */
/*                                                           */
/* --------------------------------------------------------- */
xmlnode jutil_presnew(int type, char *to, char *status); /* Create a skeleton presence packet */
xmlnode jutil_iqnew(int type, char *ns);		 /* Create a skeleton iq packet */
xmlnode jutil_msgnew(char *type, char *to, char *subj, char *body);
							 /* Create a skeleton message packet */
xmlnode jutil_header(char* xmlns, char* server);	 /* Create a skeleton stream packet */
int     jutil_priority(xmlnode x);			 /* Determine priority of this packet */
void    jutil_tofrom(xmlnode x);			 /* Swaps to/from fields on a packet */
xmlnode jutil_iqresult(xmlnode x);			 /* Generate a skeleton iq/result, given a iq/query */
char*   jutil_timestamp(void);				 /* Get stringified timestamp */
void    jutil_error(xmlnode x, terror E);		 /* Append an <error> node to x */
void    jutil_delay(xmlnode msg, char *reason);		 /* Append a delay packet to msg */
char*   jutil_regkey(char *key, char *seed);		 /* pass a seed to generate a key, pass the key again to validate (returns it) */


#ifdef __cplusplus
}
#endif

#endif	/* INCL_LIB_H */
