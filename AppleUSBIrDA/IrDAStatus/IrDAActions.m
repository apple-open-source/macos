#import "IrDAActions.h"
#import "IrDAStatus.h"
#import <sys/stat.h>
#import "Preferences.h"

@implementation IrDAActions
// IsCheetahNetworkPrefs
- (NSString *) GetCurrentDriverName{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	return ([defaults objectForKey: DefaultDriverKey]);
}
- (IBAction)StartIrDA:(id)sender
{
    kern_return_t	kr;
    io_object_t		netif;
    io_connect_t	conObj;
    mach_port_t		masterPort;
	NSString		*driverName = [self GetCurrentDriverName];
	const char		*driverCStringName = [driverName cString];

	NSLog(@"StartIrDA");
    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
		printf("IOMasterPort() failed: %08lx\n", (unsigned long)kr);
		return;
    }
    netif = getInterfaceWithName(masterPort, driverCStringName);
    if (netif) {
		kr = openDevice(netif, &conObj);
		if (kr == kIOReturnSuccess) {
			size_t infosize = 0;

			kr = doCommand(conObj, kIrDAUserCmd_Enable, nil, 0, nil, &infosize);
			if (kr == kIOReturnSuccess) {
				NSLog(@"StartIrDA: kIrDAUserCmd_Enable worked!");
			}
			else{
				printf("kr is %d\n", kr);
			}
			closeDevice(conObj);
      	}
		IOObjectRelease(netif);
    }
	else{
		NSLog(@"Unable to find the Requested Driver");
	}
}
- (IBAction)StopIrDA:(id)sender
{
    kern_return_t	kr;
    io_object_t		netif;
    io_connect_t	conObj;
    mach_port_t		masterPort;
	NSString		*driverName = [self GetCurrentDriverName];
	const char		*driverCStringName = [driverName cString];

	NSLog(@"StopIrDA");
    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
		printf("IOMasterPort() failed: %08lx\n", (unsigned long)kr);
		return;
    }
    netif = getInterfaceWithName(masterPort, driverCStringName);
    if (netif) {
		kr = openDevice(netif, &conObj);
		if (kr == kIOReturnSuccess) {
			size_t infosize = 0;

			kr = doCommand(conObj, kIrDAUserCmd_Disable, nil, 0, nil, &infosize);
			if (kr == kIOReturnSuccess) {
				NSLog(@"StopIrDA: kIrDAUserCmd_Disable worked!");
			}
			else{
				printf("kr is %d\n", kr);
			}
			closeDevice(conObj);
      	}
		IOObjectRelease(netif);
    }
	else{
		NSLog(@"Unable to find the Requested Driver");
	}
}

@end
