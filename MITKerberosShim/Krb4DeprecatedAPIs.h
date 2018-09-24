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
#include "include/mit-profile.h"

// ---------------------------------------------------------------------------

/*
 * The following functions are part of the KfM ABI.
 * They are deprecated, so they only appear here, not in krb.h.
 */
int krb_get_num_cred(void);
int krb_get_nth_cred(char *, char *, char *, int);
int krb_delete_cred(char *, char *,char *);
int dest_all_tkts(void);

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

#define        MAX_KTXT_LEN    1250

struct ktext {
    int     length;        /* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];    /* The data itself */
    unsigned long mbz;        /* zero to catch runaway strings */
};


#pragma mark -- KerberosPreferences --

OSErr KPInitializeWithDefaultKerberosLibraryPreferences (const FSSpec *prefLocation);

OSErr KPGetListOfPreferencesFiles (UInt32     userSystemFlags,
                                   FSSpecPtr *thePrefFiles,
                                   UInt32    *outNumberOfFiles);

void KPFreeListOfPreferencesFiles (FSSpecPtr thePrefFiles);

OSErr KPPreferencesFileIsReadable (const FSSpec *inPrefsFile);

OSErr KPPreferencesFileIsWritable (const FSSpec *inPrefsFile);

OSErr KPCreatePreferencesFile (const FSSpec *inPrefsFile);

// ---------------------------------------------------------------------------

#pragma mark -- Profile --

int krb_change_password(char *principal, char *instance, char *realm,
                        char *oldPassword, char *newPassword);
int decomp_ticket(struct ktext tkt, unsigned char *flags, char *pname, char *pinstance,
                  char *prealm, UInt32 *paddress, unsigned char session[8],
                  int *life, UInt32 *time_sec, char *sname, char *sinstance,
                  unsigned char key[8], struct key_schedule key_s);

const char * krb_get_err_text(int code);

int get_ad_tkt (char *service, char *sinstance, char *realm, int lifetime);

int krb_get_in_tkt (char *user, char *instance, char *realm,
                    char *service, char *sinstance, int life,
                    void *key_proc, void *decrypt_proc,
                    char *arg);

int krb_get_in_tkt_creds(char *user, char *instance, char *realm,
                         char *service, char *sinstance, int life,
                         void *key_proc, void *decrypt_proc,
                         char *arg, void *creds);

char * krb_get_phost(char *alias);

int krb_get_pw_in_tkt(char *user, char *instance, char *realm,
                      char *service, char *sinstance,
                      int life, char *password);

int krb_get_pw_in_tkt_creds(char *user, char *instance, char *realm,
                            char *service, char *sinstance,
                            int life, char *password, void *creds);

int get_pw_tkt(char *user, char *instance, char *realm, char *cpw);

int krb_get_svc_in_tkt(char *user, char *instance, char *realm,
                       char *service, char *sinstance, int life, char *srvtab);

int
krb_get_ticket_for_service (char *serviceName, char *buf, UInt32 *buflen,
                            int checksum, unsigned char sessionKey[8], struct key_schedule schedule,
                            char *version, int includeVersion);

int k_isrealm(char *s);

int k_isinst(char *s);

int k_isname(char *s);

int kname_parse(char *np, char *ip, char *rp, char *fullname);

int kname_unparse(char *fullname, const char *np, const char *ip, const char *rp);

int kuserok(void *kdata, char *luser);

int krb_check_auth (struct ktext buf,  UInt32 checksum, void *msg_data,
                    unsigned char session[8], struct key_schedule schedule,
                    struct sockaddr_in *laddr, struct sockaddr_in *faddr);

int krb_mk_auth(long options, struct ktext ticket,
                char *service, char *inst, char *realm,
                UInt32 checksum, char *version, struct ktext buf);

long krb_mk_err(u_char *p, UInt32 e, char *e_string);

long krb_mk_priv(u_char *in, u_char *out,  UInt32 length,
                 struct key_schedule schedule,void *key,
                 struct sockaddr_in *sender, struct sockaddr_in *receiver);

int krb_mk_req(struct ktext authent,
               char *service, char *instance, char *realm,
               UInt32 checksum);

int krb_mk_req_creds(struct ktext authent, void *creds,
                     UInt32 checksum);
int krb_set_lifetime(int newval);

long krb_mk_safe(u_char *in, u_char *out, UInt32 length,
                 void *key, struct sockaddr_in *sender,
                 struct sockaddr_in *receiver);

int put_svc_key(char *sfile, char *name, char *inst, char *realm,
                int newvno, char *key);

int krb_rd_err(u_char *in, u_long in_length, long *code, void *m_data);

long krb_rd_priv(u_char *in, UInt32 in_length,
                 struct key_schedule schedule, void *key,
                 struct sockaddr_in *sender, struct sockaddr_in *receiver,
                 void *m_data);

int krb_rd_req(struct ktext authent,
               char *service, char *instance,
               UInt32 from_addr, void *ad, char *fn);

int krb_rd_req_int(struct ktext authent, char *service, char *instance,
                   UInt32 from_addr, void *ad, unsigned char key[8]);

long krb_rd_safe(u_char *in, UInt32 in_length, void *key,
                 struct sockaddr_in *sender, struct sockaddr_in *receiver,
                 void *m_data);

int read_service_key(char *service, char *instance, char *realm,
                     int kvno, char *file, char *key);

int krb_recvauth(long options, int fd, struct ktext ticket,
                 char *service, char *instance,
                 struct sockaddr_in *faddr, struct sockaddr_in *laddr,
                 void *kdata, char *filename, struct key_schedule schedule,
                 char *version);

int krb_sendauth(long options, int fd, struct ktext ticket,
                 char *service, char *inst, char *realm,
                 UInt32 checksum, void *msg_data,
                 void *cred, struct key_schedule schedule,
                 struct sockaddr_in *laddr, struct sockaddr_in *faddr,
                 char *version);

int krb_get_tf_realm (const char *ticket_file, char *realm);

int krb_get_tf_fullname (const char *ticket_file,
                         char *name, char *instance, char *realm);

int krb_get_cred(char *service, char *instance, char *realm, void *c);

const char *tkt_string (void);

void krb_set_tkt_string (const char *val);

int dest_tkt (void);

int krb_get_num_cred (void);

int krb_get_nth_cred (char *sname, char *sinstance, char *srealm, int n);

int krb_delete_cred (char *sname, char *sinstance, char *srealm);


int dest_all_tkts (void);

int krb_get_profile (profile_t* profile);

int krb_get_lrealm (char *realm, int n);

int krb_get_admhst (char *host, char *realm, int n);

int krb_get_krbhst(char *host, const char *realm, int n);

char *krb_realmofhost(char *host);

int krb_time_to_life(UInt32 start, UInt32 end);

UInt32 krb_life_to_time(UInt32 start, int life);

char *krb__get_srvtabname(const char *default_srvtabname);

// ---------------------------------------------------------------------------

#pragma mark -- Kerberos4 --

int FSp_krb_get_svc_in_tkt (char *user, char *instance, char *realm, char *service,
                            char *sinstance, int life, const FSSpec *srvtab);

int FSp_put_svc_key (const FSSpec *sfilespec, char *name, char *inst, char *realm,
                     int newvno, char *key);

int FSp_read_service_key (char *service, char *instance, char *realm,
                          int kvno, const FSSpec *filespec, char *key);

// ---------------------------------------------------------------------------

#pragma mark -- KClient Deprecated --

OSStatus KClientCacheInitialTicketDeprecated (KClientSession *inSession, char *inService);

OSStatus KClientGetLocalRealmDeprecated (char *outRealm);

OSStatus KClientSetLocalRealmDeprecated (const char *inRealm);

OSStatus KClientGetRealmDeprecated (const char *inHost, char *outRealm);

OSStatus KClientAddRealmMapDeprecated (char *inHost, char *inRealm);

OSStatus KClientDeleteRealmMapDeprecated (char *inHost);

OSStatus KClientGetNthRealmMapDeprecated (SInt32 inIndex, char *outHost, char *outRealm);

OSStatus KClientGetNthServerDeprecated (SInt32 inIndex, char *outHost, char *inRealm, Boolean inAdmin);

OSStatus KClientAddServerMapDeprecated (char *inHost, char *inRealm, Boolean inAdmin);

OSStatus KClientDeleteServerMapDeprecated (char *inHost, char *inRealm);

OSStatus KClientGetNthServerMapDeprecated (SInt32 inIndex, char *outHost, char *outRealm, Boolean *outAdmin);

OSStatus KClientGetNthServerPortDeprecated (SInt32 inIndex, UInt16 *outPort);

OSStatus KClientSetNthServerPortDeprecated (SInt32 inIndex, UInt16 inPort);

OSStatus KClientGetNumSessionsDeprecated (SInt32 *outSessions);

OSStatus KClientGetNthSessionDeprecated (SInt32 inIndex, char *outName, char *outInstance, char *outRealm);

OSStatus KClientDeleteSessionDeprecated (char *inName, char *inInstance, char *inRealm);

OSStatus KClientGetCredentialsDeprecated (char *inName, char *inInstance, char *inRealm,
                                          void *outCred);

OSStatus KClientAddCredentialsDeprecated (char *inName, char *inInstance, char *inRealm, void *inCred);

OSStatus KClientDeleteCredentialsDeprecated (char *inName, char *inInstance, char *inRealm,
                                             char *inSname, char *inSinstance, char *inSrealm);

OSStatus KClientGetNumCredentialsDeprecated (SInt32 *outNumCredentials,
                                             char *inName, char *inInstance, char *inRealm);

OSStatus KClientGetNthCredentialDeprecated (SInt32 inIndex,
                                            char *inName, char *inInstance, char *inRealm,
                                            char *inSname, char *inSinstance, char *inSrealm);

OSStatus KClientGetUserNameDeprecated (char *outUserName);

void KClientGetErrorTextDeprecated (OSErr inError, char *outBuffer);

OSStatus K5ClientGetTicketForServiceDeprecated (char *inService, void *outBuffer, UInt32 *outBufferLength);

OSStatus K5ClientGetAuthenticatorForServiceDeprecated (char *inService, char *inApplicationVersion,
                                                       void *outBuffer, UInt32 *outBufferLength);

// ---------------------------------------------------------------------------

#pragma mark -- KClient 1.9 Compat --

OSErr KClientVersionCompat (SInt16 *outMajorVersion, SInt16 *outMinorVersion, char *outVersionString);


OSErr KClientNewSessionCompat (KClientSessionInfo *inSession,
                               UInt32 inLocalAddress, UInt16 inLocalPort,
                               UInt32 inRemoteAddress, UInt16 inRemotePort);


OSErr KClientDisposeSessionCompat (KClientSessionInfo *inSession);


OSErr KClientGetTicketForServiceCompat (KClientSessionInfo *inSession, char *inService,
                                        void *inBuffer, UInt32 *outBufferLength);


OSErr KClientGetTicketForServiceWithChecksumCompat (KClientSessionInfo *inSession, UInt32 inChecksum,
                                                    char *inService, void *inBuffer, UInt32 *outBufferLength);


OSErr KClientLoginCompat (KClientSessionInfo *inSession, KClientKey *outPrivateKey);


OSErr KClientPasswordLoginCompat (KClientSessionInfo *inSession, char *inPassword, KClientKey *outPrivateKey);


OSErr KClientLogoutCompat (void);


SInt16 KClientStatusCompat (void);


OSErr KClientGetSessionKeyCompat (KClientSessionInfo *inSession, KClientKey *outSessionKey);


OSErr KClientEncryptCompat (KClientSessionInfo *inSession,
                            void *inPlainBuffer, UInt32 inPlainBufferLength,
                            void *outEncryptedBuffer, UInt32 *ioEncryptedBufferLength);


OSErr KClientDecryptCompat (KClientSessionInfo *inSession,
                            void *inEncryptedBuffer, UInt32 inEncryptedBufferLength,
                            UInt32 *outPlainBufferOffset, UInt32 *outPlainBufferLength);


OSErr KClientProtectIntegrityCompat (KClientSessionInfo *inSession,
                                     void *inPlainBuffer, UInt32 inPlainBufferLength,
                                     void *outProtectedBuffer, UInt32 *ioProtectedBufferLength);


OSErr KClientVerifyIntegrityCompat (KClientSessionInfo *inSession,
                                    void *inProtectedBuffer, UInt32 inProtectedBufferLength,
                                    UInt32 *outPlainBufferOffset, UInt32 *outPlainBufferLength);


OSErr KServerNewSessionCompat (KClientSessionInfo *inSession, char *inService,
                               UInt32 inLocalAddress, UInt16 inLocalPort,
                               UInt32 inRemoteAddress, UInt16 inRemotePort);


OSErr KServerVerifyTicketCompat (KClientSessionInfo *inSession, void *inBuffer, char *inFilename);


OSErr KServerGetReplyTicketCompat (KClientSessionInfo *inSession, void *outBuffer, UInt32 *ioBufferLength);


OSErr KServerAddKeyCompat (KClientSessionInfo *inSession, KClientKey *inPrivateKey,
                           char *inService, SInt32 inVersion, char *inFilename);


OSErr KServerGetKeyCompat (KClientSessionInfo *inSession, KClientKey *outPrivateKey,
                           char *inService, SInt32 inVersion, char *inFilename);


OSErr KServerGetSessionTimeRemainingCompat (KClientSessionInfo *inSession, SInt32 *outSeconds);


OSErr KClientGetSessionUserNameCompat (KClientSessionInfo *inSession, char *outUserName, SInt16 inNameType);


OSErr KClientMakeSendAuthCompat (KClientSessionInfo *inSession, char *inService,
                                 void *outBuffer, UInt32 *ioBufferLength,
                                 SInt32 inChecksum, char *inApplicationVersion);


OSErr KClientVerifyReplyTicketCompat (KClientSessionInfo *inSession,
                                      void *inBuffer, UInt32 *ioBufferLength);


OSErr KClientVerifyUnencryptedReplyTicketCompat (KClientSessionInfo *inSession,
                                                 void *inBuffer, UInt32 *ioBufferLength);

// ---------------------------------------------------------------------------

#pragma mark -- KClient 3.0 --

OSStatus KClientGetVersion (UInt16 *outMajorVersion, UInt16 *outMinorVersion, const char **outVersionString);

OSStatus KClientNewClientSession (KClientSession *outSession);

OSStatus KClientNewServerSession (KClientSession *inSession, KClientPrincipal inService);

OSStatus KClientDisposeSession (KClientSession inSession);

OSStatus KClientGetClientPrincipal (KClientSession inSession, KClientPrincipal *outPrincipal);

OSStatus KClientSetClientPrincipal (KClientSession inSession, KClientPrincipal inPrincipal);

OSStatus KClientGetServerPrincipal (KClientSession inSession, KClientPrincipal *outPrincipal);

OSStatus KClientSetServerPrincipal (KClientSession inSession, KClientPrincipal inPrincipal);

OSStatus KClientGetLocalAddress (KClientSession inSession, KClientAddress *outLocalAddress);

OSStatus KClientSetLocalAddress (KClientSession inSession, const KClientAddress *inLocalAddress);

OSStatus KClientGetRemoteAddress (KClientSession inSession, KClientAddress *outRemoteAddress);

OSStatus KClientSetRemoteAddress (KClientSession inSession, const KClientAddress *inRemoteAddress);

OSStatus KClientGetSessionKey (KClientSession inSession, KClientKey *outKey);

OSStatus KClientGetExpirationTime (KClientSession inSession, UInt32 *outExpiration);

OSStatus KClientSetKeyFile (KClientSession inSession, const KClientFile *inKeyFile);

OSStatus KClientLogin (KClientSession inSession);

OSStatus KClientPasswordLogin (KClientSession inSession, const char *inPassword);

OSStatus KClientKeyFileLogin (KClientSession inSession);

OSStatus KClientLogout (KClientSession inSession);

OSStatus KClientGetServiceKey (KClientSession inSession, UInt32 inVersion, KClientKey *outKey);

OSStatus KClientAddServiceKey (KClientSession inSession, UInt32 inVersion, const KClientKey *inKey);

OSStatus KClientGetTicketForService (KClientSession inSession, UInt32 inChecksum,
                                     void*outBuffer, UInt32*ioBufferLength);

OSStatus KClientGetAuthenticatorForService (KClientSession inSession, UInt32 inChecksum,
                                            const char *inApplicationVersion,
                                            void *outBuffer, UInt32 *ioBufferLength);

OSStatus KClientVerifyEncryptedServiceReply (KClientSession inSession, const void *inBuffer, UInt32 inBufferLength);

OSStatus KClientVerifyProtectedServiceReply (KClientSession inSession, const void *inBuffer, UInt32 inBufferLength);

OSStatus KClientVerifyAuthenticator (KClientSession inSession, const void *inBuffer, UInt32 inBufferLength);

OSStatus KClientGetEncryptedServiceReply (KClientSession inSession, void *outBuffer, UInt32 *ioBufferSize);

OSStatus KClientGetProtectedServiceReply (KClientSession inSession, void * outBuffer, UInt32 *ioBufferSize);

OSStatus KClientEncrypt (KClientSession inSession,
                         const void *inPlainBuffer, UInt32 inPlainBufferLength,
                         void *outEncryptedBuffer, UInt32 *ioEncryptedBufferLength);

OSStatus KClientDecrypt (KClientSession inSession,
                         void *inEncryptedBuffer, UInt32 inDecryptedBufferLength,
                         UInt32 *outPlainOffset, UInt32 *outPlainLength);

OSStatus KClientProtectIntegrity (KClientSession inSession,
                                  const void *inPlainBuffer, UInt32 inPlainBufferLength,
                                  void *outProtectedBuffer, UInt32 *ioProtectedBufferLength);

OSStatus KClientVerifyIntegrity (KClientSession inSession,
                                 void *inProtectedBuffer, UInt32 inProtectedBufferLength,
                                 UInt32 *outPlainOffset, UInt32 *outPlainLength);

/* Miscellaneous */

/* Getting to other APIs */

OSStatus KClientGetCCacheReference (KClientSession inSession, cc_ccache_t *outCCacheReference);

OSStatus KClientV4StringToPrincipal (const char *inPrincipalString, KClientPrincipal *outPrincipal);

OSStatus KClientPrincipalToV4String (KClientPrincipal inPrincipal, char *outPrincipalString);

OSStatus KClientPrincipalToV4Triplet (KClientPrincipal inPrincipal,
                                      char *outName, char *outInstance, char *outRealm);

OSStatus KClientDisposePrincipal (KClientPrincipal inPrincipal);

