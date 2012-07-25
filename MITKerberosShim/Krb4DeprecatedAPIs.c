/*
 * Deprecated.c
 *
 * $Header$
 *
 * Copyright 2003-2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <TargetConditionals.h>
#include <sys/socket.h>

// ---------------------------------------------------------------------------

/* Header files for types: */
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include "mit-com_err.h"
#include "mit-profile.h"

// ---------------------------------------------------------------------------

/*
 * The following functions are part of the KfM ABI.  
 * They are deprecated, so they only appear here, not in krb.h.
 */
int KRB5_CALLCONV krb_get_num_cred(void);
int KRB5_CALLCONV krb_get_nth_cred(char *, char *, char *, int);
int KRB5_CALLCONV krb_delete_cred(char *, char *,char *);
int KRB5_CALLCONV dest_all_tkts(void);

// ---------------------------------------------------------------------------

typedef struct KClientSession *KClientSession;
typedef KClientSession KClientSessionInfo;

struct KClientPrincipalOpaque;
typedef struct KClientPrincipalOpaque* KClientPrincipal;

struct KClientAddress {
        UInt32                          address;
        UInt16                          port;
};
typedef struct KClientAddress KClientAddress;
typedef FSSpec KClientFile;
typedef void *cc_ccache_t;


struct KClientKey {
        unsigned char key[8];
};
typedef struct KClientKey KClientKey;

struct key_schedule {
	struct {
		UInt32 _[2];
	} keys[16];
};

/*
 * Internet address (a structure for historical reasons)
 */
struct in_addr {
	in_addr_t s_addr;
};


/*
 * Socket address, internet style.
 */
struct sockaddr_in {
	__uint8_t       sin_len;
	sa_family_t     sin_family;
	in_port_t       sin_port;
	struct  in_addr sin_addr;
	char            sin_zero[8];
};

#define		MAX_KTXT_LEN	1250

struct ktext {
    int     length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    unsigned long mbz;		/* zero to catch runaway strings */
};

// ---------------------------------------------------------------------------

static inline int _DeprecatedFunctionErrorReturn (const char *functionName);
static inline char *_DeprecatedFunctionStringReturn (const char *functionName);

#define DeprecatedFunctionErrorReturn  _DeprecatedFunctionErrorReturn(__FUNCTION__)
#define DeprecatedFunctionStringReturn _DeprecatedFunctionStringReturn(__FUNCTION__)

static inline int _DeprecatedFunctionErrorReturn (const char *functionName)
{
    return -1;
}

static inline char *_DeprecatedFunctionStringReturn (const char *functionName)
{
    static char string[BUFSIZ];
    
    _DeprecatedFunctionErrorReturn (functionName);
    
    snprintf (string, sizeof(string), "%s() is no longer supported.", functionName);
    return string;
}

// ---------------------------------------------------------------------------

#pragma mark -- KerberosPreferences --

OSErr KPInitializeWithDefaultKerberosLibraryPreferences (const FSSpec *prefLocation)
{
    return DeprecatedFunctionErrorReturn;
}

OSErr KPGetListOfPreferencesFiles (UInt32     userSystemFlags,
                                   FSSpecPtr *thePrefFiles,
                                   UInt32    *outNumberOfFiles)
{
    return DeprecatedFunctionErrorReturn;
}

void KPFreeListOfPreferencesFiles (FSSpecPtr thePrefFiles)
{
    DeprecatedFunctionErrorReturn;
}

OSErr KPPreferencesFileIsReadable (const FSSpec *inPrefsFile)
{
    return DeprecatedFunctionErrorReturn;
}

OSErr KPPreferencesFileIsWritable (const FSSpec *inPrefsFile)
{
    return DeprecatedFunctionErrorReturn;
}

OSErr KPCreatePreferencesFile (const FSSpec *inPrefsFile)
{
    return DeprecatedFunctionErrorReturn;
}

// ---------------------------------------------------------------------------

#pragma mark -- Profile --

int krb_change_password(char *principal, char *instance, char *realm, 
                        char *oldPassword, char *newPassword)
{
    return DeprecatedFunctionErrorReturn;
}

int decomp_ticket(struct ktext tkt, unsigned char *flags, char *pname, char *pinstance, 
                  char *prealm, UInt32 *paddress, unsigned char session[8], 
                  int *life, UInt32 *time_sec, char *sname, char *sinstance, 
                  unsigned char key[8], struct key_schedule key_s)
{
    return DeprecatedFunctionErrorReturn;
}

const char * const _krb_err_txt[] = {
    "krb_err_txt is no longer supported",
    NULL
};
const char * const * const krb_err_txt = _krb_err_txt;

const char * krb_get_err_text(int code)
{
    return DeprecatedFunctionStringReturn;
}

int get_ad_tkt (char *service, char *sinstance, char *realm, int lifetime)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_in_tkt (char *user, char *instance, char *realm,
                    char *service, char *sinstance, int life, 
                    void *key_proc, void *decrypt_proc, 
                    char *arg)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_in_tkt_creds(char *user, char *instance, char *realm,
                         char *service, char *sinstance, int life,
                         void *key_proc, void *decrypt_proc, 
                         char *arg, void *creds)
{
    return DeprecatedFunctionErrorReturn;
}

char * krb_get_phost(char *alias)
{
    return DeprecatedFunctionStringReturn;
}

int krb_get_pw_in_tkt(char *user, char *instance, char *realm, 
                      char *service, char *sinstance, 
                      int life, char *password)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_pw_in_tkt_creds(char *user, char *instance, char *realm, 
                            char *service, char *sinstance,
                            int life, char *password, void *creds)
{
    return DeprecatedFunctionErrorReturn;
}

int get_pw_tkt(char *user, char *instance, char *realm, char *cpw)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_svc_in_tkt(char *user, char *instance, char *realm, 
                       char *service, char *sinstance, int life, char *srvtab)
{
    return DeprecatedFunctionErrorReturn;
}

int KRB5_CALLCONV
krb_get_ticket_for_service (char *serviceName, char *buf, UInt32 *buflen,
                            int checksum, unsigned char sessionKey[8], struct key_schedule schedule,
                            char *version, int includeVersion)
{
    return DeprecatedFunctionErrorReturn;
}

int k_isrealm(char *s)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int k_isinst(char *s)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int k_isname(char *s)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int kname_parse(char *np, char *ip, char *rp, char *fullname)
{
    return DeprecatedFunctionErrorReturn;
}

int kname_unparse(char *fullname, const char *np, const char *ip, const char *rp)
{
    return DeprecatedFunctionErrorReturn;
}

int kuserok(void *kdata, char *luser)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_check_auth (struct ktext buf,  UInt32 checksum, void *msg_data, 
                    unsigned char session[8], struct key_schedule schedule, 
                    struct sockaddr_in *laddr, struct sockaddr_in *faddr)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_mk_auth(long options, struct ktext ticket, 
                char *service, char *inst, char *realm, 
                UInt32 checksum, char *version, struct ktext buf)
{
    return DeprecatedFunctionErrorReturn;
}

long krb_mk_err(u_char *p, UInt32 e, char *e_string)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

long krb_mk_priv(u_char *in, u_char *out,  UInt32 length, 
                 struct key_schedule schedule,void *key, 
                 struct sockaddr_in *sender, struct sockaddr_in *receiver)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_mk_req(struct ktext authent, 
               char *service, char *instance, char *realm, 
               UInt32 checksum)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_mk_req_creds(struct ktext authent, void *creds, 
                     UInt32 checksum)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_set_lifetime(int newval)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

long krb_mk_safe(u_char *in, u_char *out, UInt32 length, 
                 void *key, struct sockaddr_in *sender, 
                 struct sockaddr_in *receiver)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int put_svc_key(char *sfile, char *name, char *inst, char *realm, 
                int newvno, char *key)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_rd_err(u_char *in, u_long in_length, long *code, void *m_data)
{
    return DeprecatedFunctionErrorReturn;
}

long krb_rd_priv(u_char *in, UInt32 in_length, 
				 struct key_schedule schedule, void *key, 
                 struct sockaddr_in *sender, struct sockaddr_in *receiver, 
                 void *m_data)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_rd_req(struct ktext authent, 
               char *service, char *instance, 
               UInt32 from_addr, void *ad, char *fn)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_rd_req_int(struct ktext authent, char *service, char *instance, 
                   UInt32 from_addr, void *ad, unsigned char key[8])
{
    return DeprecatedFunctionErrorReturn;
}

long krb_rd_safe(u_char *in, UInt32 in_length, void *key, 
                 struct sockaddr_in *sender, struct sockaddr_in *receiver, 
                 void *m_data)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int read_service_key(char *service, char *instance, char *realm, 
                     int kvno, char *file, char *key)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_recvauth(long options, int fd, struct ktext ticket, 
                 char *service, char *instance, 
                 struct sockaddr_in *faddr, struct sockaddr_in *laddr, 
                 void *kdata, char *filename, struct key_schedule schedule, 
                 char *version)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_sendauth(long options, int fd, struct ktext ticket, 
                 char *service, char *inst, char *realm, 
                 UInt32 checksum, void *msg_data, 
                 void *cred, struct key_schedule schedule, 
                 struct sockaddr_in *laddr, struct sockaddr_in *faddr, 
                 char *version)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_tf_realm (const char *ticket_file, char *realm)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_tf_fullname (const char *ticket_file, 
                         char *name, char *instance, char *realm)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_cred(char *service, char *instance, char *realm, void *c)
{
    return DeprecatedFunctionErrorReturn;
}

const char *tkt_string (void)
{
    return DeprecatedFunctionStringReturn;
}

void krb_set_tkt_string (const char *val)
{
    DeprecatedFunctionErrorReturn;
}

int dest_tkt (void)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_num_cred (void)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_get_nth_cred (char *sname, char *sinstance, char *srealm, int n)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_delete_cred (char *sname, char *sinstance, char *srealm)
{
    return DeprecatedFunctionErrorReturn;
}

int dest_all_tkts (void)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_profile (profile_t* profile)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_lrealm (char *realm, int n)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_admhst (char *host, char *realm, int n)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_krbhst(char *host, const char *realm, int n)
{
    return DeprecatedFunctionErrorReturn;
}

char *krb_realmofhost(char *host)
{
    return DeprecatedFunctionStringReturn;
}

int krb_time_to_life(UInt32 start, UInt32 end)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

UInt32 krb_life_to_time(UInt32 start, int life)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

char *krb__get_srvtabname(const char *default_srvtabname)
{
    return DeprecatedFunctionStringReturn;
}

// ---------------------------------------------------------------------------

#pragma mark -- Kerberos4 --

int FSp_krb_get_svc_in_tkt (char *user, char *instance, char *realm, char *service, 
                            char *sinstance, int life, const FSSpec *srvtab)
{
    return DeprecatedFunctionErrorReturn;
}

int FSp_put_svc_key (const FSSpec *sfilespec, char *name, char *inst, char *realm, 
                     int newvno, char *key)
{
    return DeprecatedFunctionErrorReturn;
}

int FSp_read_service_key (char *service, char *instance, char *realm, 
                          int kvno, const FSSpec *filespec, char *key) 
{
    return DeprecatedFunctionErrorReturn;
}

// ---------------------------------------------------------------------------

#pragma mark -- KClient Deprecated --

OSStatus KClientCacheInitialTicketDeprecated (KClientSession *inSession, char *inService)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetLocalRealmDeprecated (char *outRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetLocalRealmDeprecated (const char *inRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetRealmDeprecated (const char *inHost, char *outRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientAddRealmMapDeprecated (char *inHost, char *inRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDeleteRealmMapDeprecated (char *inHost)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNthRealmMapDeprecated (SInt32 inIndex, char *outHost, char *outRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNthServerDeprecated (SInt32 inIndex, char *outHost, char *inRealm, Boolean inAdmin)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientAddServerMapDeprecated (char *inHost, char *inRealm, Boolean inAdmin)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDeleteServerMapDeprecated (char *inHost, char *inRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNthServerMapDeprecated (SInt32 inIndex, char *outHost, char *outRealm, Boolean *outAdmin)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNthServerPortDeprecated (SInt32 inIndex, UInt16 *outPort)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetNthServerPortDeprecated (SInt32 inIndex, UInt16 inPort)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNumSessionsDeprecated (SInt32 *outSessions)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNthSessionDeprecated (SInt32 inIndex, char *outName, char *outInstance, char *outRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDeleteSessionDeprecated (char *inName, char *inInstance, char *inRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetCredentialsDeprecated (char *inName, char *inInstance, char *inRealm,
                                          void *outCred)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientAddCredentialsDeprecated (char *inName, char *inInstance, char *inRealm, void *inCred)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDeleteCredentialsDeprecated (char *inName, char *inInstance, char *inRealm, 
                                             char *inSname, char *inSinstance, char *inSrealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNumCredentialsDeprecated (SInt32 *outNumCredentials,
                                             char *inName, char *inInstance, char *inRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetNthCredentialDeprecated (SInt32 inIndex,
                                            char *inName, char *inInstance, char *inRealm,
                                            char *inSname, char *inSinstance, char *inSrealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetUserNameDeprecated (char *outUserName)
{
    return DeprecatedFunctionErrorReturn;
}

void KClientGetErrorTextDeprecated (OSErr inError, char *outBuffer)
{
    char *p = DeprecatedFunctionStringReturn;
    memcpy(outBuffer, p, strlen(p) + 1); /* avoid strcpy warning, api is broken */
}

OSStatus K5ClientGetTicketForServiceDeprecated (char *inService, void *outBuffer, UInt32 *outBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus K5ClientGetAuthenticatorForServiceDeprecated (char *inService, char *inApplicationVersion,
                                                       void *outBuffer, UInt32 *outBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

// ---------------------------------------------------------------------------

#pragma mark -- KClient 1.9 Compat --

OSErr KClientVersionCompat (SInt16 *outMajorVersion, SInt16 *outMinorVersion, char *outVersionString)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientNewSessionCompat (KClientSessionInfo *inSession,
                               UInt32 inLocalAddress, UInt16 inLocalPort,
                               UInt32 inRemoteAddress, UInt16 inRemotePort)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientDisposeSessionCompat (KClientSessionInfo *inSession)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientGetTicketForServiceCompat (KClientSessionInfo *inSession, char *inService,
                                        void *inBuffer, UInt32 *outBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientGetTicketForServiceWithChecksumCompat (KClientSessionInfo *inSession, UInt32 inChecksum,
                                                    char *inService, void *inBuffer, UInt32 *outBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientLoginCompat (KClientSessionInfo *inSession, KClientKey *outPrivateKey)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientPasswordLoginCompat (KClientSessionInfo *inSession, char *inPassword, KClientKey *outPrivateKey)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientLogoutCompat (void)
{
    return DeprecatedFunctionErrorReturn;
}


SInt16 KClientStatusCompat (void)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientGetSessionKeyCompat (KClientSessionInfo *inSession, KClientKey *outSessionKey)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientEncryptCompat (KClientSessionInfo *inSession,
                            void *inPlainBuffer, UInt32 inPlainBufferLength,
                            void *outEncryptedBuffer, UInt32 *ioEncryptedBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientDecryptCompat (KClientSessionInfo *inSession,
                            void *inEncryptedBuffer, UInt32 inEncryptedBufferLength,
                            UInt32 *outPlainBufferOffset, UInt32 *outPlainBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientProtectIntegrityCompat (KClientSessionInfo *inSession,
                                     void *inPlainBuffer, UInt32 inPlainBufferLength,
                                     void *outProtectedBuffer, UInt32 *ioProtectedBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientVerifyIntegrityCompat (KClientSessionInfo *inSession,
                                    void *inProtectedBuffer, UInt32 inProtectedBufferLength,
                                    UInt32 *outPlainBufferOffset, UInt32 *outPlainBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KServerNewSessionCompat (KClientSessionInfo *inSession, char *inService,
                               UInt32 inLocalAddress, UInt16 inLocalPort,
                               UInt32 inRemoteAddress, UInt16 inRemotePort)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KServerVerifyTicketCompat (KClientSessionInfo *inSession, void *inBuffer, char *inFilename)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KServerGetReplyTicketCompat (KClientSessionInfo *inSession, void *outBuffer, UInt32 *ioBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KServerAddKeyCompat (KClientSessionInfo *inSession, KClientKey *inPrivateKey,
                           char *inService, SInt32 inVersion, char *inFilename)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KServerGetKeyCompat (KClientSessionInfo *inSession, KClientKey *outPrivateKey,
                           char *inService, SInt32 inVersion, char *inFilename)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KServerGetSessionTimeRemainingCompat (KClientSessionInfo *inSession, SInt32 *outSeconds)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientGetSessionUserNameCompat (KClientSessionInfo *inSession, char *outUserName, SInt16 inNameType)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientMakeSendAuthCompat (KClientSessionInfo *inSession, char *inService,
                                 void *outBuffer, UInt32 *ioBufferLength,
                                 SInt32 inChecksum, char *inApplicationVersion)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientVerifyReplyTicketCompat (KClientSessionInfo *inSession,
                                      void *inBuffer, UInt32 *ioBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}


OSErr KClientVerifyUnencryptedReplyTicketCompat (KClientSessionInfo *inSession,
                                                 void *inBuffer, UInt32 *ioBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

// ---------------------------------------------------------------------------

#pragma mark -- KClient 3.0 --

OSStatus KClientGetVersion (UInt16 *outMajorVersion, UInt16 *outMinorVersion, const char **outVersionString)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientNewClientSession (KClientSession *outSession)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientNewServerSession (KClientSession *inSession, KClientPrincipal inService)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDisposeSession (KClientSession inSession)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetClientPrincipal (KClientSession inSession, KClientPrincipal *outPrincipal)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetClientPrincipal (KClientSession inSession, KClientPrincipal inPrincipal)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetServerPrincipal (KClientSession inSession, KClientPrincipal *outPrincipal)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetServerPrincipal (KClientSession inSession, KClientPrincipal inPrincipal)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetLocalAddress (KClientSession inSession, KClientAddress *outLocalAddress)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetLocalAddress (KClientSession inSession, const KClientAddress *inLocalAddress)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetRemoteAddress (KClientSession inSession, KClientAddress *outRemoteAddress)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetRemoteAddress (KClientSession inSession, const KClientAddress *inRemoteAddress)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetSessionKey (KClientSession inSession, KClientKey *outKey)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetExpirationTime (KClientSession inSession, UInt32 *outExpiration)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientSetKeyFile (KClientSession inSession, const KClientFile *inKeyFile)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientLogin (KClientSession inSession)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientPasswordLogin (KClientSession inSession, const char *inPassword)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientKeyFileLogin (KClientSession inSession)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientLogout (KClientSession inSession)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetServiceKey (KClientSession inSession, UInt32 inVersion, KClientKey *outKey)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientAddServiceKey (KClientSession inSession, UInt32 inVersion, const KClientKey *inKey)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetTicketForService (KClientSession inSession, UInt32 inChecksum,
                                     void*outBuffer, UInt32*ioBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetAuthenticatorForService (KClientSession inSession, UInt32 inChecksum, 
                                            const char *inApplicationVersion,
                                            void *outBuffer, UInt32 *ioBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientVerifyEncryptedServiceReply (KClientSession inSession, const void *inBuffer, UInt32 inBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientVerifyProtectedServiceReply (KClientSession inSession, const void *inBuffer, UInt32 inBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientVerifyAuthenticator (KClientSession inSession, const void *inBuffer, UInt32 inBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetEncryptedServiceReply (KClientSession inSession, void *outBuffer, UInt32 *ioBufferSize)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetProtectedServiceReply (KClientSession inSession, void * outBuffer, UInt32 *ioBufferSize)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientEncrypt (KClientSession inSession,
                         const void *inPlainBuffer, UInt32 inPlainBufferLength,
                         void *outEncryptedBuffer, UInt32 *ioEncryptedBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDecrypt (KClientSession inSession,
                         void *inEncryptedBuffer, UInt32 inDecryptedBufferLength,
                         UInt32 *outPlainOffset, UInt32 *outPlainLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientProtectIntegrity (KClientSession inSession,
                                  const void *inPlainBuffer, UInt32 inPlainBufferLength,
                                  void *outProtectedBuffer, UInt32 *ioProtectedBufferLength)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientVerifyIntegrity (KClientSession inSession,
                                 void *inProtectedBuffer, UInt32 inProtectedBufferLength,
                                 UInt32 *outPlainOffset, UInt32 *outPlainLength)
{
    return DeprecatedFunctionErrorReturn;
}

/* Miscellaneous */

OSStatus KClientPasswordToKey (KClientSession inSession, const char *inPassword, KClientKey *outKey)
{
    return DeprecatedFunctionErrorReturn;
}

/* Getting to other APIs */

OSStatus KClientGetCCacheReference (KClientSession inSession, cc_ccache_t *outCCacheReference)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientGetProfileHandle (KClientSession inSession, profile_t *outProfileHandle)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientV4StringToPrincipal (const char *inPrincipalString, KClientPrincipal *outPrincipal)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientPrincipalToV4String (KClientPrincipal inPrincipal, char *outPrincipalString)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientPrincipalToV4Triplet (KClientPrincipal inPrincipal,
                                      char *outName, char *outInstance, char *outRealm)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientDisposePrincipal (KClientPrincipal inPrincipal)
{
    return DeprecatedFunctionErrorReturn;
}
