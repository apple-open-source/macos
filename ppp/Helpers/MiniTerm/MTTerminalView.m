#import "MTTerminalView.h"
#import "MiniTerm.h"

@implementation MTTerminalView

- (void)keyDown:(NSEvent *)theEvent
{
    unichar key = [[theEvent characters] characterAtIndex:0];

    [super keyDown: theEvent];

    if (key == NSDeleteCharacter)
    {
	u_char deleteKey = 8;
        [ (PromptChat*)[self delegate] passKeyToSocket: deleteKey];
    }
}


@end
