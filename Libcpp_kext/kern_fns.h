/* These are the declarations needed to build Libcpp_kext.a. */

typedef unsigned long size_t;

extern "C" {
extern void *kern_os_malloc(size_t);
extern void kern_os_free(void*);
extern int panic(void);
};

#ifdef __cplusplus

namespace std {
  typedef void (*new_handler)();
  struct nothrow_t { };
  const nothrow_t nothrow = { };
  new_handler
    set_new_handler (new_handler handler) ;
}


typedef void (*new_handler)();
extern new_handler __new_handler;

#endif

using std::new_handler;
