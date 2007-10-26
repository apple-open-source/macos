/*
 * Deprecated.c
 *
 * $Header$
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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
#if !TARGET_RT_64_BIT

// ---------------------------------------------------------------------------

/* Header files for types: */
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/KerberosDebug.h>
#include <Kerberos/com_err.h>
#include <Kerberos/profile.h>

#define KRB_PRIVATE 1
#include <Kerberos/krb.h>

// ---------------------------------------------------------------------------

/* Prototypes for deprecated functions: */
#include <Kerberos/KerberosPreferences.h>

long FSp_profile_init (const FSSpec* files, profile_t *ret_profile);
long FSp_profile_init_path (const FSSpec* files, profile_t *ret_profile);

int FSp_krb_get_svc_in_tkt (char *user, char *instance, char *realm, 
                            char *service, char *sinstance, int life, const FSSpec *srvtab);
int FSp_put_svc_key (const FSSpec *sfilespec, char *name, char *instance, char *realm, 
                     int newvno, char *key);
int FSp_read_service_key (char *service, char *instance, char *realm, 
                          int kvno, const FSSpec *filespec, char *key);

#include <Kerberos/KClientDeprecated.h>
#include <Kerberos/KClientCompat.h>
#include <Kerberos/KClient.h>

/*
 * The following functions are part of the KfM ABI.  
 * They are deprecated, so they only appear here, not in krb.h.
 */
int KRB5_CALLCONV krb_get_num_cred(void);
int KRB5_CALLCONV krb_get_nth_cred(char *, char *, char *, int);
int KRB5_CALLCONV krb_delete_cred(char *, char *,char *);
int KRB5_CALLCONV dest_all_tkts(void);

// ---------------------------------------------------------------------------


static inline int _DeprecatedFunctionErrorReturn (const char *functionName);
static inline char *_DeprecatedFunctionStringReturn (const char *functionName);

#define DeprecatedFunctionErrorReturn  _DeprecatedFunctionErrorReturn(__FUNCTION__)
#define DeprecatedFunctionStringReturn _DeprecatedFunctionStringReturn(__FUNCTION__)

static inline int _DeprecatedFunctionErrorReturn (const char *functionName)
{
    dprintf ("Warning: function %s() is no longer supported.  Returning immediately.", functionName);

#warning Need better error for deprecated functions.
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

long FSp_profile_init (const FSSpec *files, profile_t *ret_profile)
{
    return DeprecatedFunctionErrorReturn;
}

long FSp_profile_init_path (const FSSpec *files, profile_t *ret_profile)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_change_password(char *principal, char *instance, char *realm, 
                        char *oldPassword, char *newPassword)
{
    return DeprecatedFunctionErrorReturn;
}

int decomp_ticket(KTEXT tkt, unsigned char *flags, char *pname, char *pinstance, 
                  char *prealm, unsigned KRB4_32 *paddress, C_Block session, 
                  int *life, unsigned KRB4_32 *time_sec, char *sname, char *sinstance, 
                  C_Block key, Key_schedule key_s)
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
                    key_proc_type key_proc, decrypt_tkt_type decrypt_proc, 
                    char *arg)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_get_in_tkt_creds(char *user, char *instance, char *realm,
                         char *service, char *sinstance, int life,
                         key_proc_type key_proc, decrypt_tkt_type decrypt_proc, 
                         char *arg, CREDENTIALS *creds)
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
                            int life, char *password, CREDENTIALS *creds)
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
krb_get_ticket_for_service (char *serviceName, char *buf, unsigned KRB4_32 *buflen,
                            int checksum, des_cblock sessionKey, Key_schedule schedule,
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

int kuserok(AUTH_DAT *kdata, char *luser)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_check_auth (KTEXT buf, unsigned KRB4_32 checksum, MSG_DAT *msg_data, 
                    C_Block session, Key_schedule schedule, 
                    struct sockaddr_in *laddr, struct sockaddr_in *faddr)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_mk_auth(long options, KTEXT ticket, 
                char *service, char *inst, char *realm, 
                unsigned KRB4_32 checksum, char *version, KTEXT buf)
{
    return DeprecatedFunctionErrorReturn;
}

long krb_mk_err(u_char *p, KRB4_32 e, char *e_string)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

long krb_mk_priv(u_char *in, u_char *out, unsigned KRB4_32 length, 
                 Key_schedule schedule, C_Block *key, 
                 struct sockaddr_in *sender, struct sockaddr_in *receiver)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_mk_req(register KTEXT authent, 
               char *service, char *instance, char *realm, 
               KRB4_32 checksum)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_mk_req_creds(register KTEXT authent, CREDENTIALS *creds, 
                     KRB4_32 checksum)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_set_lifetime(int newval)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

long krb_mk_safe(u_char *in, u_char *out, unsigned KRB4_32 length, 
                 C_Block *key, struct sockaddr_in *sender, 
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

int krb_rd_err(u_char *in, u_long in_length, long *code, MSG_DAT *m_data)
{
    return DeprecatedFunctionErrorReturn;
}

long krb_rd_priv(u_char *in, unsigned KRB4_32 in_length, 
                 Key_schedule schedule, C_Block *key, 
                 struct sockaddr_in *sender, struct sockaddr_in *receiver, 
                 MSG_DAT *m_data)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int krb_rd_req(register KTEXT authent, 
               char *service, char *instance, 
               unsigned KRB4_32 from_addr, AUTH_DAT *ad, char *fn)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_rd_req_int(KTEXT authent, char *service, char *instance, 
                   KRB_UINT32 from_addr, AUTH_DAT *ad, C_Block key)
{
    return DeprecatedFunctionErrorReturn;
}

long krb_rd_safe(u_char *in, unsigned KRB4_32 in_length, C_Block *key, 
                 struct sockaddr_in *sender, struct sockaddr_in *receiver, 
                 MSG_DAT *m_data)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

int read_service_key(char *service, char *instance, char *realm, 
                     int kvno, char *file, char *key)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_recvauth(long options, int fd, KTEXT ticket, 
                 char *service, char *instance, 
                 struct sockaddr_in *faddr, struct sockaddr_in *laddr, 
                 AUTH_DAT *kdata, char *filename, Key_schedule schedule, 
                 char *version)
{
    return DeprecatedFunctionErrorReturn;
}

int krb_sendauth(long options, int fd, KTEXT ticket, 
                 char *service, char *inst, char *realm, 
                 unsigned KRB4_32 checksum, MSG_DAT *msg_data, 
                 CREDENTIALS *cred, Key_schedule schedule, 
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

int krb_get_cred(char *service, char *instance, char *realm, CREDENTIALS *c)
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

int krb_time_to_life(KRB4_32 start, KRB4_32 end)
{
    DeprecatedFunctionErrorReturn;
    return 0;
}

KRB4_32 krb_life_to_time(KRB4_32 start, int life)
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
                                          CREDENTIALS *outCred)
{
    return DeprecatedFunctionErrorReturn;
}

OSStatus KClientAddCredentialsDeprecated (char *inName, char *inInstance, char *inRealm, CREDENTIALS *inCred)
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
    strcpy (outBuffer, DeprecatedFunctionStringReturn);
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


#endif /* !TARGET_RT_64_BIT */
