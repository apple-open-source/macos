#import "Preferences.h"

@implementation Preferences

NSString *DefaultDriverKey = @"DefaultDriver";

static NSString *nibName = @"Preferences";

- (IBAction)update:(id)sender {
    // provide a single update action method for all defaults
    // it's a lot easier than setting each indvidually...
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject: [defaultDriverField stringValue] forKey: DefaultDriverKey];
 }

- (void)windowDidResignKey:(NSNotification *)n {
    // make sure defaults are saved automatically after edits
    [self update: self];
}

- (void)windowDidClose:(NSNotification *)n {
    // make sure defaults are saved automatically when window closed
    [self update: self];
}

- (IBAction)show:(id)sender
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (!panel) {
        if (![NSBundle loadNibNamed: nibName owner: self])
            NSLog(@"Unable to load nib \"%@\"",nibName);
    }
    [defaultDriverField setStringValue: [defaults objectForKey: DefaultDriverKey]];
    [panel makeKeyAndOrderFront: self];
}

@end
