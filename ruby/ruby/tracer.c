#include "ruby.h"

#ifdef ENABLE_DTRACE
#include "dtrace.h"
#endif

VALUE rb_mDtrace;

static VALUE
ruby_dtrace_probe(int argc, VALUE *argv, unsigned check_only)
{
#ifdef ENABLE_DTRACE
  if (check_only) {
    return RUBY_RUBY_PROBE_ENABLED() ? Qtrue : Qfalse; 
  }
  else {
    VALUE name, data;
    char *probe_data;
 
    rb_scan_args(argc, argv, "11", &name, &data);
    probe_data = NIL_P(data) ? "" : StringValuePtr(data);

    RUBY_RUBY_PROBE(StringValuePtr(name), probe_data);
  }
#endif
  return Qnil;
}

static VALUE
ruby_dtrace_fire(int argc, VALUE *argv, VALUE klass)
{
  return ruby_dtrace_probe(argc, argv, 0);
}

static VALUE
ruby_dtrace_enabled(VALUE klass)
{
  return ruby_dtrace_probe(0, NULL, 1);
}

void Init_DTracer()
{
  rb_mDtrace = rb_define_module("DTracer");
  rb_define_module_function(rb_mDtrace, "fire", ruby_dtrace_fire, -1);
  rb_define_module_function(rb_mDtrace, "enabled?", ruby_dtrace_enabled, 0);
}

