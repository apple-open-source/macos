/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#import <AppKit/NSTextView.h>
//#import <Carbon/CarbonPriv.h>
#import <Carbon/Carbon.h>

#import "MiniTerm.h"

#import "../../Controller/ppp_privmsg.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>          /* struct msghdr */
#include <sys/uio.h>	/* struct iovec */
#include <sys/un.h>
#include <sys/syslog.h>


@implementation PromptChat

enum {
    MATCH_NONE,
    MATCH_7E,
    MATCH_FF,
    MATCH_7D,
    MATCH_23,
    MATCH_03,
    MATCH_COMPLETE
};

NSFileHandle	*file_tty, *pppd_socket;

/* -----------------------------------------------------------------------------
 Receive a file descriptor from another process (a server).
 We have a 2-byte protocol for receiving the fd from send_fd().
----------------------------------------------------------------------------- */
int recv_fd(int servfd)
{
    struct cmsg {
        struct cmsghdr 	hdr;
        int		fd;
    } cmsg;
    int			newfd, nread, status;
    char		*ptr, buf[2]; /* send_fd()/recv_fd() 2-byte protocol */
    struct iovec	iov[1];
    struct msghdr	msg;

	newfd = -1;
    status = -1;
    for ( ; ; ) {
        iov[0].iov_base = buf;
        iov[0].iov_len = sizeof(buf);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_control = (caddr_t) &cmsg;
        msg.msg_controllen = sizeof(struct cmsg);

        nread = recvmsg(servfd, &msg, 0);
        if (nread == 0) {
            return -1;
        }
	/* See if this is the final data with null & status.
	Null must be next to last byte of buffer, status
	byte is last byte.  Zero status means there must
	be a file descriptor to receive. */
        for (ptr = buf; ptr < &buf[nread]; ) {
            if (*ptr++ == 0) {
                status = *ptr & 255;
                if (status == 0) {
                    newfd = cmsg.fd; /* new descriptor */
                } 
                else
                    newfd = -status;
                nread -= 2;
            }
        }
        if (status >= 0)        /* final data has arrived */
                return newfd;  /* descriptor, or -status */
    }
	return -1;
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (void)awakeFromNib {

    NSRange 	range;
    int 	err, ttyfd, sockfd;
    struct sockaddr_un	adr;

    // init vars
    fromline = 0;
    match = MATCH_NONE;
    
    // affect self as delegate to intercept input
    range.location = 0;
    range.length = 0;
    [text setSelectedRange: range];
    [text setDelegate: self];
    [[text window] makeFirstResponder:text];

    // bring the app to the front, window centered
    [NSApp activateIgnoringOtherApps:YES];
    [[text window] center];
    [[text window] makeKeyAndOrderFront:self];
    [[text window] setLevel:NSFloatingWindowLevel];

    // enable only roman keyboard
#if 0
    KeyScript(smKeyEnableRomanOnly);
#else
    TISInputSourceRef asciiInpSrc = TISCopyCurrentASCIICapableKeyboardInputSource();
    if (asciiInpSrc != NULL) {
        TISSelectInputSource( asciiInpSrc );
        CFRelease( asciiInpSrc );
    }
#endif /* __LP64__ */
    
    // contact pppd to get serial descriptor and exit code communication channel
    sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd < 0) {
        exit(0);	// should probably display an alert
    }

    bzero(&adr, sizeof(adr));
    adr.sun_family = AF_LOCAL;
    strlcpy(adr.sun_path, "/var/run/pppd-miniterm", sizeof(adr.sun_path));

    if ((err = connect(sockfd, (struct sockaddr *)&adr, sizeof(adr)) < 0)) {
        exit(0);	// should probably display an alert
    }
    
    ttyfd = recv_fd(sockfd);
    
    file_tty = [[NSFileHandle alloc] initWithFileDescriptor: ttyfd];

    // install notification and read asynchronously on file_tty
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(input:) 
        name:NSFileHandleReadCompletionNotification 
        object:file_tty];    
    
    [file_tty readInBackgroundAndNotify];    
    
    // there is nothing to read on pppd_socket fd
    // but we need to catch when pipe is closed
    pppd_socket = [[NSFileHandle alloc] initWithFileDescriptor: sockfd];
    [pppd_socket readInBackgroundAndNotify];    

}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (BOOL)textView:(NSTextView *)aTextView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString
{

    u_char 		c, *p;
    int 		i, len = [replacementString length];
    NSMutableData 	*data;
    
    // are we inserting the incoming char from the line ?
    // could be a critical section here... not sure about messaging system
    if (fromline) {
        return YES;
    }
    
    data = [NSMutableData alloc]; 
    if (data) {

        if (len == 0) {
            // send the delete char
            c = 8;
            [data initWithBytes: &c length: 1]; 
        }
        else {
            [data initWithData:[replacementString dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES]];
            // can the len change during conversion ?
            len = [data length];
            // replace 10 by 13 in the string
            p = [data mutableBytes];
            for (i = 0; i < len; i++)
                if (p[i] == 10)
                    p[i] = 13;
        }

        // write the data to the output file
        [file_tty writeData: data];
        [data release];
    }
    
    return NO;
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
#if 0
- (NSRange)textView:(NSTextView *)textView willChangeSelectionFromCharacterRange:(NSRange)oldSelectedCharRange toCharacterRange:(NSRange)newSelectedCharRange
{
    // Don't allow the selection to change
    return NSMakeRange([[textView string] length], 0);
}
#endif

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (void)display:(u_char *)data
    length:(u_int)length {
    
    NSString *str;
    
    if (length) {
    	str = (NSString *)CFStringCreateWithBytes(NULL, data, length, kCFStringEncodingASCII, NO);
        if (str) {
            fromline = 1;
            [text insertText:str];
            fromline = 0;
            [str release];
        }
    }
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (void)input:(NSNotification *)notification {

    NSData 	*data;
    u_char	*p, *p0;
    u_long	len;
            
    // move the selection to the end
    [text setSelectedRange: NSMakeRange([[text string] length], 0)];
    
    data = [[notification userInfo] objectForKey: NSFileHandleNotificationDataItem];
    
    p0 = p = (u_char *)[data bytes];
    len = [data length];

    if (len == 0) {
        // pipe has been closed (happens when pppd quits)
        exit(0);
    }
    
    while (len) {

        // look for ppp frame
        // match for 7E-FF-03
        // match for 7E-FF-7D-23
        switch (*p) {
            case 0x7e: match = MATCH_7E; break;
            case 0xff: match = (match == MATCH_7E) ? MATCH_FF : MATCH_NONE; break;
            case 0x7d: match = (match == MATCH_FF) ? MATCH_7D : MATCH_NONE; break;
            case 0x03: match = (match == MATCH_FF) ? MATCH_COMPLETE : MATCH_NONE; break;
            case 0x23: match = (match == MATCH_7D) ? MATCH_COMPLETE : MATCH_NONE; break;
            default: match = MATCH_NONE;
        }
    
        if (match == MATCH_COMPLETE) {
            // display what was valid before we exit
            [self display:p0 length:p - p0];
            // will quit app, successfully
            [self continuechat: self];
            return;
        }
        
        if (*p >= 128)
            *p -= 128;
        
        if ((*p >= 0x20)
            || (*p == 9)
            || (*p == 10)
            || (*p == 13)) {
                
            // valid bytes, they will be display later, in one chunck
        }
        else {
            // got a non printable byte, display what was valid so far
            [self display:p0 length:p - p0];
            p0 = p + 1;
            
            // check for delete char
            if (*p == 8) {
                if ([[text string] length] > 0)
                    [text replaceCharactersInRange: NSMakeRange([[text string] length] - 1, 1) 
                        withString:@""];
            }
        }
        
        p++;
        len--;
    }
    
    // display all the undisplayed bytes
    [self display:p0 length:p - p0];
    
    // post an other read
    [file_tty readInBackgroundAndNotify];    
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (IBAction)cancelchat:(id)sender
{
    u_char 	c = (unsigned char)cclErr_ScriptCancelled;
    NSData 	*data;
    
    data = [NSData dataWithBytes: &c length: 1]; 
    if (data) {
        [pppd_socket writeData: data];
        [data release];
    }
    
    // time to quit
    exit(0);
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (IBAction)continuechat:(id)sender
{
    u_char 	c = 0;
    NSData 	*data;
    
    data = [NSData dataWithBytes: &c length: 1]; 
    if (data) {
        [pppd_socket writeData: data];
        [data release];
    }
    
    // time to quit
    exit(0);
}

@end
