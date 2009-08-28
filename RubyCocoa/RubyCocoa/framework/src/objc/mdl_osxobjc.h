/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <objc/objc.h>
#import "osx_ruby.h"
#import "cls_objcid.h"
#import "cls_objcptr.h"
#import "mdl_objwrapper.h"

#if __LP64__
# define OCID2NUM(val) ULONG2NUM((unsigned long)(val))
# define NUM2OCID(val) ((id)NUM2ULONG((VALUE)(val)))
#else
# define OCID2NUM(val) UINT2NUM((unsigned int)(val))
# define NUM2OCID(val) ((id)NUM2UINT((VALUE)(val)))
#endif

/** OSX module **/
VALUE osx_s_module();

/** OCObject methods **/
VALUE ocobj_s_new_with_class_name(id ocid, const char *cls_name);
VALUE ocobj_s_new (id ocid);

/** getter **/
id    rbobj_get_ocid (VALUE rcv);
VALUE ocid_get_rbobj (id ocid);

/** misc **/
VALUE rb_osx_class_const (const char* name);
VALUE rb_cls_ocobj (const char* name);
VALUE ocobj_s_class (void);

/** initialize **/
void initialize_mdl_osxobjc();
