/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *
 *	$Id: Node.h,v 1.4 2003/08/20 19:41:46 nano Exp $
 *
 *	$Log: Node.h,v $
 *	Revision 1.4  2003/08/20 19:41:46  nano
 *	
 *	Bug #:
 *	New version's of Nima's USB Prober (2.2b17)
 *	3382540  Panther: Ejecting a USB CardBus card can freeze a machine
 *	3358482  Device Busy message with Modems and IOUSBFamily 201.2.14 after sleep
 *	3385948  Need to implement device recovery on High Speed Transaction errors to full speed devices
 *	3377037  USB EHCI: returnTransactions can cause unstable queue if transactions are aborted
 *	
 *	Also, updated most files to use the id/log functions of cvs
 *	
 *	Submitted by: nano
 *	Reviewed by: rhoads/barryt/nano
 *	
 *	Revision 1.3  2003/08/18 20:25:29  nano
 *	Added id/log
 *	
 */

#import <Foundation/Foundation.h>

@interface Node : NSObject {
    NSMutableArray *children;
    NSString *itemName;
    NSString *itemValue;
}

- (id)init;

// Accessor methods for the strings
- (NSString *)itemName;
- (NSString *)itemValue;
- (void)setItemName:(NSString *)s;
- (void)setItemValue:(NSString *)s;

// Accessors for the children
- (void)addChild:(Node *)n;
- (int)childrenCount;
- (Node *)childAtIndex:(UInt32)i;

// Other properties
- (BOOL)expandable;
- (NSString *)stringRepresentationWithInitialIndent:(int)startingLevel recurse:(BOOL)recurse;

- (void)clearNode;

- (void)dealloc;
@end
