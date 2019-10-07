/*
 * Closure require special memmory support on x86_64 and PPC64: executation must be
 * explicitly enabled for the memory used for closure.
 */
#ifndef PyObjC_CLOSURE_POOL
#define PyObjC_CLOSURE_POOL

typedef struct ffi_closure_wrapper {
	ffi_closure* closure;
	void* code_addr;
} ffi_closure_wrapper;

extern ffi_closure_wrapper* PyObjC_malloc_closure(void);
extern int PyObjC_free_closure(ffi_closure_wrapper* cl);
extern ffi_closure_wrapper* PyObjC_closure_from_code(void* code);


#endif /* PyObjC_CLOSURE_POOL */
