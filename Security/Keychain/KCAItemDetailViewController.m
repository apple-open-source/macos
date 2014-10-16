//
//  KCAItemDetailViewController.m
//  Security
//
//  Created by John Hurley on 10/24/12.
//
//

#import "KCAItemDetailViewController.h"
#import "EditItemViewController.h"

@interface KCAItemDetailViewController ()

@end

@implementation KCAItemDetailViewController

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
    
//    NSLog(@"_itemDetailModel: %@", _itemDetailModel);
    _itemName.text = [_itemDetailModel objectForKey:(__bridge id)(kSecAttrService)];
    _itemAccount.text = [_itemDetailModel objectForKey:(__bridge id)(kSecAttrAccount)];
    
    NSData *pwdData = [_itemDetailModel objectForKey:(__bridge id)(kSecValueData)];
    if (pwdData)
    {
        NSString *pwd = [[NSString alloc] initWithData:pwdData encoding:NSUTF8StringEncoding];
        _itemPassword.text = pwd;
    }
    else
        _itemPassword.text = @"";
        
    _itemCreated.text = [[_itemDetailModel objectForKey:(__bridge id)(kSecAttrCreationDate)] description];
    _itemModified.text = [[_itemDetailModel objectForKey:(__bridge id)(kSecAttrModificationDate)] description];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    if ([[segue identifier] isEqualToString:@"ItemEdit"])
    {
        NSLog(@"Preparing seque to EditItemViewController: %@", _itemDetailModel);
        EditItemViewController *editItemViewController = [segue destinationViewController];
        editItemViewController.itemDetailModel = [NSDictionary dictionaryWithDictionary:_itemDetailModel];
    }
}

@end
