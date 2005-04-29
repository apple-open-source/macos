/*
 * RealmsConfiguration.h
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/RealmsConfiguration.h,v 1.9 2004/11/02 23:28:37 lxs Exp $
 *
 * Copyright 2004 Massachusetts Institute of Technology.
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

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

// ---------------------------------------------------------------------------

@interface KerberosDomain : NSObject
{
    NSString *nameString;
}

+ (KerberosDomain *) emptyDomain;

- (id) init;
- (id) initWithName: (NSString *) name;
- (void) dealloc;

- (NSString *) name;
- (void) setName: (NSString *) newName;

@end

// ---------------------------------------------------------------------------

@interface KerberosServer : NSObject
{
    KLKerberosVersion version;
    unsigned int typeMenuIndex;
    NSString *hostString;
    BOOL hasCustomPort;
    int customPort;
}

+ (KerberosServer *) emptyServer;

- (id) init;
- (id) initWithTypeString: (NSString *) serverType 
                  version: (KLKerberosVersion) serverVersion 
            profileString: (NSString *) profileString;
- (void) dealloc;

- (NSString *) profileString;

- (NSString *) typeString;
- (unsigned int) typeMenuIndex;
- (void) setTypeMenuIndex: (unsigned int) newTypeMenuIndex;

- (KLKerberosVersion) version;
- (void) setVersion: (KLKerberosVersion) newVersion;

- (NSString *) host;
- (void) setHost: (NSString *) newHost;

- (NSNumber *) port;
- (void) setPort: (NSNumber *) newPort;
- (NSNumber *) defaultPort;

@end

// ---------------------------------------------------------------------------

@interface KerberosRealm : NSObject
{
    NSString *nameInProfileString;
    NSString *nameString;
    NSString *v4NameString;
    NSString *defaultDomainString;
    KLKerberosVersion version;
    BOOL displayInDialogPopup;
    NSMutableArray *serversArray;
    NSMutableArray *domainsArray;
}

+ (KerberosRealm *) emptyRealm;

- (id) init;
- (id) initWithName: (NSString *) name profile: (profile_t) profile;
- (void) dealloc;

- (krb5_error_code) flushToProfile: (profile_t) profile;

- (krb5_error_code) addServersForVersion: (KLKerberosVersion) version profile: (profile_t) profile;
- (krb5_error_code) flushServersForVersion: (KLKerberosVersion) version toProfile: (profile_t) profile; 

- (krb5_error_code) flushDomainsForVersion: (KLKerberosVersion) serverVersion toProfile: (profile_t) profile;

- (NSString *) nameInProfile;

- (NSString *) name;
- (void) setName: (NSString *) newName;

- (BOOL) hasV4Name;
- (NSString *) v4Name;
- (void) setV4Name: (NSString *) newV4Name;

- (BOOL) hasDefaultDomain;
- (NSString *) defaultDomain;
- (void) setDefaultDomain:(NSString *) defaultDomain;

- (BOOL) displayInDialogPopup;
- (void) setDisplayInDialogPopup: (BOOL) newDisplayInDialogPopup;

- (unsigned int) numberOfServers;
- (KerberosServer *) serverAtIndex: (unsigned int) serverIndex;
- (unsigned int) indexOfServer: (KerberosServer *) server;

- (void) addServer: (KerberosServer *) server;
- (void) removeServer: (KerberosServer *) server;

- (unsigned int) numberOfDomains;
- (KerberosDomain *) domainAtIndex: (unsigned int) domainIndex;
- (unsigned int) indexOfDomain: (KerberosDomain *) domain;
- (BOOL) mappedToByDomainString: (NSString *) domain;

- (void) addDomain: (KerberosDomain *) domain;
- (void) removeDomain: (KerberosDomain *) domain;

@end

// ---------------------------------------------------------------------------

@interface RealmsConfiguration : NSObject
{
    NSString *configurationPathString;
    profile_t profile;
    BOOL useDNS;
    NSMutableArray *realmsArray;
    NSString *defaultRealmString;
}

- (id) init;
- (void) dealloc;

- (krb5_error_code) abandon;
- (krb5_error_code) load;
- (krb5_error_code) flush;

- (BOOL) useDNS;
- (void) setUseDNS: (BOOL) newUseDNS;

- (BOOL) hasDefaultRealm;
- (NSString *) defaultRealm;
- (void) setDefaultRealm: (NSString *) defaultRealm;

- (void) addRealm: (KerberosRealm *) realm;
- (void) removeRealm: (KerberosRealm *) realm;

- (KerberosRealm *) findRealmByName: (NSString *) realmName;
- (KerberosRealm *) findRealmByV4Name: (NSString *) realmName;

- (unsigned int) numberOfRealms;
- (KerberosRealm *) realmAtIndex: (unsigned int) realmIndex;
- (unsigned int) indexOfRealm: (KerberosRealm *) realm;

- (BOOL) allowDomainString: (NSString *) domainString 
          mappedToNewRealm: (KerberosRealm *) realm 
              currentRealm: (KerberosRealm **) outMappedRealm;

@end
