#import "SettingsWindow.h"

// @@@ need to add code to remember the radio button settings as a pref

@implementation SettingsWindow

- (void)applicationDidFinishLaunching:(NSNotification *)val
{
	[myWindow makeKeyAndOrderFront:self];
	[NSApp activateIgnoringOtherApps:YES];
}

// @@@ don't need this right now as the nib is connected to terminate
- (void)cancel:(id)sender
{

}

- (void)ok:(id)sender
{
	int status;
	char commandLine[256];
	
	strcpy(commandLine, "/System/Library/StartupItems/SecurityServer/enable ");
	strcat(commandLine, (securityOnOffSetting ? "no" : "yes"));
	
	status=system(commandLine);
    if ( status ) 
	{
        NSRunAlertPanel(@"Alert", @"Error executing the enable component.\nSecurity settings not changed.", @"OK", nil, nil, nil);
    }
			
			
	[[NSApplication sharedApplication] terminate:self];
		
}

- (void)securityOnOff:(id)sender
{
    switch ([sender selectedRow]) 
	{
		case 0: securityOnOffSetting = 0;			
			break;
		case 1: securityOnOffSetting = 1;			
			break;
	}
}





@end
