//
//  ViewController.h
//

#import <UIKit/UIKit.h>

@interface ViewController : UIViewController {
    dispatch_queue_t _queue;
}

@property (nonatomic, retain) IBOutlet  UITextView *ticketView;
@property (nonatomic, retain) IBOutlet  UITextField *authServerName;
@property (nonatomic, retain) IBOutlet  UITextField *authServerResult;
@property (nonatomic, retain) IBOutlet  UITextField *urlTextField;
@property (nonatomic, retain) IBOutlet  UITextView *urlResultTextView;

- (IBAction)addCredential:(id)sender;

@end
