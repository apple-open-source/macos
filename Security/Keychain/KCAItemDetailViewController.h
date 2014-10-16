//
//  KCAItemDetailViewController.h
//  Security
//
//  Created by John Hurley on 10/24/12.
//
//

#import <UIKit/UIKit.h>

@interface KCAItemDetailViewController : UIViewController
{
    
}
@property (weak, nonatomic) NSDictionary *itemDetailModel;
@property (weak, nonatomic) IBOutlet UITextField *itemName;
@property (weak, nonatomic) IBOutlet UITextField *itemAccount;
@property (weak, nonatomic) IBOutlet UITextField *itemPassword;
@property (weak, nonatomic) IBOutlet UITextField *itemCreated;
@property (weak, nonatomic) IBOutlet UITextField *itemModified;

@end
