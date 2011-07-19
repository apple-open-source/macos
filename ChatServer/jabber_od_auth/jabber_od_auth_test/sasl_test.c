#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#include <arpa/inet.h>
/* temporary work around to <rdar://problem/8196059> */
#include <ldap.h>

#include "../sasl_switch_hit.h"
//TODO: CLEANUP XXX
#define main __unused_main
#define usage __unused_usage
#define argv0 A0
#include "membership_test.c"
#undef main
#undef usage
#undef argv0

#define kAuthMethod                "DIGEST-MD5"

// AD test user on ets.apple.com
#define kADUserName                "digesttest"
#define kADUserPass                "password"

#define kLDAPUserName            "diradmin"
#define kLDAPUserPass            "admin"

#define kLocalUserName            "admin"
#define kLocalUserPass            "admin"

#define kSASLMinSecurityFactor        0
#define kSASLMaxSecurityFactor        65535
#define kSASLMaxBufferSize            65536
#define kSASLSecurityFlags            0
#define kSASLPropertyNames            (NULL)
#define kSASLPropertyValues            (NULL)

typedef struct saslContext {
    const char *user;
    const char *pass;
} saslContext;

char *argv0;
int logging = 1;

#pragma mark -
#pragma mark Prototypes
#pragma mark -

int getrealm(void *context /*__attribute__((unused))*/,
            int cb_id,
            const char **availrealms,
            const char **result);
int simple(void *context /*__attribute__((unused))*/,
          int cb_id,
          const char **result,
          unsigned *len);
int
getsecret(sasl_conn_t *conn,
      void *context /*__attribute__((unused))*/,
      int cb_id,
      sasl_secret_t **psecret);

sasl_conn_t *SASLClientNewContext( sasl_callback_t *callbacks, saslContext *context );
char *GetDigestMD5ChallengeFromSASL(char *userName, char *realm, sasl_conn_t **conn);
int auth_test(char *realm, char *inUsername, char *password);

#pragma mark -
#pragma mark SASL Support
#pragma mark -

typedef int sasl_cbproc();

int getrealm(void *context /*__attribute__((unused))*/,
            int cb_id,
            const char **availrealms,
            const char **result)
{
    #pragma unused (context)

    /* paranoia check */
    if (cb_id != SASL_CB_GETREALM) return SASL_BADPARAM;
    if (!result) return SASL_BADPARAM;

    if ( availrealms ) {
        *result = *availrealms;
    }

    return SASL_OK;
}


int simple(void *context,
          int cb_id,
          const char **result,
          unsigned *len)
{
    saslContext *text = (saslContext *)context;

    //syslog(LOG_INFO, "in simple\n");

    /* paranoia check */
    if ( result == NULL )
        return SASL_BADPARAM;

    *result = NULL;

    switch (cb_id) {
        case SASL_CB_USER:
            *result = text->user;
            break;

        case SASL_CB_AUTHNAME:
            *result = text->user;
            break;

        default:
            return SASL_BADPARAM;
    }

    if (*result != NULL && len != NULL)
        *len = strlen(*result);

    return SASL_OK;
}


int
getsecret(sasl_conn_t *conn,
      void *context,
      int cb_id,
      sasl_secret_t **psecret)
{
    saslContext *text = (saslContext *)context;

    //syslog(LOG_INFO, "in getsecret\n");

    /* paranoia check */
    if (! conn || ! psecret || cb_id != SASL_CB_PASS)
        return SASL_BADPARAM;

    size_t pwdLen = strlen(text->pass);
    *psecret = (sasl_secret_t *) malloc( sizeof(sasl_secret_t) + pwdLen );
    (*psecret)->len = pwdLen;
    strcpy((char *)(*psecret)->data, text->pass);

    return SASL_OK;
}


//----------------------------------------------------------------------------------------
//    SASLClientNewContext
//
//    Returns: A SASL context, or NULL
//
//    <callbacks> must be an array with capacity for at least 5 items
//----------------------------------------------------------------------------------------

sasl_conn_t *SASLClientNewContext( sasl_callback_t *callbacks, saslContext *context )
{
    int result = 0;
    sasl_conn_t *sasl_conn = NULL;
    sasl_security_properties_t secprops = { kSASLMinSecurityFactor, kSASLMaxSecurityFactor,
                                            kSASLMaxBufferSize, kSASLSecurityFlags,
                                            kSASLPropertyNames, kSASLPropertyValues };
    /* temporary work around to <rdar://problem/8196059> */
    LDAP *ldap_con = NULL; 
    ldap_initialize(&ldap_con, "ldap://127.0.0.1");

    result = sasl_client_init( NULL );
    if (logging) printf( "sasl_client_init = %d\n", result );
    if ( result != SASL_OK ) {
        ldap_unbind(ldap_con);
        return NULL;
    }

    // callbacks we support
    callbacks[0].id = SASL_CB_GETREALM;
    callbacks[0].proc = (sasl_cbproc *)&getrealm;
    callbacks[0].context = context;

    callbacks[1].id = SASL_CB_USER;
    callbacks[1].proc = (sasl_cbproc *)&simple;
    callbacks[1].context = context;

    callbacks[2].id = SASL_CB_AUTHNAME;
    callbacks[2].proc = (sasl_cbproc *)&simple;
    callbacks[2].context = context;

    callbacks[3].id = SASL_CB_PASS;
    callbacks[3].proc = (sasl_cbproc *)&getsecret;
    callbacks[3].context = context;

    callbacks[4].id = SASL_CB_LIST_END;
    callbacks[4].proc = NULL;
    callbacks[4].context = NULL;

    result = sasl_client_new( "http", "simost1.apple.com", NULL, NULL, callbacks, 0, &sasl_conn );
    if (logging) printf( "sasl_client_new = %d\n", result );
    if ( result != SASL_OK ) {
        ldap_unbind(ldap_con);
        return NULL;
    }

    result = sasl_setprop( sasl_conn, SASL_SEC_PROPS, &secprops );
    if (logging) printf( "sasl_setprop = %d\n", result );
    if ( result != SASL_OK ) {
        sasl_dispose( &sasl_conn );
        ldap_unbind(ldap_con);
        return NULL;
    }

    ldap_unbind(ldap_con);

    return sasl_conn;
}

#pragma mark -
#pragma mark DIGEST-MD5 Server Challenge
#pragma mark -

//----------------------------------------------------------------------------------------
//    GetDigestMD5ChallengeFromSASL
//
//    Returns: A server challenge for DIGEST-MD5 authentication
//----------------------------------------------------------------------------------------

char *GetDigestMD5ChallengeFromSASL(char *userName, char *realm, sasl_conn_t **conn)
{
//TODO: USE userName if it's non-null
    int result = 0;
    sasl_conn_t *sasl_server_conn = NULL;
    char *serverOut = 0;
    unsigned int serverOutLen = 0;

        *conn = 0;

    sasl_security_properties_t secprops = { kSASLMinSecurityFactor, kSASLMaxSecurityFactor,
                        kSASLMaxBufferSize, kSASLSecurityFlags,
                        kSASLPropertyNames, kSASLPropertyValues };

    /* temporary work around to <rdar://problem/8196059> */  
    LDAP *ldap_con = NULL; 
    ldap_initialize(&ldap_con, "ldap://127.0.0.1");
	
    // Get a challenge from SASL
    //result = sasl_server_init( NULL, "AppName" );
    result = sasl_server_init_alt( NULL, "AppName" );
    if ( result != SASL_OK ) {
        if (logging) printf( "sasl_server_init_alt = %d\n", result );
        goto done;
    }

        /* wire up ODKit to libsasl */
    result = sasl_switch_hit_register_apple_digest_md5();
    if (result != SASL_OK ) {
        if (logging) printf( "sasl_switch_hit_register_apple_digest_md5 = %d\n", result );
        goto done;
    }

    //"127.0.0.1;80"
    result = sasl_server_new( "http", realm, NULL, NULL, NULL, NULL, 0, &sasl_server_conn );
    if ( result != SASL_OK ) {
        if (logging) printf( "sasl_server_init_alt = %d\n", result );
        goto done;
    }

    result = sasl_setprop( sasl_server_conn, SASL_SEC_PROPS, &secprops );
    if ( result != SASL_OK ) {
        if (logging) printf( "sasl_setprop = %d\n", result );
        goto done;
    }

    result = sasl_server_start( sasl_server_conn, kAuthMethod, NULL, 0, (const char**)&serverOut, &serverOutLen );
    if ( result != SASL_CONTINUE ) {
        if (logging) printf( "sasl_server_start = %d\n", result );
        goto done;
    }

        *conn = sasl_server_conn;
done:
    ldap_unbind(ldap_con);
    return serverOut;
}

int
usage()
{
    fprintf(stderr, "usage: %s username realm password\n", argv0);
    fprintf(stderr, "       %s --unittest\n", argv0);
    fprintf(stderr, "       %s iterations username realm password\n", argv0);
    fprintf(stderr, "       username, realm, and password are passed to sprintf, where %%d is the index (1..iterations)\n");
    fprintf(stderr, "       (eg %s 1000 'testuser%%d' 'od%%d.apple.com' 'pass%%d')\n", argv0);
    exit(1);
}

int
main_iterate(int argc, const char * argv[])
{
    if (argc != 5)
        usage();

    if (! isdigit(argv[1][0]))
        usage();

    int iterations = atoi(argv[1]);
    fprintf(stderr, "Starting %d iterations\n", iterations);

    logging = 0;

    int i;
    int auths_succeeded = 0;
    for (i = 1; i <= iterations; ++i) {
        char username[1024];
        char realm[1024];
        char password[1024];

        // lots of i in case %d is used multiple times
        snprintf(username, sizeof(username), argv[2], i, i, i, i, i, i);
        snprintf(realm, sizeof(realm), argv[3], i, i, i, i, i, i);
        snprintf(password, sizeof(password), argv[4], i, i, i, i, i, i);

        if (auth_test(realm, username, password) == 0)
            ++auths_succeeded;

//TODO:CLEANUP XXX
//od_auth_check_service_membership(username, APPLE_CHAT_SACL_NAME);
    }

    fprintf(stderr, "auths_succeeded: %d (of %d): %s\n", auths_succeeded, iterations,
                                                         (auths_succeeded == iterations) ? "SUCCESS" : "FAILURE");

    if (auths_succeeded != iterations)
        exit(1);

    return 0;
}

#pragma mark -
#pragma mark MAIN
#pragma mark -

int
unit_test()
{
    int result = 0;
    char *realm = "bkorver3.apple.com";

    logging = 1;

    result |= auth_test(realm, kADUserName, kADUserPass);
    printf("\n");

    result |= auth_test(realm, kLDAPUserName, kLDAPUserPass);
    printf("\n");

    result |= auth_test(realm, kLocalUserName, kLocalUserPass);
    printf("\n");

    return result;
}

int main( int argc, const char * argv[] )
{
    int result = 0;

    argv0 = (char*)argv[0];
    if (strrchr(argv0, '/'))
        argv0 = strrchr(argv0, '/') + 1;

    switch (argc) {
    case 2:
        result = unit_test();
        break;
    case 4:
        result = auth_test((char*)argv[2], (char*)argv[1], (char*)argv[3]);
        break;
    case 5:
        result = main_iterate(argc, (const char**)argv);
        break;
    default:
        usage();
        break;
    }

    sasl_done();
    return result;
}


int
auth_test(char *realm, char *inUsername, char *password)
{
    int retval = -1;
    int result = 0;
    sasl_callback_t callbacks[5] = {{0}};
    saslContext sasl_context = { NULL, NULL };
    sasl_conn_t *sasl_conn = NULL;
    sasl_conn_t *sasl_server_conn = NULL;
    const char *data = NULL;
    unsigned len = 0;
    unsigned slen = 0;
    const char *chosenmech = NULL;

    char *serverChal = 0;
    char *serverResp = 0;

    /* temporary work around to <rdar://problem/8196059> */
    LDAP *ldap_con = NULL; 
    ldap_initialize(&ldap_con, "ldap://127.0.0.1");

    sasl_context.user = inUsername;
    sasl_context.pass = password;

    // Client's first move
    sasl_conn = SASLClientNewContext( callbacks, &sasl_context );

    result = sasl_client_start( sasl_conn, kAuthMethod, NULL, &data, &len, &chosenmech );
    if (logging) printf( "sasl_client_start = %d, len = %d\n", result, len );
    if ( result != SASL_CONTINUE )
        goto done;

    // Get DIGEST-MD5 challenge
    serverChal = GetDigestMD5ChallengeFromSASL(inUsername, realm, &sasl_server_conn);
    if ( serverChal != 0 ) {
        if (logging) printf("server challenge = %s\n", serverChal);
    }
    else {
        if (logging) printf("no server challenge\n");
        goto done;
    }

    // Server forwards the challenge to the client.

    // Client hashes the digest and responds
    result = sasl_client_step(sasl_conn, serverChal, strlen(serverChal), NULL, &data, &len);
    if (result != SASL_CONTINUE) {
        if (logging) printf("sasl_client_step = %d\n", result);
        goto done;
    }

    if (logging) printf("client response = \n%s\n", data);

    // Client sends the response to the server.

    result = sasl_server_step(sasl_server_conn, data, len, (const char**)&serverResp, &slen);
    if (result != SASL_CONTINUE) {
        if (logging) printf("sasl_server_step = %d\n", result);
        goto done;
    }

    if (logging) printf("server response = %s\n", serverResp ? serverResp : "nil");

    // The Server forwards the mutual authentication data
    // The client can verify.
    result = sasl_client_step(sasl_conn, serverResp, strlen(serverResp), NULL, &data, &len);
    if (result == SASL_OK) {
        if (logging) printf("authentication succeeded.\n");
    }
    else {
        if (logging) printf("sasl_client_step (mutual auth data) = %d\n", result);
        goto done;
    }

    retval = 0;
done:
    if ( sasl_conn != NULL )
        sasl_dispose( &sasl_conn );

    if ( sasl_server_conn != NULL )
        sasl_dispose( &sasl_server_conn );

    ldap_unbind(ldap_con);

    return retval;
}

