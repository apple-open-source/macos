//
//  CredentialTableController.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2014-08-16.
//  Copyright (c) 2014 Apple, Inc. All rights reserved.
//

#import "CredentialTableController.h"
#import <GSS/GSS.h>
#import <notify.h>

@interface CredentialTableController ()
@property (assign) CFMutableArrayRef array;
@property (assign) CFMutableDictionaryRef credentials;
@property (assign) int token;
@property (strong) NSMutableSet *notificationObjects;
@end

@implementation CredentialTableController

+ (CredentialTableController *)getGlobalController {
    static dispatch_once_t onceToken;
    static CredentialTableController *controller = NULL;
    dispatch_once(&onceToken, ^{
        controller = [[CredentialTableController alloc] init];
    });
    return controller;
}

- (CredentialTableController *)init {
    if ((self = [super init]) == nil)
        return nil;

    self.array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    self.credentials = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    [self updateCredentials];

    self.notificationObjects = [NSMutableSet set];

    self.token = NOTIFY_TOKEN_INVALID;
    notify_register_dispatch("com.apple.Kerberos.cache.changed", &_token, dispatch_get_main_queue(), ^(int token) {
        [self updateCredentials];
    });

    return self;
}

- (void)dealloc {
    notify_cancel(self.token);
    CFRelease(_array);
    CFRelease(_credentials);
}

- (void)addNotification:(id<GSSCredentialsChangeNotification>)object
{
    [self.notificationObjects addObject:object];
}

- (void)removeNotification:(id<GSSCredentialsChangeNotification>)object
{
    [self.notificationObjects removeObject:object];
}


- (void)updateCredentials {
    OM_uint32 min_stat;
    __block bool changeHappned = false;

    CFMutableDictionaryRef removedItems = CFDictionaryCreateMutableCopy(NULL, 0, self.credentials);

    gss_iter_creds(&min_stat, 0, GSS_KRB5_MECHANISM, ^(gss_OID mechOID, gss_cred_id_t cred) {
        CFStringRef nameString = NULL;
        gss_name_t name = NULL;
        CFUUIDRef uuid;

        if (cred == NULL)
            return;

        uuid = GSSCredentialCopyUUID(cred);
        if (uuid == NULL) {
            CFRelease(cred);
            return;
        }

        if (CFDictionaryGetValue(self.credentials, uuid)) {
            CFDictionaryRemoveValue(removedItems, uuid);
            CFRelease(uuid);
            CFRelease(cred);
            return;
        }

        name = GSSCredentialCopyName(cred);
        if (name) {
            nameString = GSSNameCreateDisplayString(name);
            CFRelease(name);
        }

        if (nameString) {
            CFDictionarySetValue(self.credentials, uuid, nameString);
            CFArrayAppendValue(self.array, uuid);
            NSLog(@"adding: %@", nameString);
            CFRelease(nameString);
            changeHappned = true;
        }
        CFRelease(uuid);
        CFRelease(cred);
    });

    [(__bridge NSDictionary *)removedItems enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        CFIndex n = CFArrayGetFirstIndexOfValue(self.array, CFRangeMake(0, CFArrayGetCount(self.array)), (__bridge CFUUIDRef)key);
        if (n > 0) {
            CFArrayRemoveValueAtIndex(self.array, n);
            changeHappned = true;
        }
    }];

    CFRelease(removedItems);

    if (changeHappned) {
        for (id<GSSCredentialsChangeNotification> object in self.notificationObjects) {
            [object GSSCredentialChangeNotifification];
        }
    }
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return CFArrayGetCount(self.array);
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *cellIdentifier = @"CredentialTableViewCell";

    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];

    if (cell == nil)
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cellIdentifier];

    CFUUIDRef uuid = (CFUUIDRef)CFArrayGetValueAtIndex(self.array, indexPath.row);
    NSString *name = NULL;
    if (uuid)
        name = CFDictionaryGetValue(self.credentials, uuid);
    cell.textLabel.text = name ? name : @"???";

    return cell;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    return YES;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle == UITableViewCellEditingStyleDelete) {

        CFUUIDRef uuid = (CFUUIDRef)CFArrayGetValueAtIndex(self.array, indexPath.row);

        OM_uint32 min_stat;
        gss_cred_id_t cred = GSSCreateCredentialFromUUID(uuid);
        if (cred)
            gss_destroy_cred(&min_stat, &cred);

        CFDictionaryRemoveValue(self.credentials, uuid);
        CFArrayRemoveValueAtIndex(self.array, indexPath.row);

        [tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
    }
}


@end
