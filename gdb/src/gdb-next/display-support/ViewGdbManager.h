#import "GdbManager.h"
#import "DisplayMisc.h"
#import "ViewDisplayProvider_Protocol.h"
#import <Foundation/Foundation.h>

//
// A view display provider is pretty simple: we just send
// it one-way messages telling it the current line.
//
// The ViewGdbManager makes the connection with the view
// provider.
//
@interface ViewGdbManager : GdbManager
{
}

- (void) dealloc;

- (int) establishConnection;


@end
