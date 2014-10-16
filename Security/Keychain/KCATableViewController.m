//
//  KCATableViewController.m
//  Security
//
//  Created by John Hurley on 10/22/12.
//
//

/*
    Sample:
    
    (lldb) po allItems
    (NSMutableDictionary *) $3 = 0x0855f200 <__NSCFArray 0x855f200>(
    {
        acct = "Keychain Sync Test Account";
        agrp = test;
        cdat = "2012-10-04 21:59:46 +0000";
        gena = <4b657963 6861696e 2053796e 63205465 73742050 61737377 6f726420 44617461>;
        mdat = "2012-10-08 21:02:39 +0000";
        pdmn = ak;
        svce = "Keychain Sync Test Service";
    },
    {
        acct = "";
        agrp = test;
        cdat = "2012-10-22 21:08:14 +0000";
        gena = <4b657963 6861696e 2053796e 63205465 73742050 61737377 6f726420 44617461>;
        mdat = "2012-10-22 21:08:14 +0000";
        pdmn = ak;
        svce = "";
    },
    {
        acct = iacct;
        agrp = test;
        cdat = "2012-10-22 21:08:29 +0000";
        gena = <4b657963 6861696e 2053796e 63205465 73742050 61737377 6f726420 44617461>;
        mdat = "2012-10-22 21:08:29 +0000";
        pdmn = ak;
        svce = iname;
    },
    {
        acct = bar;
        agrp = test;
        cdat = "2012-10-23 16:57:03 +0000";
        gena = <4b657963 6861696e 2053796e 63205465 73742050 61737377 6f726420 44617461>;
        mdat = "2012-10-23 16:57:03 +0000";
        pdmn = ak;
        svce = baz;
    },
    {
        acct = foo9;
        agrp = test;
        cdat = "2012-10-24 22:11:54 +0000";
        gena = <4b657963 6861696e 2053796e 63205465 73742050 61737377 6f726420 44617461>;
        mdat = "2012-10-24 22:11:54 +0000";
        pdmn = ak;
        svce = passfoo9;
    }
    )
*/

#import "KCATableViewController.h"
#import "KeychainItemCell.h"
#import "MyKeychain.h"
#import "KCAItemDetailViewController.h"
#import <notify.h>
#import <Security/SecItemInternal.h>
#import <CoreFoundation/CFUserNotification.h>
#import <SpringBoardServices/SpringBoardServices.h>
#if TARGET_OS_EMBEDDED
#import <MobileKeyBag/MobileKeyBag.h>
#endif


@interface KCATableViewController ()
@end

@implementation KCATableViewController

- (void)viewDidLoad
{
    NSLog(@"KCATableViewController: viewDidLoad");
    [super viewDidLoad];

    _lockTimer = [NSTimer timerWithTimeInterval:15.0 target:self selector:@selector(stopLockTimer:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer: _lockTimer forMode: NSDefaultRunLoopMode];
    
    notify_register_dispatch(kSecServerKeychainChangedNotification, &notificationToken, dispatch_get_main_queue(),
        ^ (int token __unused)
        {
            NSLog(@"Received %s", kSecServerKeychainChangedNotification);
            [self.tableView reloadData];
        });
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (BOOL)shouldPerformSegueWithIdentifier:(NSString *)identifier sender:(id)sender
{
    // Invoked immediately prior to initiating a segue. Return NO to prevent the segue from firing. The default implementation returns YES.
    if (hasUnlockedRecently)
        return YES;
    if ([identifier isEqualToString:@"ItemDetail"])
        return [self askForPassword];
    return YES;
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    if ([[segue identifier] isEqualToString:@"ItemDetail"])
    {
        KCAItemDetailViewController *detailViewController = [segue destinationViewController];
        NSIndexPath *myIndexPath = [self.tableView indexPathForSelectedRow];
        CFIndex row = [myIndexPath row];
        //TODO - horribly inefficient !
        NSArray *items = [self getItems];
        detailViewController.itemDetailModel = [items objectAtIndex: row];
    }
}

// MARK: - Table view data source

- (NSArray *)getItems
{
    // Each array element is a dictionary. If svce is present, compare
    NSArray *allItems = (NSArray *)[[MyKeychain sharedInstance] fetchDictionaryAll];
    
    return [allItems sortedArrayUsingComparator:^NSComparisonResult(id obj1, id obj2) {
        NSString *service1 = obj1[(__bridge id)(kSecAttrService)];
        NSString *service2 = obj2[(__bridge id)(kSecAttrService)];
        return [service1 compare:service2];
    }];

    return allItems;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    // Return the number of sections.
    return 1;   //TODO
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
    NSInteger count = [[self getItems] count];
//  NSLog(@"numberOfRowsInSection: %d", count);
    self.navigationController.navigationBar.topItem.title = [NSString stringWithFormat:@"All Items (%ld)", (long)count];
    
//  NSLog(@"Items: %@", [self getItems]);
    return count;
}

- (BOOL)itemUpdatedRecently:(NSDictionary *)item
{
    const NSTimeInterval recent = 15.0;    // within the last 15 seconds
    NSDate *modDate = [item[(__bridge id)kSecAttrModificationDate] dateByAddingTimeInterval:recent];
    NSDate *now = [NSDate dateWithTimeIntervalSinceNow:0];

//  NSLog(@"Mod date: %@, now: %@", modDate, now);
    return [modDate compare:now] == NSOrderedDescending;    // i.e. modDate+15s > now
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    KeychainItemCell *cell = [tableView dequeueReusableCellWithIdentifier:@"kcTableCell" forIndexPath:(NSIndexPath *)indexPath];
    if (cell == nil)
    {
        NSLog(@"cellForRowAtIndexPath : cell was nil");
        cell = [[KeychainItemCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"kcTableCell"];
    }

    // Configure the cell...

    NSUInteger row = [indexPath row];

    NSArray *items = [self getItems];
    NSDictionary *theItem = [items objectAtIndex: row];

    NSString *svce = [theItem objectForKey: (__bridge id)(kSecAttrService)];
    NSString *acct = [theItem objectForKey: (__bridge id)(kSecAttrAccount)];
    

 if ([self itemUpdatedRecently:theItem])
        [cell startCellFlasher];
/*    else
        cell.itemStatus.text = @"";
*/
    cell.itemName.text = (svce && [svce length]) ? svce : @"<<no service>>";
    cell.itemAccount.text = (acct  && [acct length])? acct : @"<<no account>>";
    
    [cell.itemAccount setTextColor:[UIColor grayColor]];
    
    return cell;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    OSStatus status;
    NSUInteger row = [indexPath row];
    
    NSArray *items = [self getItems];
    NSDictionary *item = [items objectAtIndex: row];
    NSMutableDictionary *query = [NSMutableDictionary dictionaryWithDictionary:item];
    
    [query removeObjectForKey:(__bridge id)kSecValueData];
    [query removeObjectForKey:(__bridge id)kSecAttrCreationDate];
    [query removeObjectForKey:(__bridge id)kSecAttrModificationDate];
    [query removeObjectForKey:(__bridge id)kSecAttrGeneric];
    [query removeObjectForKey:@"tomb"];
    query[(__bridge id)(kSecClass)] = (__bridge id)(kSecClassGenericPassword);

//  NSLog(@"Item: %@", item);
//  NSLog(@"Query: %@", query);
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status)
        NSLog(@"Error from SecItemDelete: %d", (int)status);
    else
        [self.tableView reloadData];
}

// MARK: Ask for PIN

- (void)startLockTimer
{
    NSLog(@"startLockTimer");
    hasUnlockedRecently = true;
    [_lockTimer setFireDate:[NSDate dateWithTimeIntervalSinceNow:60.0]];
}

- (void)stopLockTimer:(NSTimer *)timer
{
//    NSLog(@"stopLockTimer");
    // Call when we hit home button
//    [_lockTimer invalidate];
    hasUnlockedRecently = false;
}

- (BOOL)unlockDeviceWithPasscode:(NSString *)passcode
{
#if TARGET_OS_EMBEDDED
    int status = kMobileKeyBagError;
    NSData *passcodeData = [passcode dataUsingEncoding:NSUTF8StringEncoding];
    if (MKBGetDeviceLockState(NULL) != kMobileKeyBagDisabled)
        status = MKBUnlockDevice((__bridge CFDataRef)passcodeData, NULL);
    else
        status = kMobileKeyBagSuccess;
    
    // #define kMobileKeyBagDeviceLockedError (-2)

    if (status != kMobileKeyBagSuccess)
    {
        NSLog(@"Could not unlock device. Error: %d", status);
        return NO;
    }
#endif
    [self startLockTimer];
    return YES;
}

#define NO_AUTOCAPITALIZATION 0
#define NO_AUTOCORRECTION   1

- (BOOL)askForPassword
{
    // Return YES if authenticated
    CFUserNotificationRef dialog_alert = 0;
    SInt32 err;
    NSMutableDictionary *nd = [NSMutableDictionary dictionaryWithCapacity:0];
    CFOptionFlags flags = kCFUserNotificationCautionAlertLevel | kCFUserNotificationNoDefaultButtonFlag;
    NSString *passcode;
    
    // Header and buttons
    nd[(NSString *)kCFUserNotificationAlertHeaderKey] = @"Unlock";
    nd[(NSString *)kCFUserNotificationAlertMessageKey]  = @"To view details";
    nd[(NSString *)kCFUserNotificationDefaultButtonTitleKey] = @"OK";
    nd[(NSString *)kCFUserNotificationAlternateButtonTitleKey] = @"Cancel";
    nd[(__bridge __strong id)(SBUserNotificationGroupsTextFields)] = (__bridge id)(kCFBooleanTrue);
    nd[(NSString *)kCFUserNotificationTextFieldTitlesKey] = @[@"Passcode"];
    nd[(__bridge NSString *)SBUserNotificationTextAutocapitalizationType] = @[ @(NO_AUTOCAPITALIZATION) ];
    nd[(__bridge NSString *)SBUserNotificationTextAutocapitalizationType] = @[ @(NO_AUTOCORRECTION) ];

    flags = kCFUserNotificationPlainAlertLevel | CFUserNotificationSecureTextField(0);
    
    dialog_alert = CFUserNotificationCreate(NULL, 0, flags, &err, (__bridge CFMutableDictionaryRef)nd);
    if (!dialog_alert)
        return NO;
    
    CFUserNotificationReceiveResponse(dialog_alert, 0, &flags);
    // the 2 lower bits of the response flags will give the button pressed
    // 0 --> default
    // 1 --> alternate
    if (flags & kCFUserNotificationCancelResponse) {  // user cancelled
        if (dialog_alert)
            CFRelease(dialog_alert);
        return NO;
    }
        
    // user clicked OK
    passcode = CFBridgingRelease(CFUserNotificationGetResponseValue(dialog_alert, kCFUserNotificationTextFieldValuesKey, 0));
    // test using MKBUnlockDevice
//  NSLog(@"PIN: %@", passcode);      // TODO: REMOVE THIS!!!!
    
    if (dialog_alert)
        CFRelease(dialog_alert);
    return [self unlockDeviceWithPasscode:passcode];
}

@end
