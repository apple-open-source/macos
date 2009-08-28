/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <RubyCocoa/RBObject.h>
#import <RubyCocoa/osx_ruby.h>

@interface RBObject(RBSlaveObject)
- initWithMasterObject: master;
- initWithClass: (Class)occlass masterObject: master;
- initWithRubyClass: (VALUE)rbclass masterObject: master;
@end

@interface RBObject(RBSlaveObjectPrivate)
- (void)trackRetainReleaseOfRubyObject;
- (void)releaseRubyObject;
- (void)retainRubyObject;
@end
