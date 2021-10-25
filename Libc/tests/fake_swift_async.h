#include <stdint.h>
#include <stdalign.h>

#include <ptrauth.h>

#if __has_feature(ptrauth_calls)
#define __ptrauth_swift_async_context_parent                                   \
  __ptrauth(ptrauth_key_process_independent_data, 1, 0xbda2)
#define __ptrauth_swift_async_context_resume                                   \
  __ptrauth(ptrauth_key_function_pointer, 1, 0xd707)
#else
#define __ptrauth_swift_async_context_parent
#define __ptrauth_swift_async_context_resume
#endif

// This struct fakes the Swift AsyncContext struct which is used by
// the Swift concurrency runtime. We only care about the first 2 fields.
struct fake_async_context {
    struct fake_async_context* __ptrauth_swift_async_context_parent next;
    void (* __ptrauth_swift_async_context_resume resume_pc)(void);
};

struct fake_async_task {
    void *padding1[4];
    uint32_t padding2;
    uint32_t task_id;
};
#define FAKE_TASK_ID 1234

static void level1_func() {}
static void level2_func() {}

// Create a chain of fake async contexts
static alignas(16) struct fake_async_context root = { 0 };
static alignas(16) struct fake_async_context level1 = { &root, level1_func };
static alignas(16) struct fake_async_context level2 = { &level1, level2_func };
static alignas(16) struct fake_async_task task = { .task_id = FAKE_TASK_ID };

