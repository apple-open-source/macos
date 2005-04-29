/*
 * RealmsEditor.m
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/RealmsConfiguration.m,v 1.13 2004/11/02 23:28:38 lxs Exp $
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

#import "RealmsConfiguration.h"
#import "UNIXReadWrite.h"
//#import "krbports.h" // Kerberos 4 port names
//#import "osconf.h"   // Kerberos 5 port names

#pragma mark -

@implementation KerberosServer

// Note: typeMenuIndex is the index of menu items in the type popup menu

typedef struct __ServerType {
    unsigned int typeMenuIndex;
    NSString *string;
} ServerType;

#define kdcType     0
#define adminType   1
#define krb524Type  2
#define kpasswdType 3

const ServerType kServerTypes[] = { 
    { kdcType,     @"kdc" }, 
    { adminType,   @"admin_server" }, 
    { krb524Type,  @"krb524_server" }, 
    { kpasswdType, @"kpasswd_server" }, 
    { 4,           NULL } };

// ---------------------------------------------------------------------------

+ (KerberosServer *) emptyServer
{
    return [[[KerberosServer alloc] init] autorelease];
}

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        hostString = NULL;
        customPort = 0;
        hasCustomPort = FALSE;
        typeMenuIndex = 0;
        version = kerberosVersion_V5;
    }
    return self;
}

// ---------------------------------------------------------------------------
    
- (id) initWithTypeString: (NSString *) serverType 
                  version: (KLKerberosVersion) serverVersion 
            profileString: (NSString *) profileString
{
    if ((self = [self init])) {
        const ServerType *typesPtr;

        for (typesPtr = kServerTypes; typesPtr->string != NULL; typesPtr++) {
            if ([serverType compare: typesPtr->string] == NSOrderedSame) {
                typeMenuIndex = typesPtr->typeMenuIndex;
            }
        }

        NSRange separator = [profileString rangeOfString: @":" options: (NSLiteralSearch | NSBackwardsSearch)];
        if (separator.location == NSNotFound) {
            hostString = [profileString retain];
        } else {
            hostString = [[profileString substringToIndex: separator.location] retain];
            customPort = [[profileString substringFromIndex: separator.location + separator.length] intValue];
            hasCustomPort = TRUE;
        }
        
        version = serverVersion;
    }
    return self;
}


// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (hostString != NULL) { [hostString release]; }
    [super dealloc];
}


// ---------------------------------------------------------------------------
- (NSString *) profileString
{
    NSMutableString *string = [NSMutableString stringWithCapacity: [[self host] length]];
    if (string != NULL) {
        [string appendFormat: @"%s:%d", [[self host] UTF8String], [[self port] intValue]];
    }
    
    return (string != NULL) ? string : [self host];
}

// ---------------------------------------------------------------------------

- (NSString *) typeString
{
    const ServerType *typesPtr;
    
    for (typesPtr = kServerTypes; typesPtr->string != NULL; typesPtr++) {
        if (typeMenuIndex == typesPtr->typeMenuIndex) {
            return typesPtr->string;
        }
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (unsigned int) typeMenuIndex
{
    return typeMenuIndex;
}

// ---------------------------------------------------------------------------

- (void) setTypeMenuIndex: (unsigned int) newTypeMenuIndex
{
    typeMenuIndex = newTypeMenuIndex;
}

// ---------------------------------------------------------------------------

- (KLKerberosVersion) version
{
    return version;
}

// ---------------------------------------------------------------------------

- (void) setVersion: (KLKerberosVersion) newVersion
{
    version = newVersion;
}

// ---------------------------------------------------------------------------

- (NSString *) host
{
    return (hostString != NULL) ? hostString : @"";
}

// ---------------------------------------------------------------------------

- (void) setHost: (NSString *) newHost
{
    if (hostString != NULL) { [hostString release]; }
    hostString = [newHost retain];
    //NSLog (@"setting host string to '%@'", hostString);
}

// ---------------------------------------------------------------------------

- (NSNumber *) port
{
    if (hasCustomPort) {
        return [NSNumber numberWithInt: customPort];
    } else {
        return [self defaultPort];
    }
}

// ---------------------------------------------------------------------------

- (void) setPort: (NSNumber *) newPort
{
    hasCustomPort = (newPort > 0);
    customPort = (hasCustomPort) ? [newPort intValue] : [[self defaultPort] intValue];
}

// ---------------------------------------------------------------------------

- (NSNumber *) defaultPort
{
    int port = 0;
    
    if (typeMenuIndex == kdcType) {
        port = (version == kerberosVersion_V4) ? 750 /* KERBEROS_PORT */ : 88 /* KRB5_DEFAULT_PORT */;
    } else if (typeMenuIndex == adminType) {
        port = (version == kerberosVersion_V4) ? 751 /* KADM_PORT */ : 749 /* DEFAULT_KADM5_PORT */;
    } else if (typeMenuIndex == krb524Type) {
        port = 4444; // KRB524_PORT
    } else if (typeMenuIndex == kpasswdType) {
        port = 464; // DEFAULT_KPASSWD_PORT
    }
     
    return [NSNumber numberWithInt: port];
}

@end

#pragma mark -

@implementation KerberosDomain

// ---------------------------------------------------------------------------

+ (KerberosDomain *) emptyDomain
{
    return [[[KerberosDomain alloc] init] autorelease];
}

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        nameString = NULL;
    }
    return self;
}

// ---------------------------------------------------------------------------

- (id) initWithName: (NSString *) name
{
    if ((self = [self init])) {
        nameString = [name retain];
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (nameString != NULL) { [nameString release]; }
    [super dealloc];
}


// ---------------------------------------------------------------------------

- (NSString *) name
{
    return ((nameString != NULL) ? nameString : @"");
}

// ---------------------------------------------------------------------------

- (void) setName: (NSString *) newName
{
    if (nameString != NULL) { [nameString release]; }
    nameString = [newName retain];
}

@end

#pragma mark -

@implementation KerberosRealm

// ---------------------------------------------------------------------------

+ (KerberosRealm *) emptyRealm
{
    return [[[KerberosRealm alloc] init] autorelease];
}

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        krb5_error_code err = 0;
        nameInProfileString = NULL;
        nameString = NULL;
        v4NameString = NULL;
        defaultDomainString = NULL;
        serversArray = NULL;
        displayInDialogPopup = YES;
        version = 0;
        
        if (!err) {
            serversArray = [[NSMutableArray alloc] init];
            if (serversArray == NULL) { err = ENOMEM; }
        }
        
        if (!err) {
            domainsArray = [[NSMutableArray alloc] init];
            if (domainsArray == NULL) { err = ENOMEM; }
        }
        
        if (err) {
            [self release];
            return NULL;
        }        
    }
    return self;
}

// ---------------------------------------------------------------------------

- (id) initWithName: (NSString *) name profile: (profile_t) profile
{
    if ((self = [self init])) {
        krb5_error_code err = 0;

        nameInProfileString = [name retain];
        nameString = [name retain];
        
        if (!err) {
            const char  *v4NamesList[] = { "realms", [name UTF8String], "v4_realm", NULL };
            char       **v4Names = NULL;

            err = profile_get_values (profile, v4NamesList, &v4Names);
            if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }

            if (!err && (v4Names != NULL)) {
                v4NameString = [[NSString alloc] initWithUTF8String: v4Names[0]];
                if (v4NameString == NULL) { err = ENOMEM; }
            }

            if (v4Names != NULL) { profile_free_list (v4Names); }
        }
        
        
        if (!err) {
            const char  *defaultDomainsList[] = { "realms", [name UTF8String], "default_domain", NULL };
            char       **defaultDomains = NULL;

            err = profile_get_values (profile, defaultDomainsList, &defaultDomains);        
            if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
            
            if (!err && (defaultDomains != NULL) && (defaultDomains[0] != NULL)) {
                defaultDomainString = [[NSString alloc] initWithUTF8String: defaultDomains[0]];
                if (defaultDomainString == NULL) { err = ENOMEM; }
            }
            
            if (defaultDomains != NULL) { profile_free_list (defaultDomains); }
        }
        
        if (!err) {
            err = [self addServersForVersion: kerberosVersion_V5 profile: profile];
        }
        
        if (!err) {
            err = [self addServersForVersion: kerberosVersion_V4 profile: profile];
        }

        // Is the realm in the KLL realms list?
        if (!err) {
            KLIndex popupIndex = 0;
            [self setDisplayInDialogPopup: (KLFindKerberosRealmByName ([name UTF8String], &popupIndex) == klNoErr)];
        }
        
        // The reason we don't have the realms load the domains is that some domains don't have a
        // realm listed in the profile -- this is common if you have DNS SRV records
        // instead the realms configuration will load them so it can create KerberosRealm objects
        // for these extra domain_realm mappings
        
        if (err) {
            [self release];
            return NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (nameString   != NULL) { [nameString release]; }
    if (v4NameString != NULL) { [v4NameString release]; }
    if (serversArray != NULL) { [serversArray release]; }
    if (domainsArray != NULL) { [domainsArray release]; }
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (krb5_error_code) flushToProfile: (profile_t) profile
{
    krb5_error_code err = 0;
    
    if ([[self name] length] <= 0)    { err = EINVAL; }
    
    // Note: we do not need to deal with the case of creating empty realm sections.
    // Any nonexistent realm sections will be created when we try to write into them.
    
    // Check the v5 realm name to see if we need to rename it:

    if (!err) {
        const char *realmList[] = { "realms", [[self nameInProfile] UTF8String], NULL };
        err = profile_rename_section (profile, realmList, [[self name] UTF8String]);
        if (err == PROF_NO_SECTION) { err = 0; }  // OK if there isn't one yet
    }
    
    // Change the v4 realm name if necesssary
    
    if (!err) {
        const char  *v4NamesList[] = { "realms", [[self name] UTF8String], "v4_realm", NULL };
        char       **v4Names = NULL;
        const char  *v4NameInProfile = NULL;
        
        if (profile_get_values (profile, v4NamesList, &v4Names) == 0) {
            v4NameInProfile = v4Names[0];  // profile has a separate v4 realm
            
            if ([self hasV4Name]) {
                err = profile_update_relation (profile, v4NamesList, v4NameInProfile, [[self v4Name] UTF8String]);
            } else {
                err = profile_clear_relation (profile, v4NamesList);
            }
        } else {
            v4NameInProfile = [[self nameInProfile] UTF8String];  // Is just the old v5 name
            
            if ([self hasV4Name]) {
                err = profile_add_relation (profile, v4NamesList, [[self v4Name] UTF8String]);  
            }
        }
        
        if (!err) {
            const char *v4RealmList[] = { REALMS_V4_PROF_REALMS_SECTION, v4NameInProfile, NULL };
            err = profile_rename_section (profile, v4RealmList, [[self v4Name] UTF8String]);
            if (err == PROF_NO_SECTION) { err = 0; }  // OK if there isn't one yet
        }

        if (v4Names != NULL) { profile_free_list (v4Names); }
    }
    
    // Write out the default domain:
    
    if (!err) {
        const char  *defaultDomainsList[] = { "realms", [[self name] UTF8String], "default_domain", NULL };
        char       **defaultDomains = NULL;
        
        if (profile_get_values (profile, defaultDomainsList, &defaultDomains) == 0) {
            if ([self hasDefaultDomain]) {
                err = profile_update_relation (profile, defaultDomainsList, defaultDomains[0], [[self defaultDomain] UTF8String]);
            } else {
                err = profile_clear_relation (profile, defaultDomainsList);
            }
        } else {
            if ([self hasDefaultDomain]) { 
                err = profile_add_relation (profile, defaultDomainsList, [[self defaultDomain] UTF8String]);
            }
        }

        if (defaultDomains != NULL) { profile_free_list (defaultDomains); }
    }

    // Write out the servers:
    
    if (!err) {
        err = [self flushServersForVersion: kerberosVersion_V5 toProfile: profile];
    }
    
    if (!err) {
        err = [self flushServersForVersion: kerberosVersion_V4 toProfile: profile];
    }    
    
    // Write out the domains:
    
    if (!err) {
        err = [self flushDomainsForVersion: kerberosVersion_V5 toProfile: profile];
    }
    
    if (!err) {
        err = [self flushDomainsForVersion: kerberosVersion_V4 toProfile: profile];
    }    
    
    // Is the realm in the KLL realms list?
    if (!err) {
        KLIndex popupIndex = 0;
        BOOL inPopup = (KLFindKerberosRealmByName ([[self name] UTF8String], &popupIndex) == klNoErr);
        
        if (inPopup && ![self displayInDialogPopup]) {
            err = KLRemoveKerberosRealm (popupIndex); // remove it
        } else if (!inPopup && [self displayInDialogPopup]) {
            err = KLInsertKerberosRealm (realmList_End, [[self name] UTF8String]);  // add it
        }
    }
    
    if (err) {
        NSLog (@"[KerberosRealm flush] for realm %@ returning err %d (%s)", 
               [self name], err, error_message (err));
    }
    
    return err;
}

// ---------------------------------------------------------------------------

- (krb5_error_code) addServersForVersion: (KLKerberosVersion) serverVersion profile: (profile_t) profile
{
    krb5_error_code err = 0;
    
    const char       *section = (serverVersion == kerberosVersion_V5) ? "realms" : REALMS_V4_PROF_REALMS_SECTION;
    NSString         *realmString = (serverVersion == kerberosVersion_V5) ? [self name] : [self v4Name];
    const char       *serversList[] = { section, [realmString UTF8String], NULL, NULL };
    const ServerType *typesPtr = NULL;
    
    for (typesPtr = kServerTypes; typesPtr->string != NULL && !err; typesPtr++) {
        NSString  *typeString = typesPtr->string;
        char     **servers = NULL;
        serversList[2] = [typeString UTF8String];
        
        err = profile_get_values (profile, serversList, &servers);        
        
        if (!err && (servers != NULL)) {
            char **s;
            for (s = servers; *s != NULL && !err; s++) {
                NSString *profileString = [NSString stringWithUTF8String: *s];
                KerberosServer *server = [[KerberosServer alloc] initWithTypeString: typeString
                                                                            version: serverVersion 
                                                                      profileString: profileString];
                if (server == NULL) {
                    err = ENOMEM; 
                } else {
                    //NSLog (@"Adding server '%@' of type '%@' for realm '%@'", 
                    //       realmString, typeString, profileString); 
                    [serversArray addObject: server];
                    [server release];
                }
            }            
        }
        
        // These errors are ok... there just aren't servers of this type
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (servers != NULL) { profile_free_list (servers); }
    }
    
    return err;
}

// ---------------------------------------------------------------------------

- (krb5_error_code) flushServersForVersion: (KLKerberosVersion) serverVersion toProfile: (profile_t) profile
{
    krb5_error_code err = 0;
    
    const char  *section = (serverVersion == kerberosVersion_V5) ? "realms" : REALMS_V4_PROF_REALMS_SECTION;
    NSString    *realmString = (serverVersion == kerberosVersion_V5) ? [self name] : [self v4Name];
    const char  *serversList[] = { section, [realmString UTF8String], NULL, NULL };
    
    // Clear out old servers:
    
    if (!err) {
        const ServerType *typesPtr = NULL;
        
        for (typesPtr = kServerTypes; typesPtr->string != NULL && !err; typesPtr++) {
            serversList[2] = [typesPtr->string UTF8String];
            
            err = profile_clear_relation (profile, serversList);        
            if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        }
    }
    
    // Store the new servers:
    
    if (!err) {
        unsigned int i = 0;
        
        for (i = 0; i < [serversArray count] && !err; i++) {
            KerberosServer *server = [serversArray objectAtIndex: i];
            if ((server != NULL) && ([server version] == serverVersion)) {
                serversList[2] = [[server typeString] UTF8String];
                err = profile_add_relation (profile, serversList, [[server profileString] UTF8String]);
            }
        }
        
    }    
    
    return err;
}

// ---------------------------------------------------------------------------

- (krb5_error_code) flushDomainsForVersion: (KLKerberosVersion) serverVersion toProfile: (profile_t) profile
{
    krb5_error_code err = 0;
    
    const char *section = (serverVersion == kerberosVersion_V5) ? "domain_realm" : REALMS_V4_PROF_DOMAIN_SECTION;
    NSString   *realmString = (serverVersion == kerberosVersion_V5) ? [self name] : [self v4Name];
    const char *domainMappingList[] = { section, NULL, NULL };
    
    unsigned int i = 0;
    
    if (profile == NULL) { err = ENOMEM; }
    
    for (i = 0; i < [self numberOfDomains] && !err; i++) {
        KerberosDomain *domain = [self domainAtIndex: i];
        if (domain != NULL) {
            char **domainMapping = NULL;
            
            domainMappingList[1] = [[domain name] UTF8String];
            
            if (!err) {
                err = profile_get_values (profile, domainMappingList, &domainMapping);
                if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
            }
            
            if (!err) {
                if (domainMapping != NULL) {
                    if ([realmString compare: [NSString stringWithUTF8String: domainMapping[0]]] != NSOrderedSame) {
                        // Eek!  this domain is already mapped to another realm!
                        NSLog (@"WARNING!  Domain '%s' is mapped to realms '%s' and '%s'", 
                               domainMappingList[1], [realmString UTF8String], domainMapping[0]);
                    }
                } else {
                    err = profile_add_relation (profile, domainMappingList, [realmString UTF8String]);
                }
            }
        }
    }        
    
    if (err) {
        NSLog (@"[KerberosRealm flush] for realm %@ returning err %d (%s)", 
               [self name], err, error_message (err));
    }
    return err;
}

// ---------------------------------------------------------------------------

- (NSString *) nameInProfile
{
    return (nameInProfileString != NULL) ? nameInProfileString : @"";
}

// ---------------------------------------------------------------------------

- (NSString *) name
{
    return (nameString != NULL) ? nameString : @"";
}

// ---------------------------------------------------------------------------

- (void) setName: (NSString *) newName
{
    if (nameString != NULL) { [nameString release]; }
    nameString = ((newName != NULL) && ([newName length] > 0)) ? [newName retain] : NULL;
}

// ---------------------------------------------------------------------------

- (BOOL) hasV4Name
{
    return ((v4NameString != NULL) && ([v4NameString length] > 0));
}

// ---------------------------------------------------------------------------

- (NSString *) v4Name
{
    if (v4NameString != NULL) {
        return v4NameString;
    } else {
        return [self name];
    }
}

// ---------------------------------------------------------------------------

- (void) setV4Name: (NSString *) newV4Name
{
    if (v4NameString != NULL) { [v4NameString release]; }
    v4NameString = ((newV4Name != NULL) && ([newV4Name length] > 0)) ? [newV4Name retain] : NULL;
}

// ---------------------------------------------------------------------------

- (BOOL) hasDefaultDomain
{
    return ((defaultDomainString != NULL) && ([defaultDomainString length] > 0));
}

// ---------------------------------------------------------------------------

- (NSString *) defaultDomain
{
    return (defaultDomainString != NULL) ? defaultDomainString : @"";
}

// ---------------------------------------------------------------------------

- (void) setDefaultDomain: (NSString *) defaultDomain
{
    if (defaultDomainString != NULL) { [defaultDomainString release]; }
    defaultDomainString = ((defaultDomain != NULL) && ([defaultDomain length] > 0)) ? [defaultDomain retain] : NULL;
}

// ---------------------------------------------------------------------------

- (BOOL) displayInDialogPopup
{
    return displayInDialogPopup;
}

// ---------------------------------------------------------------------------

- (void) setDisplayInDialogPopup: (BOOL) newDisplayInDialogPopup
{
    displayInDialogPopup = newDisplayInDialogPopup;
}

// ---------------------------------------------------------------------------

- (unsigned int) numberOfServers
{
    return [serversArray count];
}

// ---------------------------------------------------------------------------

- (KerberosServer *) serverAtIndex: (unsigned int) serverIndex
{
    if (serverIndex < [serversArray count]) {
        return [serversArray objectAtIndex: serverIndex];
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (unsigned int) indexOfServer: (KerberosServer *) server
{
    return [serversArray indexOfObject: server];
}

// ---------------------------------------------------------------------------

- (void) addServer: (KerberosServer *) server
{
    [serversArray addObject: server];
}

// ---------------------------------------------------------------------------

- (void) removeServer: (KerberosServer *) server
{
    [serversArray removeObject: server];
}

// ---------------------------------------------------------------------------

- (unsigned int) numberOfDomains
{
    return [domainsArray count];
}

// ---------------------------------------------------------------------------

- (KerberosDomain *) domainAtIndex: (unsigned int) domainIndex
{
    if (domainIndex < [domainsArray count]) {
        return [domainsArray objectAtIndex: domainIndex];
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (unsigned int) indexOfDomain: (KerberosDomain *) domain
{
    return [domainsArray indexOfObject: domain];
}


// ---------------------------------------------------------------------------

- (BOOL) mappedToByDomainString: (NSString *) domain
{
    // This differs from indexOfDomain in that it does a string comparison!
    unsigned int i = 0;
    
    for (i = 0; i < [domainsArray count]; i++) {
        KerberosDomain *d = [domainsArray objectAtIndex: i];
        if ([[d name] compare: domain] == NSOrderedSame) {
            return YES;
        }
    }
    
    return NO;    
}

// ---------------------------------------------------------------------------

- (void) addDomain: (KerberosDomain *) domain
{
    // Make sure this isn't a duplicate (use string comparison)
    if (![self mappedToByDomainString: [domain name]]) {
        [domainsArray addObject: domain];
    }
}

// ---------------------------------------------------------------------------

- (void) removeDomain: (KerberosDomain *) domain
{
    [domainsArray removeObject: domain];
}


@end

#pragma mark -

@implementation RealmsConfiguration

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super init])) {
        krb5_error_code err = 0;
        
        useDNS = YES;
        configurationPathString = NULL;
        profile = NULL;
        realmsArray = NULL;
        defaultRealmString = NULL;

        if (!err) {
            realmsArray = [[NSMutableArray alloc] init];
            if (realmsArray == NULL) { err = ENOMEM; }
        }
        
        if (!err) {
            err = [self load];
        }
        
        if (err) {
            [self release];
            return NULL;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (profile                 != NULL) { profile_abandon (profile); }
    if (configurationPathString != NULL) { [configurationPathString release]; }
    if (defaultRealmString      != NULL) { [defaultRealmString release]; }
    if (realmsArray             != NULL) { [realmsArray release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (krb5_error_code) load
{
    krb5_error_code err = 0;
    
    // If the profile is NULL, initialize it
    if (profile == NULL) {
        NSString *configurationPathStrings[] = { 
            @"~/Library/Preferences/edu.mit.Kerberos", 
            @"/Library/Preferences/edu.mit.Kerberos", 
            @"/etc/krb5.conf", NULL };
        NSString **paths = NULL; 
        NSString *newConfigurationPath = NULL;
        
        // Figure out which config file we should look at
        for (paths = configurationPathStrings; *paths != NULL; paths++) {
            NSString *path = [*paths stringByExpandingTildeInPath];
            if ([[NSFileManager defaultManager] isReadableFileAtPath: path]) {
                newConfigurationPath = path;
                break;
            }
        }
        if (newConfigurationPath == NULL) { err = EACCES; }
            
        if (!err) {
            if (configurationPathString != NULL) { [configurationPathString release]; }
            configurationPathString = [newConfigurationPath retain];            
        }
        
        // initialize the profile
        if (!err) {
            const_profile_filespec_t configFiles[] = { [configurationPathString UTF8String], NULL };
            profile_t newProfile = NULL;        
            
            err = profile_init (configFiles, &newProfile);
            
            if (!err) {
                profile = newProfile;        
            }
        }
    }
    
    // Clear out any existing realm state
    if (!err) {
        [realmsArray removeAllObjects];
    }
    
    // Get the default realm
    if (!err) {
        const char  *defaultRealmList[3] = {"libdefaults", "default_realm", NULL};
        char       **defaultRealm = NULL;
        
        if (!err) {
            err = profile_get_values (profile, defaultRealmList, &defaultRealm);
            if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        }
        
        if (!err && (defaultRealm != NULL)) {
            defaultRealmString = [[NSString alloc] initWithUTF8String: defaultRealm[0]];
            if (defaultRealmString == NULL) { err = ENOMEM; }
        }
        
        if (defaultRealm != NULL) { profile_free_list (defaultRealm); }
    }

    // Do we DNS for realm configuration
    if (!err) {
        const char  *dnsFallbackList[3] = {"libdefaults", "dns_fallback", NULL};
        char       **dnsFallback = NULL;
        
        if (!err) {
            err = profile_get_values (profile, dnsFallbackList, &dnsFallback);
            if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        }
        
        if (!err && (dnsFallback != NULL)) {
            // In krb5, everything else is considered false
            [self setUseDNS: (strcasecmp (dnsFallback[0], "y") == 0 || strcasecmp (dnsFallback[0], "yes")  == 0 ||
                              strcasecmp (dnsFallback[0], "t") == 0 || strcasecmp (dnsFallback[0], "true") == 0 ||
                              strcasecmp (dnsFallback[0], "1") == 0 || strcasecmp (dnsFallback[0], "on")   == 0)];
        }
        
        if (dnsFallback != NULL) { profile_free_list (dnsFallback); }
    }
    
    // Do the v5 realms
    if (!err) {
        const char  *v5RealmsList[] = { "realms", NULL };
        char       **v5Realms = NULL;
        
        err = profile_get_subsection_names (profile, v5RealmsList, &v5Realms);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (!err && (v5Realms != NULL)) {
            char **r;
            for (r = v5Realms; *r != NULL && !err; r++) {
                NSString *name = [NSString stringWithUTF8String: *r];
                KerberosRealm *realm = [[KerberosRealm alloc] initWithName: name profile: profile];
                if (realm == NULL) { 
                    err = ENOMEM; 
                } else {
                    [realmsArray addObject: realm];
                    [realm release];
                }
            }
        }
        
        if (v5Realms  != NULL) { profile_free_list (v5Realms); }
    }
    
    // Do the v4 realms
    
    if (!err) {
        const char  *v4RealmsList[] = { REALMS_V4_PROF_REALMS_SECTION, NULL };
        char       **v4Realms = NULL;
        
        err = profile_get_subsection_names (profile, v4RealmsList, &v4Realms);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (!err && (v4Realms != NULL)) {
            char **r;
            for (r = v4Realms; *r != NULL && !err; r++) {
                NSString *name = [NSString stringWithUTF8String: *r];
                KerberosRealm *existingRealm = [self findRealmByV4Name: name];
                if (existingRealm == NULL) {
                    KerberosRealm *realm = [[KerberosRealm alloc] initWithName: name profile: profile];
                    if (realm == NULL) { 
                        err = ENOMEM; 
                    } else {
                        [realmsArray addObject: realm];
                        [realm release];
                    }
                }
            }
        }
        
        if (v4Realms  != NULL) { profile_free_list (v4Realms); }
    }
    
    // The reason we don't have the realms load the domains is that some domains don't have a
    // realm listed in the profile -- this is common if you have DNS SRV records
    
    // Now do the v5 domain-realm mappings (which depend on the realms)
    
    if (!err) {
        const char  *v5DomainsList[] = { "domain_realm", NULL };
        char       **v5Domains = NULL;
        
        err = profile_get_relation_names (profile, v5DomainsList, &v5Domains);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (!err && (v5Domains != NULL)) {
            char **d;
            for (d = v5Domains; *d != NULL && !err; d++) {
                NSString *domainString = [NSString stringWithUTF8String: *d];
                if (domainString != NULL) {
                    const char  *domainMappingList[] = { "domain_realm", *d, NULL };
                    char       **domainMapping = NULL;
                    
                    if (!err) {
                        err = profile_get_values (profile, domainMappingList, &domainMapping);
                        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
                    }
                    
                    if (!err) {
                        NSString *realmName = [NSString stringWithUTF8String: domainMapping[0]];
                        KerberosRealm *realm = [self findRealmByName: realmName];
                        if (realm == NULL) {
                            // Create it
                            realm = [KerberosRealm emptyRealm];
                            if (realm != NULL) { [realm setName: realmName]; }
                        }
                        
                        if (realm != NULL) {
                            KerberosDomain *domain = [[KerberosDomain alloc] initWithName: domainString];
                            if (domain == NULL) { 
                                err = ENOMEM; 
                            } else {
                                [realm addDomain: domain];
                                [domain release];
                            }
                        }
                    }
                    
                    if (domainMapping != NULL) { profile_free_list (domainMapping); }
                }
            }
        }
        
        if (v5Domains != NULL) { profile_free_list (v5Domains); }
    }
    
    // Now do the v4 domain-realm mappings (which depend on the realms)
    
    if (!err) {
        const char  *v4DomainsList[] = { REALMS_V4_PROF_DOMAIN_SECTION, NULL };
        char       **v4Domains = NULL;
        
        err = profile_get_relation_names (profile, v4DomainsList, &v4Domains);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (!err && (v4Domains != NULL)) {
            char **d;
            for (d = v4Domains; *d != NULL && !err; d++) {
                NSString *domainString = [NSString stringWithUTF8String: *d];
                if (domainString != NULL) {
                    const char     *v4DomainMappingList[] = { REALMS_V4_PROF_DOMAIN_SECTION, *d, NULL };
                    char          **v4DomainMapping = NULL;
                    
                    if (!err) {
                        err = profile_get_values (profile, v4DomainMappingList, &v4DomainMapping);
                        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
                    }
                    
                    if (!err) {
                        NSString *realmName = [NSString stringWithUTF8String: v4DomainMapping[0]];
                        KerberosRealm *realm = [self findRealmByV4Name: realmName];
                        if (realm == NULL) {
                            // Create it
                            realm = [KerberosRealm emptyRealm];
                            if (realm != NULL) { [realm setName: realmName]; }
                        }
                        
                        if (realm != NULL) {
                            KerberosDomain *domain = [[KerberosDomain alloc] initWithName: domainString];
                            if (domain == NULL) { 
                                err = ENOMEM; 
                            } else {
                                [realm addDomain: domain];
                                [domain release];
                            }
                        }
                    }
                    
                    if (v4DomainMapping != NULL) { profile_free_list (v4DomainMapping); }
                }
            }
        }
        
        if (v4Domains != NULL) { profile_free_list (v4Domains); }
    }
    
    return err;
}

// ---------------------------------------------------------------------------

- (krb5_error_code) abandon
{
    if (profile != NULL) {
        profile_abandon (profile);
        profile = NULL;
    }
    
    return [self load];
}

// ---------------------------------------------------------------------------

- (krb5_error_code) flush
{
    krb5_error_code err = 0;
    int writable = 0;
    int modified = 1;
    
    // First flush the realms so all the names, servers and domains get updated:
    if (!err) {
        unsigned int i = 0;
        
        for (i = 0; i < [self numberOfRealms] && !err; i++) {
            KerberosRealm *realm = [self realmAtIndex: i];
            if (realm != NULL) {
                err = [realm flushToProfile: profile];
            }
        }        
    }
    
    // Walk over the profile looking for deleted v5 realms:
    
    if (!err) {
        const char     *v5RealmsList[] = { "realms", NULL };
        char          **v5Realms = NULL;

        err = profile_get_subsection_names (profile, v5RealmsList, &v5Realms);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }

        if (!err && (v5Realms != NULL)) {
            char **r;
            for (r = v5Realms; *r != NULL && !err; r++) {
                NSString *name = [NSString stringWithUTF8String: *r];
                KerberosRealm *existingRealm = [self findRealmByName: name];
                if (existingRealm == NULL) {
                    // Realm is not in our array! Delete it.
                    const char *deletedRealmList[] = { "realms", *r, NULL };
                    err = profile_rename_section (profile, deletedRealmList, NULL);
                }
            }
        }
        
        if (v5Realms  != NULL) { profile_free_list (v5Realms); }
    }
    
    // Walk over the profile looking for deleted v4 realms:

    if (!err) {
        const char  *v4RealmsList[] = { REALMS_V4_PROF_REALMS_SECTION, NULL };
        char       **v4Realms = NULL;
        
        err = profile_get_subsection_names (profile, v4RealmsList, &v4Realms);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (!err && (v4Realms != NULL)) {
            char **r;
            for (r = v4Realms; *r != NULL && !err; r++) {
                NSString *name = [NSString stringWithUTF8String: *r];
                KerberosRealm *existingRealm = [self findRealmByV4Name: name];
                if (existingRealm == NULL) {
                    // Realm is not in our array! Delete it.
                    const char *deletedV4RealmList[] = { REALMS_V4_PROF_REALMS_SECTION, *r, NULL };
                    err = profile_rename_section (profile, deletedV4RealmList, NULL);
                }
            }
        }
        
        if (v4Realms  != NULL) { profile_free_list (v4Realms); }
    }
    
    // Walk over the profile looking for deleted v5 domains:
    
    if (!err) {
        const char     *v5DomainsList[] = { "domain_realm", NULL };
        char          **v5Domains = NULL;

        err = profile_get_relation_names (profile, v5DomainsList, &v5Domains);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }

        if (!err && (v5Domains != NULL)) {    
            char **d;
            for (d = v5Domains; *d != NULL && !err; d++) {
                NSString *domain = [NSString stringWithUTF8String: *d];
                if (domain != NULL) {
                    const char  *domainMappingList[] = { "domain_realm", *d, NULL };
                    char       **domainMapping = NULL;
                    
                    if (!err) {
                        err = profile_get_values (profile, domainMappingList, &domainMapping);
                        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
                    }
                    
                    if (!err) {
                        KerberosRealm *realm = [self findRealmByName: [NSString stringWithUTF8String: domainMapping[0]]];
                        if ((realm == NULL) || (![realm mappedToByDomainString: domain])) {
                            // Domain is not in our array or no such realm! Delete it.
                            const char *deletedRealmList[] = { "domain_realm", *d, NULL };
                            err = profile_clear_relation (profile, deletedRealmList);
                        }
                    }
                    
                    if (domainMapping != NULL) { profile_free_list (domainMapping); }
                }
            }
        }
         
        if (v5Domains != NULL) { profile_free_list (v5Domains); }
    }
    
    // Walk over the profile looking for deleted v4 domains:
    
    if (!err) {
        const char     *v4DomainsList[] = { REALMS_V4_PROF_DOMAIN_SECTION, NULL };
        char          **v4Domains = NULL;

        err = profile_get_relation_names (profile, v4DomainsList, &v4Domains);
        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        
        if (!err && (v4Domains != NULL)) {
            char **d;
            for (d = v4Domains; *d != NULL && !err; d++) {
                NSString *domain = [NSString stringWithUTF8String: *d];
                if (domain != NULL) {
                    const char     *v4DomainMappingList[] = { REALMS_V4_PROF_DOMAIN_SECTION, *d, NULL };
                    char          **v4DomainMapping = NULL;
                    
                    if (!err) {
                        err = profile_get_values (profile, v4DomainMappingList, &v4DomainMapping);
                        if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
                    }
                    
                    if (!err && (v4DomainMapping != NULL)) {
                        KerberosRealm *realm = [self findRealmByV4Name: [NSString stringWithUTF8String: v4DomainMapping[0]]];
                        if ((realm == NULL) || (![realm mappedToByDomainString: domain])) {
                            // Domain is not in our array or no such realm! Delete it.
                            const char *deletedV4RealmList[] = { REALMS_V4_PROF_DOMAIN_SECTION, *d, NULL };
                            err = profile_clear_relation (profile, deletedV4RealmList);
                        }
                        
                        if (v4DomainMapping != NULL) { profile_free_list (v4DomainMapping); }
                    }
                }
            }
        }
        
        if (v4Domains != NULL) { profile_free_list (v4Domains); }
    }
    
    // Write out the default realm:
    
    if (!err) {
        const char  *defaultRealmList[3] = {"libdefaults", "default_realm", NULL};
        char       **defaultRealm = NULL;
        
        if (profile_get_values (profile, defaultRealmList, &defaultRealm) == 0) {
            if ([self hasDefaultRealm]) {
                err = profile_update_relation (profile, defaultRealmList, defaultRealm[0], [[self defaultRealm] UTF8String]);
            } else {
                err = profile_clear_relation (profile, defaultRealmList);
            }
        } else {
            if ([self hasDefaultRealm]) { 
                err = profile_add_relation (profile, defaultRealmList, [[self defaultRealm] UTF8String]);
            }
        }
        
        if (defaultRealm != NULL) { profile_free_list (defaultRealm); }
    }
    
    // use DNS for realm configuration
    if (!err) {
        const char  *dnsFallbackList[3] = {"libdefaults", "dns_fallback", NULL};
        char       **dnsFallback = NULL;
        char        *newValue = [self useDNS] ? "yes" : "no";
        
        if (!err) {
            err = profile_get_values (profile, dnsFallbackList, &dnsFallback);
            if ((err == PROF_NO_SECTION) || (err == PROF_NO_RELATION)) { err = 0; }
        }
        
        if (!err) {
            if (dnsFallback != NULL) {
                err = profile_update_relation (profile, dnsFallbackList, dnsFallback[0], newValue);
            } else {
                err = profile_add_relation (profile, dnsFallbackList, newValue);  
            }
        }
        
        if (dnsFallback != NULL) { profile_free_list (dnsFallback); }
    }

    // See if the profile even needs to be updated
    if (!err) {
        err = profile_is_modified (profile, &modified);
    }
    
    // And see if the user can just flush the profile as themselves:
    if (!err) {
        err = profile_is_writable (profile, &writable);
    }
    
    if (!err && modified) {
        if (writable) {
            err = profile_flush (profile);
        } else {
            char            *profileFileData = NULL;
            NSString        *resourcePath = NULL;
            NSString        *toolString = NULL;
            FILE            *toolPipe = NULL;
            AuthorizationRef authorizationRef = NULL; 
            
            err = profile_flush_to_buffer (profile, &profileFileData);
            
            // Locate the tool to copy the temporary file to the correct location
            if (!err) {
                resourcePath = [[NSBundle mainBundle] resourcePath];
                if (resourcePath == NULL) { err = EINVAL; }
            }
            
            if (!err) {
                toolString = [NSString stringWithFormat: @"%@/%@", resourcePath, @"SaveNewProfile"];
                if (toolString == NULL) { err = ENOMEM; }
            }
            
            // Make sure the user is authorized to use the tool
            if (!err) {
                err = AuthorizationCreate (NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, 
                                           &authorizationRef);
            }
            
            if (!err) {
                AuthorizationItem   items = { kAuthorizationRightExecute, 0, NULL, 0 }; 
                AuthorizationRights rights = { 1, &items };
                AuthorizationFlags  flags = (kAuthorizationFlagDefaults |  kAuthorizationFlagInteractionAllowed | 
                                             kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights); 
                
                err = AuthorizationCopyRights (authorizationRef, &rights, NULL, flags, NULL ); 
            }
            
            // Run the tool
            if (!err) {
                const char * arguments[] = { [configurationPathString UTF8String], NULL };
                
                err = AuthorizationExecuteWithPrivileges (authorizationRef, [toolString UTF8String], 
                                                          kAuthorizationFlagDefaults, (char **) arguments, &toolPipe);
            }
            
            if (!err) {
                int   toolDescriptor = fileno (toolPipe);
                pid_t toolPID = WAIT_ANY;  // look for any child by default
                int   toolStatus = 0;
                
                // Read the child's pid
                err = ReadBuffer (toolDescriptor, sizeof (toolPID), (char *) &toolPID);
                
                // Write the file to the tool
                if (!err) {
                    err = WriteDynamicLengthBuffer (toolDescriptor, profileFileData, strlen (profileFileData));
                }
                
                // Wait on the child so we don't drip zombies
                pid_t pid = waitpid (toolPID, &toolStatus, 0);
                if (pid == -1) { 
                    if (errno != ECHILD) { err = errno; } // okay if Security.framework already waited 
                } else {
                    if (WIFEXITED(toolStatus) && WEXITSTATUS (toolStatus)) { err = WEXITSTATUS (toolStatus); }
                }
            }
            
            // free these before abandoning the profile
            if (profileFileData  != NULL) { profile_free_buffer (profile, profileFileData); }  
            if (toolPipe         != NULL) { fclose (toolPipe); }
            if (authorizationRef != NULL) { AuthorizationFree (authorizationRef, kAuthorizationFlagDefaults); }

            // Toss the profile since it still thinks its dirty
            if (!err) {
                err = [self abandon];
            }
        }
    }
    
    if (!err) {
        err = [self load];
    }
        
    if (err) { NSLog (@"[RealmsConfiguration flush] returning err %d (%s)", err, error_message (err)); }
    
    return err;
}

// ---------------------------------------------------------------------------

- (BOOL) useDNS
{
    return useDNS;
}

// ---------------------------------------------------------------------------

- (void) setUseDNS: (BOOL) newUseDNS
{
    useDNS = newUseDNS;
}

// ---------------------------------------------------------------------------

- (BOOL) hasDefaultRealm
{
    return ((defaultRealmString != NULL) && ([defaultRealmString length] > 0));
}

// ---------------------------------------------------------------------------

- (NSString *) defaultRealm
{
    return (defaultRealmString != NULL) ? defaultRealmString : @"";
}

// ---------------------------------------------------------------------------

- (void) setDefaultRealm: (NSString *) defaultRealm
{
    if (defaultRealmString != NULL) { [defaultRealmString release]; }
    defaultRealmString = ((defaultRealm != NULL) && ([defaultRealm length] > 0)) ? [defaultRealm retain] : NULL;
}

// ---------------------------------------------------------------------------

- (void) addRealm: (KerberosRealm *) realm
{
    [realmsArray addObject: realm];
}

// ---------------------------------------------------------------------------

- (void) removeRealm: (KerberosRealm *) realm
{
    [realmsArray removeObject: realm];
}

// ---------------------------------------------------------------------------

- (KerberosRealm *) findRealmByName: (NSString *) realmName
{
    unsigned int i = 0;
    
    for (i = 0; i < [realmsArray count]; i++) {
        KerberosRealm *r = [realmsArray objectAtIndex: i];
        if ((r != NULL) && ([[r name] compare: realmName] == NSOrderedSame)) {
            return r;
        }
    }
    
    return NULL;    
}

// ---------------------------------------------------------------------------

- (KerberosRealm *) findRealmByV4Name: (NSString *) realmName
{
    unsigned int i = 0;
    
    for (i = 0; i < [realmsArray count]; i++) {
        KerberosRealm *r = [realmsArray objectAtIndex: i];
        if ((r != NULL) && ([[r v4Name] compare: realmName] == NSOrderedSame)) {
            return r;
        }
    }
    
    return NULL;    
}

// ---------------------------------------------------------------------------

- (unsigned int) numberOfRealms
{
    return [realmsArray count];
}

// ---------------------------------------------------------------------------

- (KerberosRealm *) realmAtIndex: (unsigned int) realmIndex
{
    if (realmIndex < [realmsArray count]) {
        return [realmsArray objectAtIndex: realmIndex];
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (unsigned int) indexOfRealm: (KerberosRealm *) realm
{
    return [realmsArray indexOfObject: realm];
}

// ---------------------------------------------------------------------------

- (BOOL) allowDomainString: (NSString *) domainString 
          mappedToNewRealm: (KerberosRealm *) realm 
              currentRealm: (KerberosRealm **) outCurrentRealm
{
    BOOL allow = YES;
    
    unsigned int i = 0;
    
    for (i = 0; (i < [realmsArray count]) && allow; i++) {
        KerberosRealm *r = [realmsArray objectAtIndex: i];
        if ((r != NULL) && ([[r name] compare: [realm name]] != NSOrderedSame)) {
            if ([r mappedToByDomainString: domainString]) {
                // Already mapped to a realm of a different name
                if (outCurrentRealm != NULL) {
                    *outCurrentRealm = r;
                }
                allow = NO;
            }
        }
    }
    
    return allow;
}

@end
