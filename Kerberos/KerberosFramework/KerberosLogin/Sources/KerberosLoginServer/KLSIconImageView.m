/*
 * KLIconImageView.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KLSIconImageView.m,v 1.2 2003/07/16 16:13:15 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#import "KLSIconImageView.h"

@implementation KLSIconImageView

- (id) initWithFrame: (NSRect) frameRect
{
    kerberosIconImage = NULL;
    badgeIconImage = NULL;

    if (self = [super initWithFrame: frameRect]) {
        NSString *iconPathString = [[NSBundle mainBundle] pathForResource: @"KerberosLogin"
                                                                   ofType: @"icns"];
        if (iconPathString != NULL) {
            kerberosIconImage = [[NSImage alloc] initWithContentsOfFile: iconPathString];
        }
    }
    return self;
}

- (void) dealloc
{
    if (kerberosIconImage != NULL) { [kerberosIconImage release]; }
    if (badgeIconImage    != NULL) { [badgeIconImage release]; }    
}

- (void) setBadgeIconImage: (NSImage *) image
{
    if (image          != NULL) { [image retain]; }
    if (badgeIconImage != NULL) { [badgeIconImage release]; }

    badgeIconImage = image;
    [self setNeedsDisplay: YES];
}

- (void) drawRect: (NSRect) rect
{
    float frameSize = [self frame].size.width;
    float iconSize = 64.0;
    float badgeSize = 32.0;
    
    if (kerberosIconImage != NULL) {
        [self lockFocus];

        // Clear an old badged icon
        [[NSColor windowBackgroundColor] set];
        NSRectFill ([self frame]);

        [kerberosIconImage setScalesWhenResized: YES];
        [kerberosIconImage setSize: NSMakeSize (iconSize, iconSize)];
        [kerberosIconImage compositeToPoint: NSMakePoint (0.0, frameSize - iconSize)
                                  operation: NSCompositeSourceOver];

        if (badgeIconImage != NULL) {
            // draw badged icon
            [badgeIconImage setScalesWhenResized: YES];
            [badgeIconImage setSize: NSMakeSize (badgeSize, badgeSize)];
            //[badgeIconImage dissolveToPoint: NSMakePoint (39.0, 0.0) fraction: 0.8];
            [badgeIconImage compositeToPoint: NSMakePoint (frameSize - badgeSize, 0.0)
                                   operation: NSCompositeSourceOver];
        }
        [self unlockFocus];
    }    
}

@end
