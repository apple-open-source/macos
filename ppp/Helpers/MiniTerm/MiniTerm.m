#import <AppKit/NSTextView.h>

#import "MiniTerm.h"
#import "MTTerminalView.h"

#import <AppKit/NSComboBox.h>
#import <fcntl.h>
#import <sys/un.h>
#import <sys/socket.h>
#import <syslog.h>
#import <unistd.h>
#import "../../Controller/ppp_msg.h"
#import "../../Controller/ppp_privmsg.h"

extern u_long ppplink;
int csockfd;

@implementation PromptChat

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
void sys_send_confd(int sockfd, u_long code, u_char *data, u_long len, u_long *link, u_char expect_result)
{
    struct ppp_msg_hdr	msg;
    char 		*tmp;

    if (sockfd == -1)
        return;

    bzero(&msg, sizeof(msg));
    msg.m_type = code;
    msg.m_len = len;
    msg.m_link = *link;

    if (write(sockfd, &msg, sizeof(msg)) != sizeof(msg)) {
        return;
    }

    if (len && data && write(sockfd, data, len) != len) {
        return;
    }

    if (expect_result) {
       if (read(sockfd, &msg, sizeof(msg)) != sizeof(msg)) {
            return; //error
        }
        *link = msg.m_link;
        if (msg.m_len) {
            tmp = malloc(msg.m_len);
            if (tmp) {
                if (read(sockfd, tmp, msg.m_len) != msg.m_len) {
                    free(tmp);
                   return;//error
                }
                free(tmp);
            }
        }
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (void)awakeFromNib {

    NSRange 	range;
    u_long		len;
    struct sockaddr_un	adr;
    MTTerminalView* newTextView;
    
    // init vars
    fromline = 0;

    // Ensure that descriptors 0, 1, 2 are opened to /dev/null.

    if ((csockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        exit(0);
    }
    bzero(&adr, sizeof(adr));
    adr.sun_family = AF_LOCAL;
    strcpy(adr.sun_path, PPP_PATH);
    if (connect(csockfd,  (struct sockaddr *)&adr, sizeof(adr)) < 0) {
        exit(0);
    }
    len = 1500;	// buffer we want to use
    sys_send_confd(csockfd, PPP_OPENFD, (u_char *)&len, sizeof(len), &ppplink, 1);

    // affect self as delegate to intercept input
    range.location = 0;
    range.length = 0;
    // Create our own textview and center it inside the scrollview
    // I'm doing it via code so we don't have to change the nib and involve
    // the localization folks.
    newTextView = [[[MTTerminalView alloc] init] autorelease];
    [(NSScrollView*)[[text superview] superview] setDocumentView: newTextView];
    
    [newTextView setFrame: [text frame]];
    text = newTextView;
    [text setSelectedRange: range];
    [text setDelegate: self];
    [[text window] makeFirstResponder:text];

    // detach a thread for reading
    [NSThread detachNewThreadSelector:@selector(input) toTarget:self withObject:nil];

}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (BOOL)textView:(NSTextView *)aTextView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString
{
    // are we inserting the incoming char from the line ?
    // could be a critical section here... mot sure about messaging system
    if (fromline) 
        return YES;

   if ([replacementString length] == 1) {
     //   just do it 1 byte at a time for now
    // otherwise we are intercepting user input... write it to the stdout
        u_char c = [replacementString cString][0];
        if (c == 10)
            c = 13;
        sys_send_confd(csockfd, PPP_WRITEFD, &c, 1, &ppplink, 0);
    }
//    write(1, [replacementString cString], [replacementString length]);
    return NO;
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */

- (void)passKeyToSocket:(u_char)character
{
    if (csockfd >= 0)
        sys_send_confd(csockfd, PPP_WRITEFD, &character, 1, &ppplink, 0);
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (NSRange)textView:(NSTextView *)textView willChangeSelectionFromCharacterRange:(NSRange)oldSelectedCharRange toCharacterRange:(NSRange)newSelectedCharRange
{
    // Don't allow the selection to change
    return NSMakeRange([[textView string] length], 0);
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (void)input {

    NSAutoreleasePool 	*pool = [[NSAutoreleasePool alloc] init];
    u_char 		str[2];
    int 		status;
    struct ppp_msg_hdr	msg;
    u_long 		lval;
    
    str[1] = 0;

    for (;;) {

        lval = 1;
        sys_send_confd(csockfd, PPP_READFD, (char*)&lval, sizeof(lval), &ppplink, 0);

        status = read(csockfd, &msg, sizeof(msg));
        if (status == sizeof(msg)) {
            status = read(csockfd, &str[0], 1);
        }
         // read stdin
       //read(0, &str[0], 1);

        // look for ppp frame
        if (str[0] == 0x7e) {
            break;
        }
        
        
        fromline = 1;
        if (str[0] >= 128)
            str[0] -= 128;
        if (str[0] == 8)
        {
            NSString* string = [text string];
            if ([string length] > 0)
                string = [string substringWithRange: NSMakeRange(0, [string length] - 1)];
            [text setString:string];
        }
        else if ((str[0] >= 0x20)
            || (str[0] == 9)
            || (str[0] == 10)
            || (str[0] == 13)) {
            [text insertText:[[[NSString alloc] initWithCString:str] autorelease]];
            }
        fromline = 0;
    }

    lval = cclErr_ExitOK;
    sys_send_confd(csockfd, PPP_CCLRESULT, (u_char *)&lval, sizeof(lval), &ppplink, 0);
    [text setSelectedRange: NSMakeRange ([[text string] length],0)];
    [text scrollRangeToVisible:[text selectedRange]];
    [pool release];
    exit(0);
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (IBAction)cancelchat:(id)sender
{
    u_long 	m = cclErr_ScriptCancelled;

    sys_send_confd(csockfd, PPP_CCLRESULT, (u_char *)&m, sizeof(m), &ppplink, 0);
    exit(0);
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (IBAction)continuechat:(id)sender
{
    u_long 	m = cclErr_ExitOK;

    sys_send_confd(csockfd, PPP_CCLRESULT, (u_char *)&m, sizeof(m), &ppplink, 0);
    exit(0);
}

@end
