#import <Cocoa/Cocoa.h>

@interface PromptChat : NSObject
{
    IBOutlet id text;

    int 	fromline;
    int 	cancelled;

}
- (BOOL)textView:(NSTextView *)aTextView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString;

- (void)input;
- (IBAction)cancelchat:(id)sender;
- (IBAction)continuechat:(id)sender;

- (void)passKeyToSocket:(u_char)character;
@end
