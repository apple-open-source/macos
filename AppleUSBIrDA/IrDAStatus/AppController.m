#import "AppController.h"

@implementation AppController

+ (void)initialize {
    /*
    Initalize executes before the first message to the class is
    serviced therefore, we get our factory settings registered.
    Note that we do this here rather than in the Preferences as the
    Preferences instance is loaded lazily, so the Prefernces class
    may not be initialized before we need our first default
    (see applicationShouldOpenUntitledFile).
    */
    NSString		*path			= [[NSBundle mainBundle] pathForResource:@"Defaults" ofType:@"plist"];
    NSDictionary	*defaultValues	= [NSDictionary dictionaryWithContentsOfFile: path];
   [[NSUserDefaults standardUserDefaults] registerDefaults: defaultValues];
}


- (IBAction)showPreferences:(id)sender {
    [[self preferences] show:self];
}

- (Preferences *)preferences {
    // load preferences lazily
    if (preferences == nil) {
        Preferences *p = [[Preferences alloc] init];
        [self setPreferences:p];
        [p release];
    }
    return preferences;
}
- (void)setPreferences:(Preferences *)newPreferences {
    [newPreferences retain];
    [preferences release];
    preferences = newPreferences;
}

- (void)dealloc {
    [self setPreferences:nil];
    [super dealloc];
}


@end
