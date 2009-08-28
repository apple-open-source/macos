/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "osx_ruby.h"
#import <Foundation/Foundation.h>

VALUE ocdataconv_err_class(void);
VALUE oc_err_class(void);
VALUE ocmsgsend_err_class(void);

VALUE rb_err_new(VALUE klass, const char *fmt, ...);
VALUE oc_err_new(NSException* nsexcp);
