//
//  AppDelegate.m
//  GSSTestAppOSX
//
//  Created by Love Hörnquist Åstrand on 2013-06-08.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>
#import <GSS/GSS.h>

#import "AppDelegate.h"
#import "CredentialTesterView.h"
#import "FakeXCTest.h"


@interface AppDelegate ()
@property (strong) dispatch_queue_t queue;
@property (assign) bool runMeOnce;
@property (assign) bool lastStatus;
@property (strong) NSArray *identities;

@end


static AppDelegate *me = NULL;

static int
callback(const char *fmt, va_list ap)
{
    if (me == NULL)
        return -1;

    char *output = NULL;

    vasprintf(&output, fmt, ap);

    dispatch_async(dispatch_get_main_queue(), ^{
        [me appendProgress:[NSString stringWithFormat:@"%s", output] color:NULL];
        free(output);
    });

    return 0;
}

@implementation AppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    self.queue = dispatch_queue_create("test-queue", NULL);
    
    self.runMeOnce = (getenv("RUN_ME_ONCE") != NULL);

    me = self;
    XFakeXCTestCallback = callback;

    if (self.runMeOnce) {
        double delayInSeconds = 2.0;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            [self runTests:self];
        });
    } else {
        [self addExistingCredentials];
        [self loadCertificates];
        [self credentialSelector:self];
    }
}


- (void)addCredentialTab:(gss_cred_id_t)cred {
    CredentialTesterView *ctv = [[CredentialTesterView alloc] initWithGSSCredential:cred];
    
    gss_name_t gssName = GSSCredentialCopyName(cred);
    CFStringRef name = GSSNameCreateDisplayString(gssName);
    
    NSTabViewItem *tabViewItem = [[NSTabViewItem alloc] init];
    [tabViewItem setLabel:(__bridge NSString *)name];
    [tabViewItem setView:ctv.view];
    [tabViewItem setIdentifier:ctv];
    
    ctv.tabViewItem = tabViewItem;
    
    [self.tabView addTabViewItem:tabViewItem];
    [self.tabView selectTabViewItem:tabViewItem];

}

- (void)addExistingCredentials {
    OM_uint32 min_stat;
    
    gss_iter_creds(&min_stat, 0, GSS_KRB5_MECHANISM, ^(gss_OID mech, gss_cred_id_t cred) {
        if (cred)
            [self addCredentialTab:cred];
    });
}

- (void)loadCertificates {

    // Build a list of Identities from Certificates which have a local private key.
    // Searching for kSecClassIdentity does not find SEP based private keys.
    NSDictionary *params = @{
                             (__bridge id)kSecClass : (__bridge id)kSecClassCertificate,
                             (__bridge id)kSecReturnRef : (__bridge id)kCFBooleanTrue,
                             (__bridge id)kSecMatchLimit : (__bridge id) kSecMatchLimitAll,
                             };
    
    CFTypeRef result = NULL;
    OSStatus status;
    SecIdentityRef identity = NULL;
    
    status  = SecItemCopyMatching((__bridge CFDictionaryRef)params, &result);
    if (status) {
        NSLog(@"SecItemCopyMatching returned: %d", (int)status);
        return;
    }
    
    SecCertificateRef certRef;
    NSMutableArray *array = [[NSMutableArray alloc] init];

    NSLog(@"Found %ld keyChain items.", (long)CFArrayGetCount((CFArrayRef)result));
    for (CFIndex i = 0; i < CFArrayGetCount(result); ++i) {
        certRef = (SecCertificateRef)CFArrayGetValueAtIndex(result, i);

        OSStatus status = SecIdentityCreateWithCertificate(NULL, certRef, &identity);
        if (status == noErr) {
            [ array addObject:CFBridgingRelease(identity)];
        }
    }

    NSMutableArray *items = [NSMutableArray array];
    
    NSLog(@"found: %ld entries:", [array count]);
    
    for (id identity in array) {
        SecIdentityRef ident = (__bridge SecIdentityRef)identity;
        
        NSLog(@"SecIdentityRef : %@", ident);
        
        SecCertificateRef cert = NULL;
        NSString *name = NULL;
        
        SecIdentityCopyCertificate(ident, &cert);
        if (cert) {
#if 0
            CFErrorRef error = NULL;
            NSString *appleIDName = CFBridgingRelease(_CSCopyAppleIDAccountForAppleIDCertificate(cert, &error));
            if (appleIDName == NULL) {
                NSLog(@"error: %@", error);
                if (error) CFRelease(error);
                name = CFBridgingRelease(SecCertificateCopySubjectSummary(cert));
            } else {
                name = [NSString stringWithFormat:@"AppleID: %@", appleIDName];
            }
#else
            name = CFBridgingRelease(SecCertificateCopySubjectSummary(cert));
#endif
            CFRelease(cert);
        }
        
        if (name == NULL)
            name = @"<can't find certificate/name, how is this an identity ?>";

        NSLog(@"SecIdentityRef: %@ subject: %@", identity, name);
        
        [items addObject:@{
                           @"name" : name,
                           @"value" : identity,
                           }];
    }
    
    self.identities = items;
    
    [self.certificateArrayController setContent:items];

}


- (IBAction)runTests:(id)sender
{
    [self.statusLabel setStringValue:@"running"];
    [self.runTestsButton setEnabled:NO];
    [self.progressTextView setString:@""];
    
    dispatch_async(self.queue, ^{

        [XCTest runTests];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.statusLabel setStringValue:@""];
            [self.runTestsButton setEnabled:YES];
	    if (self.runMeOnce)
		exit(self.lastStatus ? 0 : 1);
        });
    });
}

- (void)appendProgress:(NSString *)string color:(NSColor *)color {
    
    NSMutableAttributedString* str = [[NSMutableAttributedString alloc] initWithString:string];
    if (color)
        [str addAttribute:NSForegroundColorAttributeName value:color range:NSMakeRange(0, [str length])];
    
    NSTextStorage *textStorage = [self.progressTextView textStorage];
    
    [textStorage beginEditing];
    [textStorage appendAttributedString:str];
    [textStorage endEditing];
}

#pragma mark Manual tests

enum : NSInteger {
    passwordTag = 0,
    certificateTag = 1
};

- (IBAction)credentialSelector:(id)sender {
    NSInteger tag = [self.credentialRadioButton selectedTag];
    
    [self.password setEnabled:(tag == passwordTag)];
    [self.certificiatePopUp setEnabled:(tag == certificateTag)];
}

- (IBAction)certificateSelector:(id)sender {
    
    SecIdentityRef ident = (__bridge SecIdentityRef)([self.certificateArrayController.selectedObjects lastObject][@"value"]);
    
    if (ident == NULL)
        return;
    
    SecCertificateRef cert;
    SecIdentityCopyCertificate(ident, &cert);
    if (cert == NULL)
        return;

    CFStringRef name = _CSCopyKerberosPrincipalForCertificate(cert);
    CFRelease(cert);
    if (name == NULL)
        return;

    [self.username setStringValue:[NSString stringWithFormat:@"%@@WELLKNOWN:COM.APPLE.LKDC", (__bridge NSString *)name]];
    CFRelease(name);
}

- (IBAction)acquireCredential:(id)sender {
    gss_cred_id_t cred = NULL;
    OM_uint32 maj_stat;
    
    CFErrorRef error = NULL;
    
    gss_name_t gname = GSSCreateName((__bridge CFStringRef)self.username.stringValue, GSS_C_NT_USER_NAME, &error);
    if (gname == NULL) {
        NSLog(@"CreateName failed with: %@", error);
        if (error) CFRelease(error);
        return;
    }
    
    NSMutableDictionary *options = [NSMutableDictionary dictionary];

    NSInteger tag = [self.credentialRadioButton selectedTag];
    NSLog(@"selected tag: %d", (int)tag);
    
    switch (tag) {
        case passwordTag:
            NSLog(@"using password");
            [options setObject:self.password.stringValue forKey:(id)kGSSICPassword];
            break;
        case certificateTag: {
            
            NSLog(@"using certificate");

            SecIdentityRef ident = (__bridge SecIdentityRef)([self.certificateArrayController.selectedObjects lastObject][@"value"]);
            if (ident == NULL) {
                NSLog(@"picked cert never existed ?");
                return;
            }
            
            NSLog(@"using identity: %@ : %@", ident, [self.certificateArrayController.selectedObjects lastObject][@"name"]);
            
            [options setObject:(__bridge id)ident forKey:(id)kGSSICCertificate];
            }
            break;
        default:
            abort();
    }
 
    NSString *hostname = self.kdchostname.stringValue;
    if (hostname && [hostname length] > 0) {
        [options setObject:hostname forKey:(id)kGSSICLKDCHostname];
    }
    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)options, &cred, &error);
    CFRelease(gname);
    if (maj_stat) {
        NSLog(@"gss_aapl_initial_cred failed with: %@", error);
        if (error) CFRelease(error);
    }
    
    if (cred) {
        NSLog(@"got cred: %@", cred);
        
        [self addCredentialTab:cred];

        CFRelease(cred);
    }

    
}



@end
