#import <AppKit/AppKit.h>

extern NSString *DefaultDriverKey;

@interface Preferences : NSObject
{
    IBOutlet NSPanel  *panel;
    IBOutlet NSTextField *defaultDriverField;
}

- (IBAction)show:(id)sender;
- (IBAction)update:(id)sender;
@end
