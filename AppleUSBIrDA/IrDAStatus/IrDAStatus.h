#import <Cocoa/Cocoa.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <IOKit/IOTypes.h>
#if MAC_OS_X_VERSION_10_5
#include <IOKit/iokitmig.h>
#else
#include <IOKit/iokitmig_c.h>
#endif
#include <IOKit/IOKitLib.h>

#include "IrDAStats.h"
#include "IrDAUserClient.h"

@interface IrDAStatusObj : NSObject
{
    IBOutlet id connectionSpeed;
    IBOutlet id connectionState;
    IBOutlet id crcErrors;
    IBOutlet id dataPacketsIn;
    IBOutlet id dataPacketsOut;
    IBOutlet id dropped;
    IBOutlet id iFrameRec;
    IBOutlet id iFrameSent;
    IBOutlet id ioErrors;
    IBOutlet id nickName;
    IBOutlet id protocolErrs;
    IBOutlet id recTimeout;
    IBOutlet id rejRec;
    IBOutlet id rejSent;
    IBOutlet id resent;
    IBOutlet id rnrRec;
    IBOutlet id rnrSent;
    IBOutlet id rrRec;
    IBOutlet id rrSent;
    IBOutlet id srejRec;
    IBOutlet id srejSent;
    IBOutlet id uFrameRec;
    IBOutlet id uFrameSent;
    IBOutlet id xmitTimeout;
	NSTimer			*timer;
	Boolean			state;
	IrDAStatus		oldStatus;
	io_connect_t	conObj;
}
kern_return_t doCommand(io_connect_t con, unsigned char commandID, void *inputData, unsigned long inputDataSize, void *outputData, size_t *outputDataSize);
io_object_t getInterfaceWithName(mach_port_t masterPort, const char *className);
kern_return_t openDevice(io_object_t obj, io_connect_t * con);
kern_return_t closeDevice(io_connect_t con);

- (NSString *) GetCurrentDriverName;
- (void) InvalidateOldStatus;
- (void) DumpResults:(IrDAStatus *)stats;
- (IBAction)StartTimer:(id)sender;
@end
