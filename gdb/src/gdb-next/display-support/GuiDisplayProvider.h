#include <stdio.h>

#include <Foundation/Foundation.h>

#include "DebuggerController_Protocol.h"
#include "ViewDisplayProvider_Protocol.h"
#include "GuiDisplayProvider_Protocol.h"

@interface TestViewDisplayProvider :
  NSObject <ViewDisplayProvider>
{
}
@end

@interface TestGuiDisplayProvider : 
  TestViewDisplayProvider <GuiDisplayProvider>
{
  NSPosixFileDescriptor *stdinPFD;
  id <DebuggerController> controller;
}

@end
