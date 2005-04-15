/*
**
** mysql Auxprop plugin
**   by Simon Loader
**
** $Id: mysql.c,v 1.2 2004/07/07 22:45:18 snsimon Exp $
**
**  Auxiliary property plugin for Sasl 2.1.x
**
**   The plugin uses the following options in the
** sasl application config file ( usually in /usr/lib/sasl2 )
**
**  mysql_user: <username to login as>
**  mysql_passwd: <password to use>
**  mysql_hostnames: < comma seprated host list >
**  mysql_database: <database to connect to>
**  mysql_statement: <select statement to use>
**  mysql_verbose:  ( if it exists will print select statement to syslog )
**
**   The select statement used in the option mysql_statement is parsed
** for 3 place holders %u %r and %p they are replaced with username
** realm and property required respectively.
**
**  e.g
**    mysql_statement: select %p from user_table where username = %u and
**    realm = %r
**  would produce a statement like this :-
**
**     select userPassword from user_table where username = simon
**     and realm = madoka.surf.org.uk
**
**   Presuming username is simon, the sasl application is trying to
**   authenticate and you didn't have a realm to start with (and it was
**   my computer).
**
** OK so thats a bit complex but essential
**   %u is the username the user logged in as
**   %p is the property requested this could technically be anything
**     but sasl authentication will try userPassword and
**     cmusaslsecretMECHNAME (where MECHNAME is the name of a mechanism).
**   %r is the realm which could be the kerbros realm, the FQDN of the 
**     computer the sasl app is on or what ever is after the @ on a username.
** 
**   These do not have to be all used or used at all
** in testing I used select password from auth where username = '%u'
**     
*/

#include <config.h>

/* checkpw stuff */

#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "sasl.h"
#include "saslutil.h"
#include "saslplug.h"

#ifdef __APPLE__
#include <mysql/mysql.h>
// hack for now
my_bool compress(unsigned char *a,unsigned long *b, unsigned long *c) { return 0; };
my_bool uncompress(unsigned char *a, unsigned long *b, unsigned long *c) { return 0; };
// end hack
#else
#include <mysql.h>
#endif


#include "plugin_common.h"

typedef struct mysql_settings {
    const char *mysql_user;
    const char *mysql_passwd;
    const char *mysql_hostnames;
    const char *mysql_database;
    const char *mysql_statement;
    int mysql_verbose;
    int have_settings;
} mysql_settings_t;

static const char * MYSQL_BLANK_STRING = "";

/*
**  Mysql_create_statemnet
**   uses select line and allocate memory to replace
**  Parts with the strings provided.
**   %<char> =  no change
**   %% = %
**   %u = user
**   %p = prop
**   %r = realm
**  e.g select %p from auth where user = %p and domain = %r;
**  Note: calling function must free memory.
**
*/
static char *mysql_create_statement(sasl_server_params_t *sparams,
				    const char *select_line, const char *prop,
				    const char *user, const char *realm)
{
    const char *ptr,*line_ptr;
    char *buf,*buf_ptr;
    int filtersize = 0;
    int ulen = 0, plen = 0, rlen = 0;
    
    /* ++++ this could be modulised more */
    /* calculate memory needed for creating 
       the complete filter string. */
    ptr = select_line;
    
    /* we can use strtok to get all vars */
    while ( (ptr = strchr(ptr,'%')) ) {
	ptr++;
	switch ( *ptr ) {
	case '%':
	    filtersize--;  /* we are actully deleting a character */
	    break;
	case 'u':
	    ulen = strlen(user);
	    filtersize += ulen-2;
	    break;
	case 'r':
	    rlen = strlen(realm);
	    filtersize += rlen-2;
	    break;
	case 'p':
	    plen = strlen(prop);
	    filtersize += plen-2;
	    break;
	default:
	    break;
	}
    }
    
    /* don't forget the trailing 0x0 */
    filtersize = filtersize+strlen(select_line)+1;
    
    /* ok, now try to allocate a chunk of that size */
    buf = (char *)sparams->utils->malloc(filtersize);
    if (!buf)
	return NULL;
    
    buf_ptr = buf;
    line_ptr = select_line;
    
    /* replace the strings */
    while ( (ptr = strchr(line_ptr,'%')) ) {
	/* copy up to but not including the next % */
	memcpy(buf_ptr,line_ptr,ptr - line_ptr); 
	buf_ptr += ptr - line_ptr;
	ptr++;
	switch (ptr[0]) {
	case '%':
	    buf_ptr[0] = '%';
	    buf_ptr++;
	    break;
	case 'u':
	    memcpy(buf_ptr,user,ulen);
	    buf_ptr += ulen;
	    break;
	case 'r':
	    memcpy(buf_ptr,realm,rlen);
	    buf_ptr += rlen;
	    break;
	case 'p':
	    memcpy(buf_ptr,prop,plen);
	    buf_ptr += plen;
	    break;
	default:
	    buf_ptr[0] = '%';
	    buf_ptr[1] = ptr[0];
	    buf_ptr += 2;
	    break;
	}
	ptr++;
	line_ptr = ptr;
    }
    /* now copy the last bit */
    memcpy(buf_ptr,line_ptr,strlen(line_ptr)+1); /* we need the null */
    return(buf);
}

/* mysql_get_settings
**
**  Get the auxprop settings and put them in 
** The global context array
*/
void mysql_get_settings(const sasl_utils_t *utils,void *glob_context) {
    struct mysql_settings *settings;
    int r;
    char *verbose_test;
    
    settings = (struct mysql_settings *)glob_context;
    /* do I have to allocate memory for the vars only testing will tell */
    /*( probably )*/
    if ( settings->have_settings == 0 ) {
	utils->getopt(utils->getopt_context,
		      "MYSQL","mysql_verbose",
		      (const char **)&verbose_test,NULL);
	if ( verbose_test != NULL ) {
	    settings->mysql_verbose = 1;
	    utils->log(NULL, SASL_LOG_WARN,
		       "mysql auxprop plugin has been requested\n");
	} else {
	    settings->mysql_verbose = 0;
	}
	
	r = utils->getopt(utils->getopt_context,"MYSQL","mysql_user",
			  &settings->mysql_user,NULL);
	if ( r || !settings->mysql_user ) {
	    /* set it to a blank string */
	    settings->mysql_user = MYSQL_BLANK_STRING;
	}
  	r = utils->getopt(utils->getopt_context,"MYSQL", "mysql_passwd",
			  &settings->mysql_passwd, NULL);
  	if ( r || !settings->mysql_passwd ) {
	    settings->mysql_passwd = MYSQL_BLANK_STRING;
  	}
	r = utils->getopt(utils->getopt_context,"MYSQL", "mysql_hostnames",
			  &settings->mysql_hostnames, NULL);
	if ( r || !settings->mysql_hostnames ) {
	    settings->mysql_hostnames = MYSQL_BLANK_STRING;
	}
	r = utils->getopt(utils->getopt_context,"MYSQL", "mysql_database",
			  &settings->mysql_database, NULL);
	if ( r || !settings->mysql_database ) {
	    settings->mysql_database = MYSQL_BLANK_STRING;
	}
	r = utils->getopt(utils->getopt_context,"MYSQL", "mysql_statement",
		          &settings->mysql_statement, NULL);
	if ( r || !settings->mysql_statement ) {
	    settings->mysql_statement = MYSQL_BLANK_STRING;
	}
	settings->have_settings = 1;
    }
}

static void mysql_auxprop_lookup(void *glob_context,
				 sasl_server_params_t *sparams,
				 unsigned flags,
				 const char *user,
				 unsigned ulen) 
{
    char *userid = NULL;
    /* realm could be used for something clever */
    char *realm = NULL;
    const char *user_realm = NULL;
    const struct propval *to_fetch, *cur;
    char value[8192];
    size_t value_len;
    
    int row_count;
    char *user_buf;
    char *db_host_ptr = NULL;
    char *db_host = NULL;
    char *cur_host;
    char *query = NULL;
    char *escap_userid = NULL;
    char *escap_realm = NULL;
    struct mysql_settings *settings;
    MYSQL mysql,*sock = NULL;
    MYSQL_RES *result;
    MYSQL_ROW row;
    
    /* setup the settings */
    settings = (struct mysql_settings *)glob_context;
    mysql_get_settings(sparams->utils,glob_context);
    
    if(!sparams || !user) return;
    
    if ( settings->mysql_verbose )
	sparams->utils->log(NULL, SASL_LOG_WARN,
			    "mysql plugin Parse the username %s\n", user);
    
    user_buf = sparams->utils->malloc(ulen + 1);
    if(!user_buf)
	goto done;
    
    memcpy(user_buf, user, ulen);
    user_buf[ulen] = '\0';
    
    if(sparams->user_realm) {
	user_realm = sparams->user_realm;
    } else {
	user_realm = sparams->serverFQDN;
    }
    
    if (_plug_parseuser(sparams->utils, &userid, &realm, user_realm,
			sparams->serverFQDN, user_buf) != SASL_OK )
	goto done;
    
    /* just need to escape userid and realm now */
    /* allocate some memory */
    escap_userid = (char *)sparams->utils->malloc(strlen(userid)*2+1);
    escap_realm = (char *)sparams->utils->malloc(strlen(realm)*2+1);
    
    if (!escap_userid || !escap_realm) {
	MEMERROR(sparams->utils);
	goto done;
    }
    
    /*************************************/
    
    /* find out what we need to get */
    /* this corrupts const char *user */
    to_fetch = sparams->utils->prop_get(sparams->propctx);
    if(!to_fetch) goto done;
    
    /* now loop around hostnames till we get a connection 
    ** it should probably save the connection but for 
    ** now we will just disconnect eveyrtime
    */
    if ( settings->mysql_verbose )
	sparams->utils->log(NULL, SASL_LOG_WARN,
			    "mysql plugin try and connect to a host\n");
    
    /* create a working version of the hostnames */
    _plug_strdup(sparams->utils, settings->mysql_hostnames,
		 &db_host_ptr, NULL);
    db_host = db_host_ptr;
    cur_host = db_host;
    while ( cur_host != NULL ) {
	db_host = strchr(db_host,',');
	if ( db_host != NULL ) {  
	    db_host[0] = '\0';
	    /* loop till we find some text */
	    while (!isalnum(db_host[0]))
		db_host++;
	}
	
	if (settings->mysql_verbose)
	    sparams->utils->log(NULL, SASL_LOG_WARN,
				"mysql plugin trying to connect to %s\n",
				cur_host);

	if(mysql_init(&mysql) == NULL) {
	    sparams->utils->log(NULL, SASL_LOG_WARN,
				"mysql plugin: could not execute mysql_init");
	    goto done;
	}

	sock = mysql_real_connect(&mysql,cur_host,
				  settings->mysql_user,
			          settings->mysql_passwd,
				  NULL, 0, NULL, 0);
	if (sock) break;
	
	cur_host = db_host;
    }
    
    if ( !sock ) {
	sparams->utils->log(NULL, SASL_LOG_ERR,
			    "mysql plugin couldnt connect to any host\n");
	goto done;
    }
    /* escape out */
    mysql_real_escape_string(&mysql,escap_userid,userid,strlen(userid));
    mysql_real_escape_string(&mysql,escap_realm,realm,strlen(realm));
    /* connect to database */
    if (mysql_select_db(sock,settings->mysql_database) < 0) {
	goto done;
    }
    
    for(cur = to_fetch; cur->name; cur++) {
	char *realname = (char *)cur->name;
	/* Only look up properties that apply to this lookup! */
	if (cur->name[0] == '*'
	    && (flags & SASL_AUXPROP_AUTHZID))
	    continue;
	if(!(flags & SASL_AUXPROP_AUTHZID)) {
	    if(cur->name[0] != '*')
		continue;
	    else
		realname = (char*)cur->name + 1;
	}
	
	/* If it's there already, we want to see if it needs to be
	 * overridden */
	if(cur->values && !(flags & SASL_AUXPROP_OVERRIDE))
	    continue;
	else if(cur->values)
	    sparams->utils->prop_erase(sparams->propctx, cur->name);
	
	if ( settings->mysql_verbose )
	    sparams->utils->log(NULL, SASL_LOG_WARN,
			       "mysql plugin create statement from %s %s %s\n",
			       realname,escap_userid,escap_realm);
	
	/* create a statment that we will use */
	query = mysql_create_statement(sparams,
				       settings->mysql_statement,
				       realname,escap_userid,
				       escap_realm);
	
	if (settings->mysql_verbose)
	    sparams->utils->log(NULL, SASL_LOG_WARN,
				"mysql plugin doing query %s\n",
				query);
	
	/* run the query */
	if (mysql_query(sock,query) < 0
	    || !(result=mysql_store_result(sock))) {
	    sparams->utils->free(query);
	    continue;
	}
	
	/* quick row check */
	row_count = mysql_affected_rows(&mysql);
	if ( row_count == 0) {
	    /* umm nothing found */
	    sparams->utils->free(query);
	    mysql_free_result(result);
	    continue;
	}
	if ( row_count > 1 ) {
	    sparams->utils->log(NULL, SASL_LOG_WARN,
				"mysql plugin found duplicate (will take first) doing query %s \n",
				query);
	}
	
	/* now get the result set value and value_len */
	/* we only fetch one becuse we dont car about the rest */
	row = mysql_fetch_row(result);
	strncpy(value,row[0],8190);
	value_len = strlen(value);
	
	sparams->utils->prop_set(sparams->propctx, cur->name,
				 value, value_len);
	
	/* free result*/
	sparams->utils->free(query);
	mysql_free_result(result);
    }
    
 done:
    if (escap_userid) sparams->utils->free(escap_userid);
    if (escap_realm) sparams->utils->free(escap_realm);
    if (sock)  mysql_close(sock);
    if (db_host_ptr)  sparams->utils->free(db_host_ptr);
    if (userid) sparams->utils->free(userid);
    if (realm)  sparams->utils->free(realm);
    if (user_buf) sparams->utils->free(user_buf);
}

static void mysql_auxprop_free(void *glob_context, const sasl_utils_t *utils) {
    struct mysql_settings *settings;

    settings = (struct mysql_settings *)glob_context;

    if(!settings) return;

    if(settings->mysql_verbose)
	utils->log(NULL, SASL_LOG_DEBUG, "mysql freeing meme\n");

    utils->free(settings);
}

static sasl_auxprop_plug_t mysql_auxprop_plugin = {
    0,           /* Features */
    0,           /* spare */
    NULL,        /* glob_context */
    mysql_auxprop_free,        /* auxprop_free */
    mysql_auxprop_lookup, /* auxprop_lookup */
    "MYSQL",     /* name */
    NULL         /* spare */
};

int mysql_auxprop_plug_init(const sasl_utils_t *utils,
			    int max_version,
			    int *out_version,
			    sasl_auxprop_plug_t **plug,
			    const char *plugname __attribute__((unused))) 
{
    struct mysql_settings *settings;
    if(!out_version || !plug) return SASL_BADPARAM;
    
    if(max_version < SASL_AUXPROP_PLUG_VERSION) return SASL_BADVERS;
    *out_version = SASL_AUXPROP_PLUG_VERSION;
    
    *plug = &mysql_auxprop_plugin;
    
    /* should I get config values here or not 
    ** only testing will tell
    ** ok we need to get some options
    */
    
    settings =
	(struct mysql_settings *)utils->malloc(sizeof(struct mysql_settings));

    if(!settings) {
	MEMERROR(utils);
	return SASL_NOMEM;
    }

    mysql_auxprop_plugin.glob_context = settings;

    memset(settings, 0, sizeof(struct mysql_settings));
    
    return SASL_OK;
}



