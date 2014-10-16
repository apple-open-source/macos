//
//  DeviceTableViewController.m
//  Security
//
//


#import "DeviceTableViewController.h"
#import "DeviceItemCell.h"

static NSString *redCircle = @"🔴";
static NSString *blueCircle = @"🔵";

enum
{
    KCA_ITEM_STATUS_ID = 300,
    KCA_ITEM_NAME_ID = 301,
    KCA_ITEM_ACCT_ID = 302,
};

@interface DeviceTableViewController ()

@end

@implementation DeviceTableViewController

- (id)initWithStyle:(UITableViewStyle)style
{
    NSLog(@"DeviceTableViewController: initWithStyle");
    if (self = [super initWithStyle:style])
    {
        // Custom initialization
   //     [self.tableView registerClass:(Class)[KeychainItemCell class] forCellReuseIdentifier:(NSString *)@"kciCell"];

        _kcItemStatuses = [[NSArray alloc] initWithObjects:@"",redCircle, blueCircle,@"",@"",blueCircle,nil];
        _kcItemNames = [[NSArray alloc] initWithObjects:@"Facebook",@"iCloud", @"WSJ",@"Twitter",@"NYTimes",@"Wells Fargo", nil];
   //     self.navigationItem.rightBarButtonItem = self.editButtonItem;
    }
    return self;
}

#if 0
- (void)viewDidLoad
{
    NSLog(@"DeviceTableViewController: viewDidLoad");
    [super viewDidLoad];

    // Uncomment the following line to preserve selection between presentations.
    // self.clearsSelectionOnViewWillAppear = NO;
    _kcItemStatuses = [[NSArray alloc] initWithObjects:@"",redCircle, blueCircle,@"",@"",nil];
    _kcItemNames = [[NSArray alloc] initWithObjects:@"Facebook",@"iCloud", @"WSJ",@"Twitter",@"NYTimes",nil];
    
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    self.navigationItem.rightBarButtonItem = self.editButtonItem;
}
#endif

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    if ([[segue identifier] isEqualToString:@"ItemDetail"])
    {
#if 0
        KCAItemDetailViewController *detailViewController = [segue destinationViewController];
        NSIndexPath *myIndexPath = [self.tableView indexPathForSelectedRow];
        int row = [myIndexPath row];
        //TODO - horribly inefficient !
    NSArray *items = [self getItems];
        detailViewController.itemDetailModel = [items objectAtIndex: row];
#endif
    }
}

// MARK: - Table view data source

- (NSArray *)getItems
{
    NSArray *allItems = NULL;//(NSArray *)[[MyKeychain sharedInstance] fetchDictionaryAll];
    return allItems;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    // Return the number of sections.
    NSLog(@"numberOfSectionsInTableView");
    return 1;   //TODO
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
    
    // SOSCircleCountPeers(circle) + SOSCircleCountApplicants(circle)
    
    static bool dumpedit = false;
    
    if (!dumpedit)
    {
        NSMutableDictionary *allItems = NULL;//[[MyKeychain sharedInstance] fetchDictionaryAll];
     NSLog(@"numberOfRowsInSection: items: %lu", (unsigned long)[allItems count]);
   
        dumpedit = true;
    }
    //TODO
#if _USE_TEST_DATA
return _kcItemStatuses.count;
#else
    NSInteger count = [[self getItems] count];
    NSLog(@"numberOfRowsInSection: %ld", (long)count);
    return count;
#endif

    return 1;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
#if 1
    NSLog(@"cellForRowAtIndexPath %@", indexPath);

//- (id)dequeueReusableCellWithIdentifier:(NSString *)identifier forIndexPath:(NSIndexPath *)indexPath NS_AVAILABLE_IOS(6_0); // newer dequeue method guarantees a cell is returned and resized properly, assuming identifier is registered

//    KeychainItemCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];

//    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:KCAItemCellIdentifier forIndexPath:(NSIndexPath *)indexPath];
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"deviceTableCell" forIndexPath:(NSIndexPath *)indexPath];
    if (cell == nil)
    {
        NSLog(@"cellForRowAtIndexPath : cell was nil");
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"deviceTableCell"];
    }

    // Configure the cell...

    NSUInteger row = [indexPath row];

#if _USE_TEST_DATA
    UILabel *statusLabel = (UILabel *)[cell viewWithTag:KCA_ITEM_STATUS_ID];
    statusLabel.text = [_kcItemStatuses objectAtIndex: row];
    UILabel *nameLabel = (UILabel *)[cell viewWithTag:KCA_ITEM_NAME_ID];
    nameLabel.text = [_kcItemNames objectAtIndex: row];
#else
    NSArray *items = [self getItems];
    NSDictionary *theItem = [items objectAtIndex: row];
    UILabel *statusLabel = (UILabel *)[cell viewWithTag:KCA_ITEM_STATUS_ID];
    statusLabel.text = [_kcItemStatuses objectAtIndex: row];
    UILabel *nameLabel = (UILabel *)[cell viewWithTag:KCA_ITEM_NAME_ID];
    nameLabel.text = [theItem objectForKey: (__bridge id)(kSecAttrService)];
    UILabel *accountLabel = (UILabel *)[cell viewWithTag:KCA_ITEM_ACCT_ID];
    accountLabel.text = [theItem objectForKey: (__bridge id)(kSecAttrAccount)];
#endif

/*
    WHY DOESNT THIS WORK !!!
    cell.itemAccount.text =
    cell.itemStatus.text =
    cell.itemName.text = [_kcItemNames objectAtIndex: [indexPath row]];
*/
    return cell;
#else
    static NSString *CellIdentifier = @"DeviceCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }
    
    // Configure the cell...
//    cell.textLabel.text = @"1234";
 cell.textLabel.text = self.objects[indexPath.row];
#endif
    return cell;
}


@end
