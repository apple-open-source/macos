//
//  ManualTests.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-07-01.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import "ManualTests.h"
#import <GSS/GSS.h>
#import <Security/SecCertificatePriv.h>

@interface ManualTests () <UIPickerViewDataSource, UIPickerViewDelegate, UITextFieldDelegate>
@property (retain) NSArray *identities;
@property (assign) NSInteger selectedRow;
@property (retain) UIActionSheet *actionSheet;
@end

@implementation ManualTests

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView {
    return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component {
    if (component > 0) abort();
    return [self.identities count];
}

- (UIView *)pickerView:(UIPickerView *)pickerView
            viewForRow:(NSInteger)row
          forComponent:(NSInteger)component
           reusingView:(UIView *)view
{
    UILabel* label = (UILabel*)view;
    if (label == nil) {
        label = [[UILabel alloc] init];
        label.minimumScaleFactor = 0.75;
        label.adjustsFontSizeToFitWidth = YES;
    }
    label.text = self.identities[row][@"name"];
    return label;
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component {
    self.certificateLabel.text = self.identities[row][@"name"];
    self.selectedRow = row;
}

- (void)viewDidLoad {
    self.selectedRow = -1;
}

- (void)viewDidAppear:(BOOL)animated {
    
    static CFDataRef smartcardLogon;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        smartcardLogon = CFDataCreate(NULL, (void *)"\x2b\x06\x01\x04\x01\x82\x37\x14\x02\x02", 10);
    });
    
    NSDictionary *params = @{
                             (__bridge id)kSecClass : (__bridge id)kSecClassIdentity,
                             (__bridge id)kSecReturnRef : (__bridge id)kCFBooleanTrue,
                             (__bridge id)kSecMatchLimit : (__bridge id) kSecMatchLimitAll,
                             };
    
    CFTypeRef result = NULL;
    OSStatus status;
    
    status  = SecItemCopyMatching((__bridge CFDictionaryRef)params, &result);
    if (status) {
        NSLog(@"SecItemCopyMatching returned: %d", (int)status);
        return;
    }
    
    NSArray *array = CFBridgingRelease(result);
    
    NSMutableArray *items = [NSMutableArray array];
    
    NSLog(@"found: %lu entries:", (unsigned long)[array count]);

    self.selectedRow = 0;
    
    for (id identity in array) {
        SecIdentityRef ident = (__bridge SecIdentityRef)identity;
        CFArrayRef eku = NULL;
        
        NSLog(@"SecIdentityRef : %@", ident);
        
        SecCertificateRef cert = NULL;
        NSString *name;

        SecIdentityCopyCertificate(ident, &cert);
        if (cert) {
            name = CFBridgingRelease(SecCertificateCopySubjectSummary(cert));
            
            eku = SecCertificateCopyExtendedKeyUsage(cert);

            CFRelease(cert);
        } else {
            name = @"<noname>";
        }
        NSLog(@"SecIdentityRef: %@ subject: %@", identity, name);
        
        [items addObject:@{
                           @"name" : name,
                           @"value" : identity,
                           }];
        
        if (eku) {
            if (CFArrayContainsValue(eku, CFRangeMake(0, CFArrayGetCount(eku)), smartcardLogon))
                self.selectedRow = [items count] - 1;
            CFRelease(eku);
        }
    }
    
    self.identities = items;
    
    if ([items count] > 0) {
        [self.certificateLabel setHidden:NO];
        [self.doCertificate setHidden:NO];
        
        self.certificateLabel.text = self.identities[self.selectedRow][@"name"];
    } else {
        [self.certificateLabel setHidden:YES];
        [self.doCertificate setHidden:YES];
        self.selectedRow = -1;
    }
    
    
    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(certificateTapped)];
    tap.numberOfTapsRequired = 1;
    [self.certificateLabel addGestureRecognizer:tap];

}

- (IBAction)doAcquire:(id)sender {
    NSLog(@"acquire");
    CFErrorRef error = NULL;
    
    gss_cred_id_t cred = NULL;
    OM_uint32 maj_stat;
    
    gss_name_t gname = GSSCreateName((__bridge CFStringRef)self.username.text, GSS_C_NT_USER_NAME, &error);
    if (gname == NULL) {
        self.statusLabel.text = [NSString stringWithFormat:@"import failed with: %@", error];
        NSLog(@"CreateName failed with: %@", error);
        if (error) CFRelease(error);
        return;
    }
    
    NSMutableDictionary *options = [NSMutableDictionary dictionary];
    
    if (self.doPassword.on)
        [options setObject:self.password.text forKey:(id)kGSSICPassword];

    if (self.doCertificate.on && self.selectedRow >= 0) {
        SecIdentityRef ident = (__bridge SecIdentityRef)self.identities[self.selectedRow][@"value"];
        if (ident == NULL) {
            NSLog(@"picked cert never existed ?");
            return;
        }

        [options setObject:(__bridge id)ident forKey:(id)kGSSICCertificate];
    }
    
    NSString *hostname = self.kdchostname.text;
    if (hostname && [hostname length] > 0) {
        [options setObject:hostname forKey:(id)kGSSICLKDCHostname];
    }

    self.statusLabel.text = @"acquiring...";
    
    maj_stat = gss_aapl_initial_cred(gname, GSS_KRB5_MECHANISM, (__bridge CFDictionaryRef)options, &cred, &error);
    CFRelease(gname);
    if (maj_stat) {
        self.statusLabel.text = [NSString stringWithFormat:@"failed: %@", error];
        NSLog(@"gss_aapl_initial_cred failed with: %@", error);
        if (error)
            CFRelease(error);
    } else if (cred) {
        self.statusLabel.text = [NSString stringWithFormat:@"got cred: %@", cred];
        NSLog(@"got cred: %@", cred);
    } else {
        self.statusLabel.text = [NSString stringWithFormat:@"got no error, but also didn't get a cred!?"];
    }
    
    if (cred)
        CFRelease(cred);
}

- (void)dismissActionSheet:(id)sender {
    if (self.selectedRow >= 0)
        self.certificateLabel.text = self.identities[self.selectedRow][@"name"];
    [self.actionSheet dismissWithClickedButtonIndex:0 animated:YES];
    self.actionSheet = nil;
}

- (void)certificateTapped {
    self.actionSheet = [[UIActionSheet alloc] initWithTitle:nil
                                                             delegate:nil
                                                    cancelButtonTitle:nil
                                               destructiveButtonTitle:nil
                                                    otherButtonTitles:nil];
    
    [self.actionSheet setActionSheetStyle:UIActionSheetStyleBlackTranslucent];
    
    CGRect pickerFrame = CGRectMake(0, 40, 0, 0);
    
    UIPickerView *certifiatePicker = [[UIPickerView alloc] initWithFrame:pickerFrame];
    certifiatePicker.showsSelectionIndicator = YES;
    certifiatePicker.dataSource = self;
    certifiatePicker.delegate = self;
    
    if (self.selectedRow >= 0)
        [certifiatePicker selectRow:self.selectedRow inComponent:0 animated:NO];

    [self.actionSheet addSubview:certifiatePicker];
    
    
    UISegmentedControl *closeButton = [[UISegmentedControl alloc] initWithItems:[NSArray arrayWithObject:@"Close"]];
    closeButton.momentary = YES;
    closeButton.frame = CGRectMake(260, 7.0f, 50.0f, 30.0f);
    closeButton.tintColor = [UIColor blackColor];
    [closeButton addTarget:self action:@selector(dismissActionSheet:) forControlEvents:UIControlEventValueChanged];
    [self.actionSheet addSubview:closeButton];

    
    [self.actionSheet showInView:self.view];
    [self.actionSheet setBounds:CGRectMake(0, 0, 320, 485)];
}

-(BOOL)textFieldShouldReturn:(UITextField *)sender {
    [sender resignFirstResponder];
    return YES;
}

@end
