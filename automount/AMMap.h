/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef __MAP_H__
#define __MAP_H__

#import "RRObject.h"
#import "Array.h"

@class Vnode;
@class String;

@interface Map : RRObject
{
	String *mountPoint;
	String *name;
	Vnode *root;
}

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir;
- (unsigned int)didAutoMount;

- (void)setName:(String *)n;
- (String *)name;

- (Vnode *)root;
- (BOOL)checkVnodePath:(String *)path from:(Vnode *)v;
- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v;
- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v withType:(String *)type;
- (Vnode *)mkdir:(String *)s attributes:(void *)x atVnode:(Vnode *)v;
- (Vnode *)symlink:(String *)l name:(String *)s atVnode:(Vnode *)v;

- (unsigned int)mount:(Vnode *)v;
- (unsigned int)mount:(Vnode *)v withUid:(int)uid;
- (unsigned int)unmount:(Vnode *)v;

- (String *)mountPoint;
- (int)mountArgs;
- (void)timeout;
- (void)reInit;
- (BOOL)acceptOptions:(Array *)opts;

@end

#endif __MAP_H__
