/* build with: dtrace -C -h -s dtrace_cxa_runtime.d -o dtrace_cxa_runtime.h */

provider cxa_runtime
{
    probe cxa_exception_throw(void *obj);
    probe cxa_exception_rethrow();
};
