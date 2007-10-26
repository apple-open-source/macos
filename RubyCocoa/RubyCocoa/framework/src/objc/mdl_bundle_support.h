/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#ifndef _MDL_BUNDLE_SUPPORT_H_
#define  _MDL_BUNDLE_SUPPORT_H_

#import <objc/objc.h>
#import "osx_ruby.h"

void initialize_mdl_bundle_support();

typedef VALUE (* bundle_support_program_loader_t)(const char*, Class, id);

VALUE load_ruby_program_for_class(const char* path,    Class objc_class, id additional_param);
VALUE eval_ruby_program_for_class(const char* program, Class objc_class, id additional_param);

#endif  //  _MDL_BUNDLE_SUPPORT_H_
