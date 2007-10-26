/*
 * Copyright 1998-2003 Massachusetts Institute of Technology.
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

/*
 * KerberosLoginPrivate.h
 *
 * $Header$
 */

#ifndef __KERBEROSLOGINPRIVATE__
#define __KERBEROSLOGINPRIVATE__

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#    include <TargetConditionals.h>
#    if TARGET_RT_MAC_CFM
#        error "Use KfM 4.0 SDK headers for CFM compilation."
#    endif
#endif

#include <Kerberos/KerberosLogin.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Kerberos/krb5.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    klPromptMechanism_Autodetect = 0,
    klPromptMechanism_GUI = 1,
    klPromptMechanism_CLI = 2,
    klPromptMechanism_None = 0xFFFFFFFF
};
typedef u_int32_t KLPromptMechanism;

#define __KLInternalAcquireInitialTicketsForCache __KLAcquireInitialTicketsForCache
    
/*************/
/*** Types ***/
/*************/

typedef krb5_error_code (*KLPrompterProcPtr) (krb5_context  context,
                                              void         *data,
                                              const char   *name,
                                              const char   *banner,
                                              int           num_prompts,
                                              krb5_prompt   prompts[]);

/*****************/
/*** Functions ***/
/*****************/

extern KLStatus __KLChangePasswordWithPasswordsCompat (KLPrincipal  inPrincipal,
                                                       const char *inOldPassword,
                                                       const char *inNewPassword);
        
extern KLStatus __KLAcquireInitialTicketsForCache (const char          *inCacheName,
                                                   KLKerberosVersion    inKerberosVersion,
                                                   KLLoginOptions       inLoginOptions,
                                                   KLPrincipal         *outPrincipal,
                                                   char               **outCacheName);
		
extern KLStatus __KLSetApplicationPrompter (KLPrompterProcPtr inPrompter);

extern krb5_error_code __KLPrompter (krb5_context  context,
                                     void         *data,
                                     const char   *name,
                                     const char   *banner,
                                     int           num_prompts,
                                     krb5_prompt   prompts[]);

extern krb5_get_init_creds_opt *__KLLoginOptionsGetKerberos5Options (KLLoginOptions ioOptions);
extern KLTime                   __KLLoginOptionsGetStartTime (KLLoginOptions ioOptions);
extern char *                   __KLLoginOptionsGetServiceName (KLLoginOptions ioOptions);

extern KLStatus  __KLSetHomeDirectoryAccess (KLBoolean inAllowHomeDirectoryAccess);
extern KLBoolean __KLAllowHomeDirectoryAccess (void);

extern KLStatus  __KLSetAutomaticPrompting (KLBoolean inAllowAutomaticPrompting);
extern KLBoolean __KLAllowAutomaticPrompting (void);

extern KLStatus          __KLSetPromptMechanism (KLPromptMechanism inPromptMechanism);
extern KLPromptMechanism __KLPromptMechanism (void);

extern CFStringRef __KLGetCFStringForInfoDictionaryKey (const char *inKeyString);
extern CFStringEncoding __KLApplicationGetTextEncoding (void);

KLStatus __KLCreatePrincipalFromTriplet (const char  *inName,
                                         const char  *inInstance,
                                         const char  *inRealm,
                                         KLKerberosVersion  inKerberosVersion,
                                         KLPrincipal *outPrincipal);

KLStatus __KLGetTripletFromPrincipal (KLPrincipal         inPrincipal,
                                      KLKerberosVersion   inKerberosVersion,
                                      char              **outName,
                                      char              **outInstance,
                                      char              **outRealm);

KLStatus __KLCreatePrincipalFromKerberos5Principal (krb5_principal  inPrincipal,
                                                    KLPrincipal    *outPrincipal);

KLStatus __KLGetKerberos5PrincipalFromPrincipal (KLPrincipal     inPrincipal, 
                                                 krb5_context    inContext, 
                                                 krb5_principal *outKrb5Principal);

KLStatus __KLGetRealmFromPrincipal (KLPrincipal inPrincipal, char **outRealm);

KLBoolean __KLPrincipalIsTicketGrantingService (KLPrincipal inPrincipal);


#if TARGET_OS_MAC
#    if defined(__MWERKS__)
#        pragma import reset
#    endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KERBEROSLOGINPRIVATE__ */

