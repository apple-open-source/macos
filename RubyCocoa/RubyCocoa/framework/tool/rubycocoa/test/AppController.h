/* AppController */

#import <Cocoa/Cocoa.h>

@interface AppController : NSObject
{
    IBOutlet id configController;
    IBOutlet id menu;
    IBOutlet id menuToggleHide;
    IBOutlet id menuToggleIgnoreMouseEvent;
    IBOutlet id menuToggleStealthMode;
    IBOutlet id menuToggleTimeout;
    IBOutlet id winConfig;
}
- (IBAction)about:(id)sender;
- (IBAction)quit:(id)sender;
- (IBAction)toggleHide:(id)sender;
- (IBAction)toggleIgnoreMouseEvent:(id)sender;
- (IBAction)toggleStealthMode:(id)sender;
- (IBAction)toggleTimeout:(id)sender;
@end
