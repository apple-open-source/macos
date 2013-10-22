//
//  CredentialTesterView.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-07-03.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "CredentialTesterView.h"

@interface CredentialTesterView ()
@property (assign) gss_cred_id_t credential;


@end

@implementation CredentialTesterView

- (id)initWithGSSCredential:(gss_cred_id_t)credential   
{
    self = [super initWithNibName:@"CredentialTesterView" bundle:nil];
    if (self) {
        CFRetain(credential);
        self.credential = credential;
        
        gss_name_t gssName = GSSCredentialCopyName(credential);
        if (gssName) {
            self.name = (__bridge NSString *)GSSNameCreateDisplayString(gssName);
            CFRelease(gssName);
        } else {
            self.name = @"<credential have no name>";
        }
        
        OM_uint32 lifetime = GSSCredentialGetLifetime(credential);
        
        self.expire = [NSDate dateWithTimeIntervalSinceNow:lifetime];
    }
    
    
    return self;
}

- (void)loadView {
    [super loadView];
    
    if ([self.name rangeOfString:@"@LKDC:"].location != NSNotFound) {
        [self.serverName setStringValue:@"host@WELLKNOWN:COM.APPLE.LKDC"];
    } else if ([self.name rangeOfString:@"@ADS.APPLE.COM"].location != NSNotFound) {
        [self.serverName setStringValue:@"HTTP@dc03.ads.apple.com"];
    }
}

- (IBAction)testISC:(id)sender {
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t gssName = GSS_C_NO_NAME;
    gss_buffer_desc buffer;
    OM_uint32 maj_stat, min_stat;
    CFErrorRef error = NULL;
    
    NSString *serverName = [self.serverName stringValue];
    
    NSLog(@"isc: %@ to %@", self.credential, serverName);
    
    
    gssName = GSSCreateName((__bridge CFStringRef)serverName, GSS_C_NT_HOSTBASED_SERVICE, NULL);
    if (gssName == NULL) {
        NSLog(@"import_name %@", error);
        if (error) CFRelease(error);
        return;
    }
    
    maj_stat = gss_init_sec_context(&min_stat, self.credential,
                                    &ctx, gssName, GSS_KRB5_MECHANISM,
                                    GSS_C_REPLAY_FLAG|GSS_C_INTEG_FLAG, 0, GSS_C_NO_CHANNEL_BINDINGS,
                                    NULL, NULL, &buffer, NULL, NULL);
    if (maj_stat) {
        [self.iscStatus setStringValue:[NSString stringWithFormat:@"FAIL init_sec_context maj_stat: %d", (int)maj_stat]];
    } else {
        [self.iscStatus setStringValue:[NSString stringWithFormat:@"success: buffer of length: %d, success", (int)buffer.length]];
	}

    gss_delete_sec_context(&min_stat, &ctx, NULL);
    CFRelease(gssName);
    gss_release_buffer(&min_stat, &buffer);
}


- (IBAction)destroyCredential:(id)sender {
    OM_uint32 min_stat;
    gss_destroy_cred(&min_stat, &_credential);
    self.name = @"";
    self.expire = [NSDate distantPast];
    
    [self.tabViewItem.tabView removeTabViewItem:self.tabViewItem];
}


@end
