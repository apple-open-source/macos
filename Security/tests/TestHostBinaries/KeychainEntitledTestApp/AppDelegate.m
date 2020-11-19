#import "AppDelegate.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

#if TARGET_OS_OSX

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {}
- (void)applicationWillTerminate:(NSNotification *)aNotification {}

#else

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {return YES;}
- (void)applicationWillResignActive:(UIApplication *)application {}
- (void)applicationDidEnterBackground:(UIApplication *)application {}
- (void)applicationWillEnterForeground:(UIApplication *)application {}
- (void)applicationDidBecomeActive:(UIApplication *)application {}
- (void)applicationWillTerminate:(UIApplication *)application {}

#endif

@end
