#import "ObjcController.h"

@implementation ObjcController

- (IBAction)btnClicked:(id)sender
{
  [textField setStringValue:
    [NSString stringWithFormat: @"%@ !!", [sender title]]];
}

@end
