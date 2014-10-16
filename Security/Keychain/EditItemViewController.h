//
//  EditItemViewController.h
//  Security
//
//  Created by john on 10/24/12.
//
//

#import <UIKit/UIKit.h>

@interface EditItemViewController : UIViewController
{
    dispatch_group_t dgroup;
    dispatch_queue_t xpc_queue;
}
@property (strong, nonatomic) NSDictionary *itemDetailModel;

@property (weak, nonatomic) IBOutlet UITextField *itemName;
@property (weak, nonatomic) IBOutlet UITextField *itemAccount;
@property (weak, nonatomic) IBOutlet UITextField *itemPassword;

@end
