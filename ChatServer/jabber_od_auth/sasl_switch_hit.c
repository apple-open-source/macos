/*
 *  sasl_switch_hit.c
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#include <stdio.h>
#include <assert.h>
#include <sasl/sasl.h>
#include <sasl/saslplug.h>
#include <sasl/saslutil.h>
#include <string.h>
#include <CoreSymbolication/CoreSymbolication.h>

#include "sasl_switch_hit.h"
#include "odckit.h"

#define kSASLMinSecurityFactor          0
#define kSASLMaxSecurityFactor          0
#define kSASLMaxBufferSize              65536
#define kSASLSecurityFlags              0
#define kSASLPropertyNames              (NULL)
#define kSASLPropertyValues             (NULL)

typedef struct sasl_switch_hit_context {
    int            step;
    ODCKSession   *session;
    char           username[1024];
    unsigned int   usernamelen;
    void          *conn_context;
} sasl_switch_hit_context;

#define SETERROR(utils, msg) \
        sasl_switch_hit_set_error((utils), (msg))

void sasl_switch_hit_set_error(const sasl_utils_t *utils, const char *msg);

static int sasl_switch_hit_server_mech_new(void *glob_context,
                                     sasl_server_params_t * sparams,
                                     const char *challenge __attribute__((unused)),
                                     unsigned challen __attribute__((unused)),
                                     void **conn_context);
static int sasl_switch_hit_server_mech_step1(sasl_switch_hit_context *text,
                            sasl_server_params_t *sparams,
                            const char *clientin __attribute__((unused)),
                            unsigned clientinlen __attribute__((unused)),
                            const char **serverout,
                            unsigned *serveroutlen,
                            sasl_out_params_t * oparams);
static int sasl_switch_hit_server_mech_step2(sasl_switch_hit_context *text,
                                       sasl_server_params_t *sparams,
                                       const char *clientin,
                                       unsigned clientinlen,
                                       const char **serverout,
                                       unsigned *serveroutlen,
                                       sasl_out_params_t * oparams);
static int sasl_switch_hit_server_mech_step(void *conn_context,
                                      sasl_server_params_t *sparams,
                                      const char *clientin,
                                      unsigned clientinlen,
                                      const char **serverout,
                                      unsigned *serveroutlen,
                                      sasl_out_params_t *oparams);
static void sasl_switch_hit_server_mech_dispose(void *conn_context,
                                          const sasl_utils_t *utils);
static void sasl_switch_hit_server_mech_free(void *glob_context,
                                       const sasl_utils_t *utils);
static int sasl_switch_hit_set_username(sasl_switch_hit_context *text, sasl_server_params_t *sparams, char *username, unsigned int usernamelen, sasl_out_params_t *oparams);
static int sasl_switch_hit_set_authid(sasl_server_params_t *sparams, char *authid, sasl_out_params_t *oparams);
static char* sasl_switch_hit_get_username(sasl_switch_hit_context *text, sasl_server_params_t *sparams, sasl_out_params_t *oparams);

static sasl_server_plug_init_t sasl_switch_hit_server_plug_init;
static int sasl_switch_hit_grab_plugin_to_override(const sasl_utils_t *utils);

static sasl_server_plug_t sasl_switch_hit_server_plugins[1];

static sasl_server_plug_t *sasl_switch_hit_plugin_to_override = 0;

void
sasl_switch_hit_set_error(const sasl_utils_t *utils, const char *msg)
{
    assert(utils != 0 && msg != 0);
    if (utils != 0 && msg != 0)
        utils->seterror(utils->conn, 0, "%s", msg);
}

int
sasl_switch_hit_server_mech_new(void *glob_context,
                   sasl_server_params_t * sparams,
                   const char *challenge __attribute__((unused)),
                   unsigned challen __attribute__((unused)),
                   void **conn_context)
{
    int retval = SASL_NOMEM;
    sasl_switch_hit_context *text = 0;

    assert(sasl_switch_hit_plugin_to_override != 0);

    *conn_context = 0;

    text = sparams->utils->calloc(1, sizeof(*text));
    if (text == 0) {
        SETERROR(sparams->utils, "Out of memory");
        goto done;
    }

    if (ODCKCreateSession(/*(char*)sparams->appname, (char*)sparams->service, 0,*/ &text->session)) {
        SETERROR(sparams->utils, "Unable to create session");
        goto done;
    }

    retval = sasl_switch_hit_plugin_to_override->mech_new(glob_context, sparams, challenge, challen, &text->conn_context);
    if (retval != SASL_OK)
        goto done;

    *conn_context = (void*)text;

    retval = SASL_OK;

done:
    if (retval != SASL_OK) {
        if (text != 0) {
            if (text->session != 0) {
                ODCKDeleteSession(&text->session);
            }
            sparams->utils->free(text);
        }
    }

    return retval;
}

int 
sasl_switch_hit_set_username(sasl_switch_hit_context *text, sasl_server_params_t *sparams, char *username, unsigned int usernamelen, sasl_out_params_t *oparams)
{
    if (usernamelen >= sizeof(text->username)-1) {
        SETERROR(sparams->utils, "Username too long");
        return SASL_FAIL;
    }

    if (text->username[0] == '\0') {
        memcpy(text->username, username, usernamelen);
        text->username[usernamelen] = '\0';
        text->usernamelen = usernamelen;
    }

    oparams->user = text->username;
    oparams->ulen = text->usernamelen;

    assert(strlen(text->username) == usernamelen);

    return 0;
}

int 
sasl_switch_hit_set_authid(sasl_server_params_t *sparams, char *authid, sasl_out_params_t *oparams)
{
    int result = sparams->canon_user(sparams->utils->conn,
                                     authid, 0, SASL_CU_AUTHID, oparams);
    if (result != SASL_OK)
        SETERROR(sparams->utils, sasl_errdetail(sparams->utils->conn));

    return result;
}

char*
sasl_switch_hit_get_username(sasl_switch_hit_context *text, sasl_server_params_t *sparams, sasl_out_params_t *oparams)
{
    char *username = 0;
    unsigned long usernamelen;
    int (*getusername)() = 0;
    void *ctx = 0;

    if (sasl_getprop(sparams->utils->conn, SASL_USERNAME, (const void**)&username)) {
        if (_sasl_getcallback(sparams->utils->conn, SASL_CB_USER,
                              &getusername, &ctx) == SASL_OK) {
            getusername(ctx, SASL_CB_USER, &username, &usernamelen);
            if (username)
                sasl_switch_hit_set_username(text, sparams, username, (unsigned int)usernamelen, oparams);
        }
    }

    return username;
}

int
sasl_switch_hit_server_mech_step1(sasl_switch_hit_context *text,
                     sasl_server_params_t *sparams,
                     const char *clientin __attribute__((unused)),
                     unsigned clientinlen __attribute__((unused)),
                     const char **serverout,
                     unsigned *serveroutlen,
                     sasl_out_params_t * oparams)
{
    int retval = SASL_FAIL;
    const char *username = sasl_switch_hit_get_username(text, sparams, oparams);
    const char *chall = 0;
    unsigned int challlen;

    text->step = 1;

    *serverout = 0;
    *serveroutlen = 0;

    if (ODCKGetServerChallenge(text->session, "DIGEST-MD5", username,
                               (char**)&chall, &challlen) == 0) {
        *serverout = chall;
        *serveroutlen = challlen;
        retval = SASL_CONTINUE;
    }
    else {
        /* unable or unwilling to use DS to generate the challenge, so do
         * it manually and hope for the best */

        static sasl_security_properties_t secprops = {
                                kSASLMinSecurityFactor, kSASLMaxSecurityFactor,
                                kSASLMaxBufferSize, kSASLSecurityFlags,
                                kSASLPropertyNames, kSASLPropertyValues };
        /* if this call fails, just continue, hoping for the best */
        (void)sasl_setprop(sparams->utils->conn, SASL_SEC_PROPS, &secprops);

        retval = sasl_switch_hit_plugin_to_override->mech_step(
                                      text->conn_context, sparams,
                                      clientin, clientinlen,
                                      &chall, &challlen, oparams);
        if (retval == SASL_CONTINUE) {
            if (ODCKSetServerChallenge(text->session, "DIGEST-MD5", username,
                                       chall, challlen) == 0) {
                *serverout = chall;
                *serveroutlen = challlen;
            }
            else {
                SETERROR(sparams->utils, ODCKGetError(text->session));
                retval = SASL_FAIL;
            }
        }
        else {
            assert(retval != SASL_OK);
            SETERROR(sparams->utils, sasl_errdetail(sparams->utils->conn));
        }
    }

    return retval;
}

int
sasl_switch_hit_server_mech_step2(sasl_switch_hit_context *text,
                     sasl_server_params_t *sparams,
                     const char *clientin,
                     unsigned clientinlen,
                     const char **serverout,
                     unsigned *serveroutlen,
                     sasl_out_params_t * oparams)
{
    int retval = SASL_FAIL;
    char *authid = 0;
    unsigned int serverlen;

    text->step = 2;

    *serverout = 0;
    *serveroutlen = 0;

    /* always use DS, never call into the manual SASL code */

    if (ODCKVerifyClientRequest(text->session, (char*)clientin, (unsigned int)clientinlen) != 0) {
        SETERROR(sparams->utils, ODCKGetError(text->session));
        goto done;
    }

    if (ODCKGetServerResponse(text->session, &authid, (char**)serverout, &serverlen)) {
        SETERROR(sparams->utils, ODCKGetError(text->session));
        goto done;
    }

    /* Before returning SASL_OK, mech_step must fill in the oparams fields for which it is
     * responsible, that is, doneflag (set to 1 to indicate a complete exchange), maxoutbuf,
     * or the maximum output size it can do at once for a security layer, mech_ssf or the
     * supplied SSF of the security layer, and encode, decode, encode_context, and
     * decode_context, which are what the glue code will call on calls to sasl_encode,
     * sasl_encodev, and sasl_decode. */

    if (sasl_switch_hit_get_username(text, sparams, oparams) == 0) {
        (void)sasl_switch_hit_set_username(text, sparams,
                              authid, (unsigned int)strlen(authid),
                              oparams);
    }

    (void)sasl_switch_hit_set_authid(sparams, authid, oparams);

    oparams->doneflag = 1;
    oparams->mech_ssf = 0;
    oparams->maxoutbuf = 0;
    oparams->encode_context = NULL;
    oparams->encode = NULL;
    oparams->decode_context = NULL;
    oparams->decode = NULL;
    oparams->param_version = 0;

    *serveroutlen = serverlen;

    /* get rid of any expensive resources that are no longer needed */
    (void)ODCKFlushSession(text->session);

    retval = SASL_OK;
done:
    return retval;
}

int
sasl_switch_hit_server_mech_step(void *conn_context,
                    sasl_server_params_t *sparams,
                    const char *clientin,
                    unsigned clientinlen,
                    const char **serverout,
                    unsigned *serveroutlen,
                    sasl_out_params_t *oparams)
{
    sasl_switch_hit_context *text = (sasl_switch_hit_context*)conn_context;

    if (text == 0) {
        /* likely a failure occurred, and _step is being called
         * again erroneously */ 
        SETERROR(sparams->utils, "Illegal call");
        return SASL_FAIL;
    }

    if (clientinlen > 4096) return SASL_BADPROT;

    *serverout = 0;
    *serveroutlen = 0;
    
    switch (text->step) {
    case 0:
        return sasl_switch_hit_server_mech_step1(text, sparams,
                                    clientin, clientinlen,
                                    serverout, serveroutlen, oparams);
        /* should never get here */
        break;
    case 1:
        return sasl_switch_hit_server_mech_step2(text, sparams,
                                    clientin, clientinlen,
                                    serverout, serveroutlen, oparams);
        /* should never get here */
        break;
    }

    /* should never get here */

    SETERROR(sparams->utils, "Invalid DIGEST-MD5 server step");
    return SASL_FAIL;
}

void
sasl_switch_hit_server_mech_dispose(void *conn_context, const sasl_utils_t *utils)
{
    sasl_switch_hit_context *text = (sasl_switch_hit_context*)conn_context;
    if (text != 0) {
        if (text->conn_context != 0)
            sasl_switch_hit_plugin_to_override->mech_dispose(text->conn_context, utils);
        if (text->session != 0)
            ODCKDeleteSession(&text->session);
        utils->free(text);
    }

    return;
}

void
sasl_switch_hit_server_mech_free(void *glob_context, const sasl_utils_t *utils)
{
    return;
}

int
sasl_switch_hit_grab_plugin_to_override(const sasl_utils_t *utils)
{
    int retval = -1;
    const char *symbname = "digestmd5_server_plugins";

    CSSymbolicatorRef symbolicator = CSSymbolicatorCreateWithTask(mach_task_self());
    if (CSIsNull(symbolicator)) {
        sasl_switch_hit_set_error(utils, "Unable to obtain symbolicator");
        goto done;
    }

    CSSymbolRef symbol = CSSymbolicatorGetSymbolWithNameFromSymbolOwnerWithNameAtTime(symbolicator, symbname, "libdigestmd5.2.so", kCSNow);
    if (CSIsNull(symbol)) {
        sasl_switch_hit_set_error(utils, "Unable to find symbol");
        goto done;
    }

    CSRange range = CSSymbolGetRange(symbol);

#if defined( __x86_64__ )
    void *addr = (void*)range.location;
#else
    void *addr = (void*)(unsigned int)range.location;
#endif
    sasl_switch_hit_plugin_to_override = (sasl_server_plug_t*)addr;

    if (sasl_switch_hit_plugin_to_override == 0) {
        sasl_switch_hit_set_error(utils, "Unable to find address");
        goto done;
    }

    if (strcmp("DIGEST-MD5", sasl_switch_hit_plugin_to_override->mech_name) != 0) {
        /* this should really, really never happen */
        sasl_switch_hit_set_error(utils, "Illegal plugin");
        goto done;
    }

    retval = 0;
done:
    CSRelease(symbolicator);

    return retval;
}

int
sasl_switch_hit_server_plug_init(const sasl_utils_t *utils,
                    int maxversion,
                    int *out_version,
                    sasl_server_plug_t **pluglist,
                    int *plugcount) 
{
    if (maxversion < SASL_SERVER_PLUG_VERSION)
        return SASL_BADVERS;

    if (sasl_switch_hit_plugin_to_override == 0) {
        if (sasl_switch_hit_grab_plugin_to_override(utils) != 0)
            return SASL_FAIL;
    }

    /* imitate everything inside the plugin that is being overridden ... */
    memcpy(sasl_switch_hit_server_plugins, sasl_switch_hit_plugin_to_override, sizeof(*sasl_switch_hit_server_plugins));

    /* ... then override the desired methods */
    sasl_switch_hit_server_plugins->mech_new = sasl_switch_hit_server_mech_new;
    sasl_switch_hit_server_plugins->mech_step = sasl_switch_hit_server_mech_step;
    sasl_switch_hit_server_plugins->mech_dispose = sasl_switch_hit_server_mech_dispose;
    sasl_switch_hit_server_plugins->mech_free = sasl_switch_hit_server_mech_free;

    /* jabberd can't do auth-int or auth-conf */
    sasl_switch_hit_server_plugins->max_ssf = 0;

    *out_version = SASL_SERVER_PLUG_VERSION;
    *pluglist = sasl_switch_hit_server_plugins;
    *plugcount = 1;
    
    return SASL_OK;
}

int
sasl_switch_hit_register_apple_digest_md5(void)
{
    static int done = 0;
    int retval = SASL_OK;

    if (! done) {
        retval = sasl_server_add_plugin("apple-digest-md5", sasl_switch_hit_server_plug_init);
        assert(retval == SASL_OK);

        done = 1;
    }

    if (retval != SASL_OK)
        return -1;
    return 0;
}

