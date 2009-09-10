// 2008-08-27 Howard Hinnant

#include <utility>
#include <testsuite_hooks.h>

int main()
{
#ifndef __GNUC_LIBSTD__
    return 1;
#else
    return 0;
#endif
}
