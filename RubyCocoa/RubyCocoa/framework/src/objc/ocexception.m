/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "ocexception.h"
#import "ocdata_conv.h"
#import "mdl_osxobjc.h"

static VALUE
_oc_exception_class(const char* name)
{
  VALUE mosx = rb_const_get(rb_cObject, rb_intern("OSX"));;
  return rb_const_get(mosx, rb_intern(name));
}

VALUE
ocdataconv_err_class(void)
{
  static VALUE exc = Qnil;
  if (NIL_P(exc))
    exc = _oc_exception_class("OCDataConvException");
  return exc;
}

VALUE
oc_err_class(void)
{
  static VALUE exc = Qnil;
  if (NIL_P(exc))
    exc = _oc_exception_class("OCException");
  return exc;
}

VALUE
ocmsgsend_err_class(void)
{
  static VALUE exc = Qnil;
  if (NIL_P(exc))
    exc = _oc_exception_class("OCMessageSendException");
  return exc;
}

VALUE
rb_err_new(VALUE klass, const char *fmt, ...)
{
  va_list args;
  VALUE ret;
  char buf[BUFSIZ];

  va_start(args, fmt);
  vsnprintf(buf, sizeof buf, fmt, args);
  ret = rb_exc_new2(klass, buf);
  va_end(args);
  return ret;
}

VALUE
oc_err_new (NSException* nsexcp)
{
  id pool;
  char buf[BUFSIZ];

  if ([[nsexcp name] hasPrefix: @"RBException_"]) {
    // This is a wrapped Ruby exception
    id rberr = [[nsexcp userInfo] objectForKey: @"$!"];
    if (rberr) {
      VALUE err = ocid_get_rbobj(rberr);
      if (err != Qnil)
        return err;
    }
  }
  
  pool = [[NSAutoreleasePool alloc] init];
  snprintf(buf, BUFSIZ, "%s - %s",
	   [[nsexcp name] UTF8String], [[nsexcp reason] UTF8String]);
  [pool release];
  return rb_funcall(oc_err_class(), rb_intern("new"), 2, ocid_to_rbobj(Qnil, nsexcp), rb_str_new2(buf));
}
