
#import <Cocoa/Cocoa.h>

@interface BootCacheLoginPlugin : NSObject 
{

}

- (id)init;
- (void)dealloc;

    //  This method gets called when loginwindow starts up, before
    //  a user has logged in.
- (void)didStartup;

    //  This method gets called after a user has successfully
    //  authenticated but before loginwindow has setuid to the
    //  logging-in user.
    //
    //  If the plugin returns NO from this method, the login
    //  process for that user will be interrupted and loginwindow
    //  will return to the login screen.
    //
    //  As a matter of good HI principle, a plugin should never
    //  return NO from this method without first presenting an alert
    //  indicating to the user the reason why the login was disallowed.
- (BOOL)isLoginAllowedForUserID:(uid_t)userID;

    //  This method is called just after the Finder has been launched,
    //  but before any of the other systems apps (Dock, SystemUIServer)
    //  or login items have been launched.
- (void)didLogin;

    //  This method gets called just before the user is logged out.
- (void)willLogout;

    //  This method is called just before the loginwindow app terminates.
- (void)willTerminate;

@end
