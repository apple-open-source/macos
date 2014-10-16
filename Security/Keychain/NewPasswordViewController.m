//
//  NewPasswordViewController.m
//  Security
//
//  Created by john on 10/24/12.
//
//

#import "NewPasswordViewController.h"
#import "MyKeychain.h"
//#import <SOSCircle/Regressions/SOSRegressionUtilities.h>
#import <CKBridge/SOSCloudKeychainClient.h>
#import <CKBridge/SOSCloudKeychainConstants.h>
#include "utilities.h"

static const CFStringRef kAddItemKeyX = CFSTR("AddItem");


@interface NewPasswordViewController ()

@end

@implementation NewPasswordViewController

- (void)viewDidLoad
{
    NSLog(@"NewPasswordViewController:viewDidLoad");
    [super viewDidLoad];

        dgroup = dispatch_group_create();
        xpc_queue = dispatch_queue_create("NewPasswordViewController", DISPATCH_QUEUE_CONCURRENT);
    // Uncomment the following line to preserve selection between presentations.
    // self.clearsSelectionOnViewWillAppear = NO;
 
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)postToCloud:(NSDictionary *)kcitem
{
    CFErrorRef error = NULL;
    testPutObjectInCloud(kAddItemKeyX, (__bridge CFTypeRef)(kcitem), &error, dgroup, xpc_queue);
    NSLog(@"Sent new item to cloud: %@", kcitem);
}

- (IBAction)handleNewPasswordDone:(id)sender
{
    // [self performSegueWithIdentifier: @"SegueToScene1" sender: self];
    NSLog(@"NewPasswordViewController:handleAddButton");

    [[MyKeychain sharedInstance] setPasswordFull:[_itemAccount text] service:[_itemName text] password:[_itemPassword text]];
    
    [self performSegueWithIdentifier:@"AllItemsSegue" sender:self];
}

@end
