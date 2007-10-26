/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "osx_ruby.h"

/** class methods **/
VALUE objcptr_s_class ();
VALUE objcptr_s_new_with_cptr (void* cptr, const char *encoding);

/** instance methods **/
void* objcptr_cptr (VALUE rcv);
long objcptr_allocated_size(VALUE rcv);

/** initial loading **/
VALUE init_cls_ObjcPtr (VALUE outer);
