
#ifndef OPENSOURCE
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFRuntime.h>
#endif

struct heim_base {
#ifndef OPENSOURCE
    CFRuntimeBase base;
#else
#ifdef HEIM_BASE_INTERNAL
    heim_type_t isa;
    heim_base_atomic_type ref_cnt;
    HEIM_TAILQ_ENTRY(heim_base) autorel;
    heim_auto_release_t autorelpool;
    uintptr_t isaextra[3];
#else
    void *data[8];
#endif
#endif
};

/* specialized version of base */
struct heim_base_uniq {
#ifndef OPENSOURCE
    CFRuntimeBase base;
    const char *name;
    void (*dealloc)(void *);
#else
#ifdef HEIM_BASE_INTERNAL
    heim_type_t isa;
    heim_base_atomic_type ref_cnt;
    HEIM_TAILQ_ENTRY(heim_base) autorel;
    heim_auto_release_t autorelpool;
    const char *name;
    void (*dealloc)(void *);
    uintptr_t isaextra[1];
#else
    void *data[8];
#endif
#endif
};

