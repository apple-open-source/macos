#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import "Preferences.h"

@interface AppController : NSObject {
    Preferences *preferences;
}

- (IBAction)showPreferences:(id)sender;
- (Preferences *)preferences;
- (void)setPreferences:(Preferences *)newPreferences;

@end
