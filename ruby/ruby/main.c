/**********************************************************************

  main.c -

  $Author: ko1 $
  created at: Fri Aug 19 13:19:58 JST 1994

  Copyright (C) 1993-2007 Yukihiro Matsumoto

**********************************************************************/

#undef RUBY_EXPORT
#include "ruby.h"
#include "vm_debug.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef RUBY_DEBUG_ENV
#include <stdlib.h>
#endif

#ifdef __APPLE__
#include <msgtracer_client.h>
#include <msgtracer_keys.h>

void
mt_log_BSDServices_ScriptingLanguageUse(const char *signature)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", "com.apple.BSDServices.ScriptingLanguageUse" );
    asl_set(m, "com.apple.message.signature", signature);
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_free(m);
}
#endif

int
main(int argc, char **argv)
{
#ifdef RUBY_DEBUG_ENV
    ruby_set_debug_option(getenv("RUBY_DEBUG"));
#endif
#ifdef HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif

#ifdef __APPLE__
    mt_log_BSDServices_ScriptingLanguageUse("ruby");
#endif

    ruby_sysinit(&argc, &argv);
    {
	RUBY_INIT_STACK;
	ruby_init();
	return ruby_run_node(ruby_options(argc, argv));
    }
}
