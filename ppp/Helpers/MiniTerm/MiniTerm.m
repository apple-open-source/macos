/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#import <AppKit/NSTextView.h>

#import "MiniTerm.h"

#import "../../Controller/ppp_privmsg.h"


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

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (void)awakeFromNib {

    NSRange 	range;

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

    // install notification and read asynchronously on stdin
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(input:) 
        name:NSFileHandleReadCompletionNotification 
        object:nil];    
    
    [[NSFileHandle fileHandleWithStandardInput] readInBackgroundAndNotify];
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (BOOL)textView:(NSTextView *)aTextView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString
{

    u_char 		c, *p;
    int 		i, len = [replacementString length];
    NSFileHandle	*file = [NSFileHandle fileHandleWithStandardOutput];
    NSMutableData 	*data;
    
    // are we inserting the incoming char from the line ?
    // could be a critical section here... not sure about messaging system
    if (fromline) 
        return YES;
    
    data = [NSMutableData alloc]; 
    if (data) {

        if (len == 0) {
            // send the delete char
            c = 8;
            [data initWithBytes: &c length: 1]; 
        }
        else {
            [data initWithBytes: [replacementString cString] length: len];     
            // replace 10 by 13 in the string
            c = 13;
            p = [data mutableBytes];
            for (i = 0; i < len; i++)
                if (p[i] == 10)
                    p[i] = 13;
        }

        // write the data to stdout
        [file writeData: data];
        [data release];
    }
    
    return NO;
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
- (void)display:(u_char *)data
    length:(u_int)length {
    
    NSString *str;
    
    if (length) {
    	str = [[NSString alloc] initWithCString:data length:length];
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
    [[NSFileHandle fileHandleWithStandardInput] readInBackgroundAndNotify];
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (IBAction)cancelchat:(id)sender
{

    exit(cclErr_ScriptCancelled);
}

/* ------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------ */
- (IBAction)continuechat:(id)sender
{

    exit(0);
}

@end
