#import <Foundation/Foundation.h>
#import "GdbManager.h"

@implementation GdbManager

- init
{
  [super init];
  displayProvider = nil;
  return self;
}

- (void) dealloc
{
  if (displayProvider != NULL) {
    [displayProvider release];
  }
  if (displayProviderConnectionName != NULL) {
    [displayProviderConnectionName release];
  }
  if (displayProviderHostName != NULL) {
    [displayProviderHostName release];
  }
  [super dealloc];
}

- (void) setDisplayProviderConnectionName: (const char *) connName
                                     host: (const char *) hostName
{
  if (connName) {
    displayProviderConnectionName = [[NSString alloc] initWithCString:connName];
  } else {
    displayProviderConnectionName = nil;
  }
  if (hostName) {
    displayProviderHostName = [[NSString alloc]	initWithCString:hostName];
  } else {
    displayProviderHostName = nil;
  }
}

- (NSString *) displayProviderConnectionName
{
  return displayProviderConnectionName;
}

- (NSString *) displayProviderHostName
{
  return displayProviderHostName;
}

- (void) setDisplayProvider:(id) dP
{
  displayProvider = [dP retain];
}


/* called from Gdb thread; returns nil if client doesn't support
   the requested protocol. */

- displayProviderForProtocol: (Protocol *) p
{
  if ([displayProvider conformsToProtocol:p]) {
    return displayProvider;
  } else {
    return nil;
  }
}

- (int) establishConnection
{
}

- (void) engageHookFunctions
{
}

- (void) disengageHookFunctions
{
}

- (void) doCommandLoop
{
}

- (void) processOutput: (NSString *) outputStr
            outputType: (GdbOutputType) t
{
}

- (const char  *) waitForLineOfInput
{
  return NULL;
}

@end
