//
//  EditItemViewController.m
//  Security
//
//  Created by john on 10/24/12.
//
//

#import "EditItemViewController.h"
#import "MyKeychain.h"
//#import <regressions/SOSRegressionUtilities.h>
//#import <SOSCircle/Regressions/SOSRegressionUtilities.h>
#import <CKBridge/SOSCloudKeychainClient.h>
#import <CKBridge/SOSCloudKeychainConstants.h>
#include "utilities.h"

static const CFStringRef kAddItemKeyY = CFSTR("AddItem");


@interface EditItemViewController ()

@end

@implementation EditItemViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
    NSLog(@"_itemDetailModel: %@", _itemDetailModel);

    dgroup = dispatch_group_create();
    xpc_queue = dispatch_queue_create("EditItemViewController", DISPATCH_QUEUE_CONCURRENT);
    _itemName.text = [_itemDetailModel objectForKey:(__bridge id)(kSecAttrService)];
    _itemAccount.text = [_itemDetailModel objectForKey:(__bridge id)(kSecAttrAccount)];
//    _itemPassword.text = @"TODO";//[_itemDetailModel objectForKey:(__bridge id)(kSecValueData)];
    NSData *pwdData = [_itemDetailModel objectForKey:(__bridge id)(kSecValueData)];
    if (pwdData)
    {
        NSString *pwd = [[NSString alloc] initWithData:pwdData encoding:NSUTF8StringEncoding];
        _itemPassword.text = pwd;
    }
    else
        _itemPassword.text = @"";

}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)postToCloud:(NSDictionary *)kcitem
{
    CFErrorRef error = NULL;
    testPutObjectInCloud(kAddItemKeyY, (__bridge CFTypeRef)(kcitem), &error, dgroup, xpc_queue);
    NSLog(@"NOT IMPLEMENTED: Sent new item to cloud: %@", kcitem);
}

- (IBAction)handlePasswordEditDone:(id)sender
{
    NSLog(@"handlePasswordEditDone");

    NSMutableDictionary *newItem = [NSMutableDictionary dictionaryWithCapacity:0];
    [newItem setObject:[_itemPassword text] forKey:kItemPasswordKey];
    [newItem setObject:[_itemAccount text] forKey:kItemAccountKey];
    [newItem setObject:[_itemName text] forKey:kItemNameKey];

    [[MyKeychain sharedInstance] setItem:newItem];
    [self postToCloud:newItem];
}


- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
        NSLog(@"prepareForSegue EditDone");
    if ([[segue identifier] isEqualToString:@"EditDone"])
    {
        NSLog(@"seque EditDone");
        [self handlePasswordEditDone:NULL];
    }
}

@end
