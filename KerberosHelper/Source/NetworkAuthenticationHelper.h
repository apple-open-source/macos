
#import <CoreFoundation/CoreFoundation.h>
#import <Security/Security.h>

typedef struct NAHData *NAHRef;
typedef struct NAHSelectionData *NAHSelectionRef;

/*
 * Flow:
 *
 * netauth = NAHCreate(CFSTR("hostname"), CFSTR("service"),
 *             { username = "foo", supportedMechs = mechs } );
 *
 * selections = NAHGetSelections(netauth);
 *
 * foreach s (selections) { 
 *
 *    acquiredict = [[NSMutableDictionary alloc] init];
 *    [acquiredict setValue:@"password" forKey:kNAHPassword];
 *
 *    if !(NAHSelectionAcquireCredential(s, acquiredict, NULL))  #blocking
 *          continue;
 *
 *    dict = NAHSelectionCopyAuthInfo(s)
 *
 *    res = NetFSOpenSesssion(dict);
 *    if (res == sucess) {
 *        CFRelese(dict);
 *        break;
 *    } else if (sucess == authentication_failed) {
 *        
 *    } else {
 *         #ignore all other failures
 *    }
 *    CFRelese(dict);
 * }
 *
 * CFRelese(netauth);
 * 
 */

extern const CFStringRef kNAHErrorDomain;

/* service keys */

extern const CFStringRef kNAHServiceAFPServer;
extern const CFStringRef kNAHServiceCIFSServer;
extern const CFStringRef kNAHServiceHostServer;
extern const CFStringRef kNAHServiceVNCServer;


extern const CFStringRef kNAHNegTokenInit; /* private - CFDictRef */
extern const CFStringRef kNAHUserName; /* CFStringRef */

/*
 * Default is to consider all certficates accessable in key chain.
 *
 * If this key is used, only these will only considered, pass in a
 * empty CFArrayRef if you want to disable using certificates.
 */

extern const CFStringRef kNAHCertificates; /* SecIdentityRef/CFArrayRef */
extern const CFStringRef kNAHPassword;

NAHRef
NAHCreate(CFAllocatorRef alloc,
	 CFStringRef hostname,
	 CFStringRef service,
	 CFDictionaryRef info);

/*
 * Return a ordered list of authentication
 */

CFArrayRef
NAHGetSelections(NAHRef);

extern const CFStringRef kNAHForceRefreshCredential;

Boolean
NAHSelectionAcquireCredential(NAHSelectionRef selection,
			     CFDictionaryRef info,
			     CFErrorRef *error);

Boolean
NAHSelectionAcquireCredentialAsync(NAHSelectionRef selection,
				  CFDictionaryRef info,
				  dispatch_queue_t q,
				  void (^result)(CFErrorRef error));

void
NAHCancel(NAHRef na);

/*
 * Status of selection
 */

extern const CFStringRef kNAHSelectionHaveCredential;
extern const CFStringRef kNAHSelectionUserPrintable;
extern const CFStringRef kNAHClientPrincipal;
extern const CFStringRef kNAHServerPrincipal;
extern const CFStringRef kNAHMechanism;
extern const CFStringRef kNAHInnerMechanism;
extern const CFStringRef kNAHCredentialType;
extern const CFStringRef kNAHUseSPNEGO;

extern const CFStringRef kNAHClientNameType;
extern const CFStringRef kNAHClientNameTypeGSSD;

extern const CFStringRef kNAHServerNameType;
extern const CFStringRef kNAHServerNameTypeGSSD;

extern const CFStringRef kNAHInferredLabel;

extern const CFStringRef kNAHNTUsername;
extern const CFStringRef kNAHNTServiceBasedName;
extern const CFStringRef kNAHNTKRB5PrincipalReferral;
extern const CFStringRef kNAHNTKRB5Principal;
extern const CFStringRef kNAHNTUUID;


CFTypeRef
NAHSelectionGetInfoForKey(NAHSelectionRef selection, CFStringRef key);

CFDictionaryRef
NAHSelectionCopyAuthInfo(NAHSelectionRef selection);

/*
 * Reference counting
 */

CFStringRef
NAHCopyReferenceKey(NAHSelectionRef selection);

Boolean
NAHAddReferenceAndLabel(NAHSelectionRef client, CFStringRef identifier);
		  
void
NAHFindByLabelAndRelease(CFStringRef identifier);

Boolean
NAHCredAddReference(CFStringRef referenceKey);

Boolean
NAHCredRemoveReference(CFStringRef referenceKey);

char *
NAHCreateRefLabelFromIdentifier(CFStringRef identifier);

extern CFStringRef kGSSAPIMechNTLM;
extern CFStringRef kGSSAPIMechKerberos;
extern CFStringRef kGSSAPIMechKerberosU2U;
extern CFStringRef kGSSAPIMechKerberosMicrosoft;
extern CFStringRef kGSSAPIMechPKU2U;
extern CFStringRef kGSSAPIMechIAKerb;
extern CFStringRef kGSSAPIMechSPNEGO;

CFStringRef
NAHCopyMMeUserNameFromCertificate(SecCertificateRef cert);
