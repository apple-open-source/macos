/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <string.h>
#include <mach-o/dyld.h>
#include <objc/objc-runtime.h>
#include <dlfcn.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSString.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSThread.h>
#import <assert.h>

#import "RBRuntime.h"
#import "RBObject.h"
#import "mdl_osxobjc.h"
#import "mdl_bundle_support.h"
#import "ocdata_conv.h"
#import "OverrideMixin.h"
#import "internal_macros.h"
#import "objc_compat.h"
#import "st.h"

#define BRIDGE_SUPPORT_NAME "BridgeSupport"

/* we cannot use the original DLOG here because it uses autoreleased objects */
#undef DLOG
#define DLOG(f,args...) do { if (rubycocoa_debug == Qtrue) printf(f, args); } while (0) 

/* this function should be called from inside a NSAutoreleasePool */
static NSBundle* bundle_for(Class klass)
{
  return (klass == nil) ?
    [NSBundle mainBundle] : 
    [NSBundle bundleForClass: klass];
}

static char* resource_item_path_for(const char* item_name, Class klass)
{
  char* result;
  POOL_DO(pool) {
    NSBundle* bundle = bundle_for(klass);
    NSString* path = [bundle resourcePath];
    path = [path stringByAppendingFormat: @"/%@",
                 [NSString stringWithUTF8String: item_name]];
    if (path == NULL) {
      NSLog(@"ERROR: Cannot locate the bundle resource `%s' - aborting.", item_name);
      exit(1);
    }
    result = strdup([path fileSystemRepresentation]);
  } END_POOL(pool);
  return result;
}

static char* resource_item_path(const char* item_name)
{
  return resource_item_path_for(item_name, nil);
}

static char* rb_main_path(const char* main_name)
{
  return resource_item_path(main_name);
}

static char* resource_path_for(Class klass)
{
  char* result;
  POOL_DO(pool) {
    NSBundle* bundle = bundle_for(klass);
    NSString* path = [bundle resourcePath];
    result = strdup([path fileSystemRepresentation]);
  } END_POOL(pool);
  return result;
}

static char* resource_path()
{
  return resource_path_for(nil);
}

static char* bridge_support_path_for(Class klass)
{
  return resource_item_path_for(BRIDGE_SUPPORT_NAME, klass);
}

static char* bridge_support_path()
{
  return bridge_support_path_for(nil);
}

static char* private_frameworks_path_for(Class klass)
{
  char* result;
  POOL_DO(pool) {
    NSBundle* bundle = bundle_for(klass);
    NSString* path = [bundle privateFrameworksPath];
    result = strdup([path fileSystemRepresentation]);
  } END_POOL(pool);
  return result;
}

static char* private_frameworks_path() { 
  return private_frameworks_path_for(nil);
}

static char* shared_frameworks_path_for(Class klass)
{
  char* result;
  POOL_DO(pool) {
    NSBundle* bundle = bundle_for(klass);
    NSString* path = [bundle sharedFrameworksPath];
    result = strdup([path fileSystemRepresentation]);
  } END_POOL(pool);
  return result;
}

static char* shared_frameworks_path() { 
  return shared_frameworks_path_for(nil);
}

char* framework_resources_path()
{
  return resource_path_for([RBObject class]);
}

static char* framework_ruby_path()
{
  return resource_item_path_for("ruby", [RBObject class]);
}

static char* framework_bridge_support_path()
{
  return bridge_support_path_for([RBObject class]);
}

static void load_path_unshift(char* path)
{
  extern VALUE rb_load_path;
  VALUE rpath = rb_str_new2(path);
  free(path);

  if (! RTEST(rb_ary_includes(rb_load_path, rpath)))
    rb_ary_unshift(rb_load_path, rpath);
}

static void sign_path_unshift(char* path)
{
  VALUE sign_paths;
  VALUE rpath;

  sign_paths = rb_const_get(osx_s_module(), rb_intern("RUBYCOCOA_SIGN_PATHS"));
  rpath = rb_str_new2(path);
  free(path);
  if (! RTEST(rb_ary_includes(sign_paths, rpath)))
    rb_ary_unshift(sign_paths, rpath);
}

static void framework_paths_unshift(char* path)
{
  VALUE frameworks_paths;
  VALUE rpath;

  frameworks_paths = rb_const_get(osx_s_module(), rb_intern("RUBYCOCOA_FRAMEWORK_PATHS"));
  rpath = rb_str_new2(path);
  free(path);
  if (! RTEST(rb_ary_includes(frameworks_paths, rpath)))
    rb_ary_unshift(frameworks_paths, rpath);
}

static int
prepare_argv(int argc, const char* argv[], const char* rb_main_name, const char*** ruby_argv_ptr)
{
  int i;
  int ruby_argc;
  const char** ruby_argv;
  int my_argc;
  char* my_argv[] = {
    rb_main_path(rb_main_name)
  };

  my_argc = sizeof(my_argv) / sizeof(char*);

  ruby_argc = 0;
  ruby_argv = malloc (sizeof(char*) * (argc + my_argc + 1));
  for (i = 0; i < argc; i++) {
    if (strncmp(argv[i], "-psn_", 5) == 0) continue;
    ruby_argv[ruby_argc++] = argv[i];
  }
  for (i = 0; i < my_argc; i++) ruby_argv[ruby_argc++] = my_argv[i];
  ruby_argv[ruby_argc] = NULL;

  *ruby_argv_ptr = ruby_argv;
  return ruby_argc;
}

/* flag for calling Init_stack frequently */
static int frequently_init_stack_mode = 0;

void rubycocoa_set_frequently_init_stack(int val)
{
  frequently_init_stack_mode = (val ? 1 : 0);
}

int rubycocoa_frequently_init_stack()
{
  return frequently_init_stack_mode;
}

int RBNotifyException(const char* title, VALUE err)
{
  VALUE ary,str;
  VALUE printf_args[2];
  int i;

  if (! RTEST(rb_obj_is_kind_of(err, rb_eException))) return 0;
  if (! RUBYCOCOA_SUPPRESS_EXCEPTION_LOGGING_P) {
    NSLog(@"%s: %s: %s",
          title,
          STR2CSTR(rb_obj_as_string(rb_obj_class(err))),
          STR2CSTR(rb_obj_as_string(err)));
    ary = rb_funcall(err, rb_intern("backtrace"), 0);
    if (!NIL_P(ary)) {
      for (i = 0; i < RARRAY(ary)->len; i++) {
        printf_args[0] = rb_str_new2("\t%s\n");
        printf_args[1] = rb_ary_entry(ary, i);
        str = rb_f_sprintf(2, printf_args);
        rb_write_error(STR2CSTR(str));
      }
    }
  }
  return 1;
}

static int notify_if_error(const char* api_name, VALUE err)
{
  return RBNotifyException(api_name, err);
}

@implementation NSObject (__DeallocHook)

- (void) __dealloc
{
}

- (void) __clearCacheAndDealloc
{
  remove_from_oc2rb_cache(self);
  [self __dealloc];
}

@end

static void install_dealloc_hook()
{
  Method dealloc_method, aliased_dealloc_method, cache_aware_dealloc_method;
  Class nsobject;

  nsobject = [NSObject class];

  dealloc_method = class_getInstanceMethod(nsobject, @selector(dealloc));
  aliased_dealloc_method = class_getInstanceMethod(nsobject, 
    @selector(__dealloc));
  cache_aware_dealloc_method = class_getInstanceMethod(nsobject,
    @selector(__clearCacheAndDealloc));

  method_setImplementation(aliased_dealloc_method,
    method_getImplementation(dealloc_method));
  method_setImplementation(dealloc_method,
    method_getImplementation(cache_aware_dealloc_method));
}


static int rubycocoa_initialized_flag = 0;

static int rubycocoa_initialized_p()
{
  return rubycocoa_initialized_flag;
}

// exported and used by internal_macros.h 
VALUE rubycocoa_debug = Qfalse;

static BOOL rb_cocoa_check_for_multiple_libruby(void);
static void RBCocoaInstallRubyThreadSchedulerHooks(void);

static void rubycocoa_init()
{
  if (! rubycocoa_initialized_flag) {
    // initialize the threading hooks
    rb_cocoa_check_for_multiple_libruby();
    RBCocoaInstallRubyThreadSchedulerHooks();

    init_rb2oc_cache();    // initialize the Ruby->ObjC internal cache
    init_oc2rb_cache();    // initialize the ObjC->Ruby internal cache
    install_dealloc_hook();
    initialize_mdl_osxobjc();  // initialize an objc part of rubycocoa
    initialize_mdl_bundle_support();
    init_ovmix();
    load_path_unshift(framework_ruby_path()); // PATH_TO_FRAMEWORK/Resources/ruby
    sign_path_unshift(framework_bridge_support_path());
    rubycocoa_initialized_flag = 1;
    rb_define_variable("$RUBYCOCOA_DEBUG", &ruby_debug);
  }
}

static VALUE
rubycocoa_bundle_init(const char* program, 
                      bundle_support_program_loader_t loader,
                      Class klass, id param)
{
  extern void Init_stack(VALUE*);
  int state;
  Init_stack((void*)&state);
  if (! rubycocoa_initialized_p()) {
    ruby_init();
    ruby_init_loadpath();
    rubycocoa_init();
    rubycocoa_set_frequently_init_stack(1);
  }
  load_path_unshift(resource_path_for(klass));
  sign_path_unshift(bridge_support_path_for(klass));
  framework_paths_unshift(private_frameworks_path_for(klass));
  framework_paths_unshift(shared_frameworks_path_for(klass));
  return loader(program, klass, param);
}

static VALUE
rubycocoa_app_init(const char* program, 
                   bundle_support_program_loader_t loader,
                   int argc, const char* argv[], id param)
{
  extern void Init_stack(VALUE*);
  int state;
  Init_stack((void*)&state);
  if (! rubycocoa_initialized_p()) {
    ruby_init();
    ruby_script(argv[0]);
    ruby_set_argv(argc - 1, (char**)(argv+1));
    ruby_init_loadpath();
    rubycocoa_init();
    rubycocoa_set_frequently_init_stack(0);
  }
  load_path_unshift(resource_path());
  sign_path_unshift(bridge_support_path());
  framework_paths_unshift(private_frameworks_path());
  framework_paths_unshift(shared_frameworks_path());
  return loader(program, nil, param);
}


/** [API] RBBundleInit
 *
 * initialize ruby and rubycocoa for a bundle
 * return not 0 when something error.
 */
int
RBBundleInit(const char* path_to_ruby_program, Class klass, id param)
{
  VALUE result;
  result = rubycocoa_bundle_init(path_to_ruby_program, 
                                 load_ruby_program_for_class, 
                                 klass, param);
  return notify_if_error("RBBundleInit", result);
}

int
RBBundleInitWithSource(const char* ruby_program, Class klass, id param)
{
  VALUE result;
  result = rubycocoa_bundle_init(ruby_program, 
                                 eval_ruby_program_for_class,
                                 klass, param);
  return notify_if_error("RBBundleInitWithSource", result);
}


/** [API] RBApplicationInit
 *
 * initialize ruby and rubycocoa for a command/application
 * return 0 when complete, or return not 0 when error.
 */
int
RBApplicationInit(const char* path_to_ruby_program, int argc, const char* argv[], id param)
{
  VALUE result;
  result = rubycocoa_app_init(path_to_ruby_program,
                              load_ruby_program_for_class,
                              argc, argv, param);
  return notify_if_error("RBApplicationInit", result);
}

int
RBApplicationInitWithSource(const char* ruby_program, int argc, const char* argv[], id param)
{
  VALUE result;
  result = rubycocoa_app_init(ruby_program, 
                              eval_ruby_program_for_class,
                              argc, argv, param);
  return notify_if_error("RBApplicationInitWithSource", result);
}

/** [API] initialize rubycocoa for a ruby extention library **/
void
RBRubyCocoaInit()
{
  rubycocoa_init();
}

/** [API] launch rubycocoa application (api for compatibility) **/
int
RBApplicationMain(const char* rb_program_path, int argc, const char* argv[])
{
  int ruby_argc;
  const char** ruby_argv;

  if (! rubycocoa_initialized_p()) {
    ruby_init();
    ruby_argc = prepare_argv(argc, argv, rb_program_path, &ruby_argv);
    ruby_options(ruby_argc, (char**) ruby_argv);
    rubycocoa_init();
    load_path_unshift(resource_path()); // PATH_TO_BUNDLE/Contents/resources
    sign_path_unshift(bridge_support_path());
    framework_paths_unshift(private_frameworks_path());
    framework_paths_unshift(shared_frameworks_path());
    ruby_run();
  }
  return 0;
}

/******************************************************************************/

/* Ruby Thread support:
  
   Ruby implements threads by using setjmp/longjmp to switch between separate 
   C stacks within one native thread.

   This confuses Objective C because NSThread stores a per-native-thread stack 
   of autorelease pools and exception handlers. When the C stack changes, an 
   error message like this is likely to appear:

      Exception handlers were not properly removed. Some code has jumped or 
      returned out of an NS_DURING...NS_HANDLER region without using the 
      NS_VOIDRETURN or NS_VALUERETURN macros
 
   The semantics of NSAutoreleasePools are also broken because the effective 
   scope crosses multiple threads.
   
   The solution below requires a patch to the Ruby interpreter that implements 
   the following function:

      void rb_add_threadswitch_hook(rb_threadswitch_hook_func_t func)

   A hook is registered that multiplexes sets of autorelease pools and 
   exception stacks onto one NSThread.
   Whenever a Ruby thread is switched in or out, the relevant context is saved 
   and restored. 
 
   Unfortunately, implementing this requires fiddling with two fields in the 
   NSThread internal structure.

   A further complication arises due to the dynamic linker. Suppose RubyCocoa 
   has been linked against a patched libruby, but at runtime the user 
   accidentally uses /usr/bin/ruby (unpatched) to load RubyCocoa. Since
   /usr/bin/ruby links to /usr/lib/libruby.dylib, and RubyCocoa links to 
   /path/to/patched/libruby.dylib, most of the functions will be picked up 
   from /usr/lib/libruby.dylib, but rb_add_threadswitch_hook will be linked 
   from the patched libruby.dylib.

   The setup code explicitly determines the name of the libraries from which 
   two relevant symbols have been loaded from, and issues a warning and aborts 
   the setup process if they are different.

   Tested on 10.4.7 PowerPC.
*/

static int rb_cocoa_thread_debug = 1;

#ifndef RUBY_THREADSWITCH_INIT
/* The declarations immediately below come from the patched ruby.h.
   Since they are not available here (determined by the lack of definition for 
   RUBY_THREADSWITCH_INIT) we define them manually, so that a patched 
   libruby.dylib can still be used at runtime.
*/

typedef unsigned int rb_threadswitch_event_t;

#define RUBY_THREADSWITCH_INIT 0x01
#define RUBY_THREADSWITCH_FREE 0x02
#define RUBY_THREADSWITCH_SAVE 0x04
#define RUBY_THREADSWITCH_RESTORE 0x08

typedef void (*rb_threadswitch_hook_func_t) _((rb_threadswitch_event_t,VALUE));
#endif

/* The following two functions are marked as weak imports so that RubyCocoa 
   will still load without thread switching hooks support in the ruby 
   interpreter.
*/
extern void *rb_add_threadswitch_hook(rb_threadswitch_hook_func_t func) 
  __attribute__ ((weak_import));
extern void rb_remove_threadswitch_hook(void *handle) 
  __attribute__ ((weak_import));

/* Cached values for direct call to +[NSThread currentThread] (not clear if 
   this is a significant performance improvement) */
static Class rb_cocoa_NSThread_class;
static NSThread *(*rb_cocoa_NSThread_currentThread_imp)(id, SEL);
static SEL rb_cocoa_currentThread_SEL;

/* NSThread that was current when thread hooks were initialised. 
   Any Ruby activity that isn't on this thread will cause an error.
 */
static NSThread *rb_cocoa_main_nsthread;

/* Hash table mapping from a Ruby thread object (a VALUE) to a struct 
   rb_cocoa_thread_context */
static struct st_table *rb_cocoa_thread_state;

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
/* <= TIGER */

/* Offsets into NSThread object of important instance vars */
static int rb_cocoa_NSThread_autoreleasePool;
static int rb_cocoa_NSThread_excHandlers;

/* Access to the autoreleasePool ivar of NSThread */
# define NSTHREAD_autoreleasePool_get(t) \
    (*(void**)( ((char*)t) + rb_cocoa_NSThread_autoreleasePool ))
# define NSTHREAD_autoreleasePool_set(t, d) \
    (NSTHREAD_autoreleasePool_get(t) = d)
# define NSTHREAD_autoreleasePool_init(t) (NULL)

/* Access to the excHandlers ivar of NSThread */
# define NSTHREAD_excHandlers_get(t) \
    (*(void**)( ((char*)t) + rb_cocoa_NSThread_excHandlers ))
# define NSTHREAD_excHandlers_set(t, d) \
    (NSTHREAD_excHandlers_get(t) = d)
# define NSTHREAD_excHandlers_free(t, d)  
# define NSTHREAD_excHandlers_init(t) (NULL)

#else /* > TIGER */

#error No implementation yet for this system (please contact lsansonetti@apple.com)

#endif

struct rb_cocoa_thread_context
{
  void *  excHandlers;
  void *  autoreleasePool;
  BOOL    ignoreHooks;
};

static BOOL rb_cocoa_did_install_thread_hooks;

/* Substitute for +[NSThread currentThread]. Not clear if this is really 
   necessary, speed-wise */
static NSThread *rb_cocoa_get_current_NSThread() 
{
  return rb_cocoa_NSThread_currentThread_imp(rb_cocoa_NSThread_class, 
    rb_cocoa_currentThread_SEL);
}

int NSAutoreleasePoolCount(void);

/* Called when a new Ruby thread comes into existence.
   The new thread may not be the current thread.
*/
static void* rb_cocoa_thread_init_context(NSThread *thread, VALUE rbthread)
{
  struct rb_cocoa_thread_context *ctx;
  
  ctx = (struct rb_cocoa_thread_context *)malloc(
    sizeof(struct rb_cocoa_thread_context));
  ASSERT_ALLOC(ctx); 

  if (NIL_P(rbthread)) {
    ctx->excHandlers = NULL;
    ctx->autoreleasePool = NULL;
    ctx->ignoreHooks = YES;
  }
  else {
    ctx->ignoreHooks = NO;

    if (rbthread == rb_thread_main()) {
      // Ruby thread is main, we don't have to save anything at this time.
      ctx->excHandlers = NULL;
      ctx->autoreleasePool = NULL;
    } 
    else {
      void *backup = NULL;
      if (rbthread != rb_thread_current())
        backup = NSTHREAD_autoreleasePool_get(thread);

      ctx->excHandlers = NSTHREAD_excHandlers_init(thread);

      // Create an autorelease pool by default. All pools will be freed when the
      // Ruby thread will die.
      ctx->autoreleasePool = NSTHREAD_autoreleasePool_init(thread);
      NSTHREAD_autoreleasePool_set(thread, ctx->autoreleasePool);
      [[NSAutoreleasePool alloc] init];
      if (ctx->autoreleasePool != NULL) {
        assert(ctx->autoreleasePool == NSTHREAD_autoreleasePool_get(thread));
      }
      else {
        ctx->autoreleasePool = NSTHREAD_autoreleasePool_get(thread);
        assert(ctx->autoreleasePool != NULL);
      }

      if (backup != NULL)
        NSTHREAD_autoreleasePool_set(thread, backup);
    } 
    DLOG("Initialized excHandlers at %p and autoreleasePool at %p\n",
      ctx->excHandlers, ctx->autoreleasePool);
  }
  return ctx;
}

/* Attempt to free autorelease pool state when a ruby thread is released.
 */
static void rb_cocoa_release_all_pools() 
{
  if ([NSAutoreleasePool respondsToSelector:@selector(releaseAllPools)])
    [NSAutoreleasePool performSelector:@selector(releaseAllPools)];
}

/* Called when a Ruby thread is destroyed.
 */
static void rb_cocoa_thread_free_context(NSThread *thread, VALUE rbthread, 
  struct rb_cocoa_thread_context *ctx)
{
  if (!ctx->ignoreHooks && rbthread != rb_thread_main()) {
    void *save_pool;

    if (ctx->autoreleasePool != NULL
        && (save_pool = NSTHREAD_autoreleasePool_get(thread))
           != ctx->autoreleasePool) {

      DLOG("Releasing all pools at %p for thread %p with save pool %p\n", 
        ctx->autoreleasePool, (void *)rbthread, save_pool);

      /* Temporarily switch back the dead thread's autorelease pool so it can 
         be cleaned up */
      NSTHREAD_autoreleasePool_set(thread, ctx->autoreleasePool);

      rb_cocoa_release_all_pools();
      
      ctx->autoreleasePool = NSTHREAD_autoreleasePool_get(thread);
      NSTHREAD_autoreleasePool_set(thread, save_pool);
    
      if (ctx->autoreleasePool != NULL) {
        fprintf(stderr,"Warning: a Ruby thread leaked an autorelease pool %p (was %scurrent)\n",
          ctx->autoreleasePool,
          rbthread == rb_thread_current() ? "" : "not ");
      }
    }
  
    if (ctx->excHandlers != NULL)
      NSTHREAD_excHandlers_free(thread, ctx->excHandlers);
  }
  free(ctx);
}

static BOOL rb_cocoa_between_threads = NO;

/*
  Called when a Ruby thread is being restored.
  We must restore the NSThread exception handler stack and the autorelease pool 
  stack.
 */
static void rb_cocoa_thread_restore_context(NSThread *thread, 
  struct rb_cocoa_thread_context *ctx)
{
  void *oldExcHandlers;
  void *oldAutoreleasePool;

  if (ctx->ignoreHooks)
    return;

  oldExcHandlers = NSTHREAD_excHandlers_get(thread);
  if (oldExcHandlers != ctx->excHandlers) {
    DLOG("Restored excHandlers as %p\n", ctx->excHandlers);
    NSTHREAD_excHandlers_set(thread, ctx->excHandlers);
    if (ctx->excHandlers != NULL)
      assert(NSTHREAD_excHandlers_get(thread) == ctx->excHandlers);
  }

  oldAutoreleasePool = NSTHREAD_autoreleasePool_get(thread);
  if (oldAutoreleasePool != ctx->autoreleasePool) {
    NSTHREAD_autoreleasePool_set(thread, ctx->autoreleasePool);
    DLOG("Restored autoreleasePool as %p (%d)\n", ctx->autoreleasePool, NSAutoreleasePoolCount());
    //ctx->saveAutoreleasePool = YES;
    if (ctx->autoreleasePool != NULL)
      assert(NSTHREAD_autoreleasePool_get(thread) == ctx->autoreleasePool);
  }

  rb_cocoa_between_threads = NO;
}

/*
  Called when a Ruby thread is being saved.
  We must save the current NSThread exception handler stack and autorelease 
  pool stack.
 */
static void rb_cocoa_thread_save_context(NSThread *thread, 
  struct rb_cocoa_thread_context *ctx)
{
  if (ctx->ignoreHooks)
    return;

  if (rb_cocoa_between_threads)
    return;

  if (ctx->excHandlers == NULL) {
    ctx->excHandlers = NSTHREAD_excHandlers_get(thread);
    DLOG("Saved excHandlers as %p\n", ctx->excHandlers);
  }
  if (ctx->autoreleasePool == NULL) {
    ctx->autoreleasePool = NSTHREAD_autoreleasePool_get(thread);
    DLOG("Saved autoreleasePool as %p (%d)\n", ctx->autoreleasePool, NSAutoreleasePoolCount());
  }

  rb_cocoa_between_threads = YES;
}

/*
  This function is registered with the ruby core as a threadswitch event hook.
*/
static void rb_cocoa_thread_schedule_hook(rb_threadswitch_event_t event, 
  VALUE thread)
{
  void *context;
  NSThread *nsthread;

  nsthread = rb_cocoa_get_current_NSThread();
  if (nsthread == NULL)
      return;
  if (nsthread != rb_cocoa_main_nsthread) {
    rb_bug("rb_cocoa_thread_schedule_hook: expecting to run on NSThread %p but was %p\n",
      rb_cocoa_main_nsthread, nsthread);
  }
  switch (event) {
    case RUBY_THREADSWITCH_INIT:
      context = rb_cocoa_thread_init_context(nsthread, 
        rb_cocoa_did_install_thread_hooks || thread == rb_thread_main() 
          ? thread : Qnil);
      DLOG("Created context %p for thread %p\n", context, (void*)thread);
      st_insert(rb_cocoa_thread_state, (st_data_t)thread, (st_data_t)context);
      break;
            
    case RUBY_THREADSWITCH_FREE:
      if (st_delete(rb_cocoa_thread_state, (st_data_t*)&thread, 
        (st_data_t *)&context)) {

        DLOG("Freeing context %p for thread %p\n", context, (void*)thread);
        rb_cocoa_thread_free_context(nsthread,thread, 
          (struct rb_cocoa_thread_context*) context);
      }
      break;

    case RUBY_THREADSWITCH_SAVE:
      if (!st_lookup(rb_cocoa_thread_state, (st_data_t)thread, 
        (st_data_t *)&context)) {

        DLOG("Created context before save %p for thread %p\n", context, 
          (void*)thread);
        context = rb_cocoa_thread_init_context(nsthread, thread);
        st_insert(rb_cocoa_thread_state, (st_data_t)thread, (st_data_t)context);
      }
      DLOG("Saving context %p for thread %p\n", context, (void*)thread);
      rb_cocoa_thread_save_context(nsthread,
        (struct rb_cocoa_thread_context*) context);
      break;
            
    case RUBY_THREADSWITCH_RESTORE:
      if (st_lookup(rb_cocoa_thread_state, (st_data_t)thread, 
        (st_data_t *)&context)) {
        
        DLOG("Restoring context %p for thread %p\n", context, (void*)thread);
        rb_cocoa_thread_restore_context(nsthread,
          (struct rb_cocoa_thread_context*) context);
      }
      break;
  }
}

static void RBCocoaInstallRubyThreadSchedulerHooks()
{
  if (getenv("RUBYCOCOA_THREAD_HOOK_DISABLE") != NULL) {
    if (rb_cocoa_thread_debug) {
      NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: warning: disabled hooks due to RUBYCOCOA_THREAD_HOOK_DISABLE environment variable");
    }
    return;
  }
  
  rb_cocoa_thread_debug = getenv("RUBYCOCOA_THREAD_DEBUG") != NULL;
  
  if (rb_add_threadswitch_hook == NULL) {
    if (rb_cocoa_thread_debug) {
      NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: warning: rb_set_cocoa_thread_hooks not present in ruby core");
    }
    return;
  }
  
  NSSymbol threadswitch_symbol = 
    NSLookupAndBindSymbol("_rb_add_threadswitch_hook");
  NSSymbol ruby_init_symbol = 
    NSLookupAndBindSymbol("_ruby_init");
  
  if (NSModuleForSymbol(threadswitch_symbol) 
      != NSModuleForSymbol(ruby_init_symbol)) {
    NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: warning: rb_set_cocoa_thread_hooks is linked from a different library (%s) than ruby_init (%s)",
      NSNameOfModule(NSModuleForSymbol(threadswitch_symbol)), NSNameOfModule(NSModuleForSymbol(ruby_init_symbol)));
    return;
  }

  rb_cocoa_thread_state = st_init_numtable();  
  rb_cocoa_main_nsthread = [NSThread currentThread];
  
  rb_cocoa_NSThread_class = objc_lookUpClass("NSThread");
  if (rb_cocoa_NSThread_class == NULL) {
    NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: couldn't find NSThread class");
    return;
  }

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4  
  Ivar v;

  v = class_getInstanceVariable(rb_cocoa_NSThread_class, "autoreleasePool");
  if (v == NULL) {
    NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: couldn't find autoreleasePool ivar");
    return;
  }
  
  rb_cocoa_NSThread_autoreleasePool = v->ivar_offset;
  
  v = class_getInstanceVariable(rb_cocoa_NSThread_class, "excHandlers");
  if (v == NULL) {
    NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: couldn't find excHandlers ivar");
    return;
  }
  
  rb_cocoa_NSThread_excHandlers = v->ivar_offset;
#endif

  rb_cocoa_currentThread_SEL = @selector(currentThread);
  Method method = class_getClassMethod(rb_cocoa_NSThread_class, 
    rb_cocoa_currentThread_SEL);
  if (method == NULL) {
    NSLog(@"RBCocoaInstallRubyThreadSchedulerHooks: can't find IMP for +[NSThread currentThread]");
    return;
  }
  rb_cocoa_NSThread_currentThread_imp = (NSThread*(*)(id, SEL)) 
    method_getImplementation(method);

  /* Finally, register the hook with the ruby core */
  rb_add_threadswitch_hook(rb_cocoa_thread_schedule_hook);
  rb_cocoa_did_install_thread_hooks = YES;

  DLOG("Thread hooks done, main Ruby thread is %p\n", 
    (void *)rb_thread_current());
}

@interface RBRuntime : NSObject
@end

@implementation RBRuntime
+(BOOL)isRubyThreadingSupported 
{
  return rb_cocoa_did_install_thread_hooks;
}
@end

BOOL RBIsRubyThreadingSupported (void)
{
  return [RBRuntime isRubyThreadingSupported];
}

static BOOL rb_cocoa_check_for_multiple_libruby() 
{
  int i, count = _dyld_image_count();
  const char *name;
  const char *libruby_name = NULL;
  BOOL multiple = false;
  
  for (i=0;i<count;i++) {
    name = _dyld_get_image_name(i);
    if (name && strstr(name, "/libruby.")) {
      if (libruby_name) {
        NSLog(@"WARNING: multiple libruby.dylib found: '%s' and '%s'", 
          libruby_name, name);
        multiple = true;
      }
      libruby_name = name;
    }
  }
  return multiple;
}
