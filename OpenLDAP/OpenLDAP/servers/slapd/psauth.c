#include "portable.h"
#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <stdarg.h>
#include <errno.h>

#include <unistd.h>

#include <syslog.h>

#include "psauth.h"
#include <sasl.h>
#include "saslutil.h"
#include "saslplug.h"
#include <lber.h>
#include "slap.h"

#undef calloc
#undef free

/* In sasl.c */
extern int slap_sasl_log( void *context, int priority, const char *message);
extern int slap_sasl_getopt( void *context, const char *plugin_name, const char *option, const char **result, unsigned *len);
extern int pws_auxprop_init( const sasl_utils_t *utils, int max_version, int *out_version, sasl_auxprop_plug_t **plug, const char *plugname);
static const char *slap_propnames[] = {
    "*slapConn", "*slapAuthcDNlen", "*slapAuthcDN",
    "*slapAuthzDNlen", "*slapAuthzDN", NULL };
#define SLAP_SASL_PROP_CONN 0
#define SLAP_SASL_PROP_AUTHCLEN 1
#define SLAP_SASL_PROP_AUTHC    2
#define SLAP_SASL_PROP_AUTHZLEN 3
#define SLAP_SASL_PROP_AUTHZ    4
#define SLAP_SASL_PROP_COUNT    5   /* Number of properties we used */

typedef struct sSASLContext {
	sasl_secret_t *secret;
	char username[35];
} sSASLContext;

typedef struct sSASLCanonCtx {
	Connection *conn;
	const char *dn;
} sSASLCanonCtx;

int getrealm(void *context /*__attribute__((unused))*/, 
		    int id,
		    const char **availrealms,
		    const char **result);
int ol_simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len);
int
ol_getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret);

//-------------
int getrealm(void *context /*__attribute__((unused))*/,
            int id,
            const char **availrealms,
            const char **result)
{
    /* paranoia check */
    if (id != SASL_CB_GETREALM) return SASL_BADPARAM;
    if (!result) return SASL_BADPARAM;

    if ( availrealms ) {
        *result = *availrealms;
    }
   
    return SASL_OK;
}

int ol_simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len)
{
#ifdef DEBUG_PRINTFS
    printf("in simple\n");
#endif

    /* paranoia check */
    if (! result)
        return SASL_BADPARAM;
    
    switch (id) {
        case SASL_CB_USER:
        case SASL_CB_AUTHNAME:
            //printf("please enter an authorization id: ");
            *result = ((sSASLContext *)context)->username;
            if (len != NULL)
                *len = strlen(((sSASLContext *)context)->username);
#ifdef DEBUG_PRINTFS
            printf("simple - user = %s (len = %d)\n", *result, *len);
#endif
            break;
        
        default:
            return SASL_BADPARAM;
    }
  
    return SASL_OK;
}


int
ol_getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret)
{    
#ifdef DEBUG_PRINTFS
    printf("in getsecret\n");
#endif
    /* paranoia check */
    if (! conn || ! psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;
    
    *psecret = ((sSASLContext *)context)->secret;
    return SASL_OK;
}

int CheckAuthType(char* inAuthAuthorityData, char* authType)
{
    char* temp;
    temp = strchr(inAuthAuthorityData, ';');
    return ((temp != NULL) && (strncmp(temp+1, authType, strlen(authType)) == 0));
}

int
pws_canonicalize(
    sasl_conn_t *sconn,
    void *context,
    const char *in,
    unsigned inlen,
    unsigned flags,
    const char *user_realm,
    char *out,
    unsigned out_max,
    unsigned *out_len)
{
	sSASLCanonCtx *ctx = (sSASLCanonCtx*)context;
	struct propctx *props = sasl_auxprop_getctx( sconn );
	struct propval auxvals[ SLAP_SASL_PROP_COUNT ] = { { 0 } };
	const char *names[2];
	ber_len_t blen;

	prop_getnames(props, slap_propnames, auxvals);
	if(!auxvals[0].name)
		prop_request(props, slap_propnames);

	names[0] = slap_propnames[SLAP_SASL_PROP_CONN];
	names[1] = NULL;
	prop_set(props, names[0], (char*)&ctx->conn, sizeof(ctx->conn));
	
	names[0] = slap_propnames[SLAP_SASL_PROP_AUTHCLEN];

	blen = strlen(ctx->dn);
	prop_set(props, names[0], (char*)&blen, sizeof(blen));

	names[0] = slap_propnames[SLAP_SASL_PROP_AUTHC];
	prop_set(props, names[0], ctx->dn, blen);

	if(out_max < (inlen+1)) return SASL_BADPARAM;
	memcpy(out, in, inlen);
	out[inlen] = '\0';
	*out_len = inlen;
	return SASL_OK;
}

int DoPSAuth(char* userName, char* password, char* inAuthAuthorityData, Connection* conn, const char* dn)
{
	int result = -1;
	sasl_conn_t *client_conn;
	sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
	sasl_callback_t callbacks[5];
	const char *data = NULL;
	unsigned int len = 0;
	const char *chosenmech = NULL;
	sSASLContext saslContext;
	sasl_conn_t *server_conn = NULL;
	const char *serverOut = NULL;
	unsigned int serverOutLen = 0;
	sasl_security_properties_t server_secprops = {0, 65536, 4096, SASL_SEC_NOPLAINTEXT, NULL, NULL};
    static sasl_callback_t server_callbacks[] = {
        { SASL_CB_LOG, &slap_sasl_log, NULL },
        { SASL_CB_GETOPT, &slap_sasl_getopt, NULL },
        { SASL_CB_LIST_END, NULL, NULL }
    };
	sasl_callback_t server_session_callbacks[5];
	struct propctx *props = NULL;
	struct propval auxvals[SLAP_SASL_PROP_COUNT] = {{0}};
	const char *names[2];
	ber_len_t blen;
	sSASLCanonCtx canonctx;

	canonctx.conn = conn;
	canonctx.dn = dn;

	/* server session callbacks */
	server_session_callbacks[0].id = SASL_CB_LOG;
	server_session_callbacks[0].proc = &slap_sasl_log;
	server_session_callbacks[0].context = conn;
	server_session_callbacks[1].id = SASL_CB_CANON_USER;
	server_session_callbacks[1].proc = &pws_canonicalize;
	server_session_callbacks[1].context = &canonctx;
	server_session_callbacks[2].id = SASL_CB_LIST_END;
	server_session_callbacks[2].proc = NULL;
	server_session_callbacks[2].context = NULL;

	memset(&saslContext, 0, sizeof(saslContext));
	strncpy(saslContext.username, userName, 35);
	saslContext.secret = (sasl_secret_t*)calloc(1, sizeof(sasl_secret_t) + strlen(password) + 1);
	saslContext.secret->len = strlen(password);
	strcpy((char*)saslContext.secret->data, password);

	/* client side callbacks */
    callbacks[0].id = SASL_CB_GETREALM;
    callbacks[0].proc = &getrealm;
    callbacks[0].context = &saslContext;
   
    callbacks[1].id = SASL_CB_USER;
    callbacks[1].proc = &ol_simple;
    callbacks[1].context = &saslContext;

    callbacks[2].id = SASL_CB_AUTHNAME;
    callbacks[2].proc = &ol_simple;
    callbacks[2].context = &saslContext;

    callbacks[3].id = SASL_CB_PASS;
    callbacks[3].proc = &ol_getsecret;
    callbacks[3].context = &saslContext;
   
    callbacks[4].id = SASL_CB_LIST_END;
    callbacks[4].proc = NULL;
    callbacks[4].context = NULL;
	
	result = sasl_client_init(NULL);
	if(result != SASL_OK) {
		syslog(LOG_ERR, "sasl_client_init returned: %d", result);
		goto out;
	}

	result = sasl_client_new("slapd", "localhost", NULL, NULL, callbacks, 0, &client_conn);
	if ( result != SASL_OK ) {
		syslog(LOG_ERR, "sasl_client_new returned: %d", result);
		goto out;
	}
	
	result = sasl_setprop(client_conn, SASL_SEC_PROPS, &secprops);

	result = sasl_client_start(client_conn, "CRAM-MD5", NULL, &data, &len, &chosenmech);
	if(result != SASL_CONTINUE) {
		syslog(LOG_ERR, "sasl_client_start returned: %d", result);
		goto out;
	}

	result = sasl_auxprop_add_plugin( "appleldap", pws_auxprop_init );
	if( result != SASL_OK ) {
		syslog(LOG_ERR, "sasl_auxprop_add_plugin returned: %d", result);
		goto out;
	}

	result = sasl_set_path(SASL_PATH_TYPE_PLUGIN, "/usr/lib/sasl2/openldap");
	if(result != SASL_OK) {
		syslog(LOG_ERR, "sasl_set_path returned: %d", result);
		goto out;
	}

	result = sasl_server_init(server_callbacks, "slapd");
	if(result != SASL_OK) {
		syslog(LOG_ERR, "sasl_server_init_alt returned: %d", result);
		goto out;
	}

	result = sasl_server_new("slapd", "localhost", NULL, NULL, NULL, server_session_callbacks, SASL_SUCCESS_DATA, &server_conn);
	if(result != SASL_OK) {
		syslog(LOG_ERR, "sasl_server_new returned: %d", result);
		goto out;
	}

	result = sasl_setprop(server_conn, SASL_SEC_PROPS, &server_secprops);

	result = sasl_server_start(server_conn, "CRAM-MD5", data, len, &serverOut, &serverOutLen);
	if(result != SASL_CONTINUE) {
		syslog(LOG_ERR, "sasl_server_start returned: %d", result);
		goto out;
	}

	result = sasl_client_step(client_conn, serverOut, serverOutLen, NULL, &data, &len);
	if(result != SASL_OK) {
		syslog(LOG_ERR, "sasl_client_step returned: %d", result);
		goto out;
	}

	result = sasl_server_step(server_conn, data, len, &serverOut, &serverOutLen);

out:
	if(client_conn) sasl_dispose(&client_conn);
	if(server_conn) sasl_dispose(&server_conn);
	if(saslContext.secret) free(saslContext.secret);

	return result;
}
