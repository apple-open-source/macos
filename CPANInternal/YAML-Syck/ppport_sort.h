#ifndef PATCHLEVEL
#include <patchlevel.h>		/* Perl's one, needed since 5.6 */
#endif

#if (PATCHLEVEL <= 6)

#if defined(USE_ITHREADS)

#define STORE_HASH_SORT \
        ENTER; { \
        PerlInterpreter *orig_perl = PERL_GET_CONTEXT; \
        SAVESPTR(orig_perl); \
        PERL_SET_CONTEXT(aTHX); \
        qsort((char *) AvARRAY(av), len, sizeof(SV *), sortcmp); \
        } LEAVE;

#else /* ! USE_ITHREADS */

#define STORE_HASH_SORT \
        qsort((char *) AvARRAY(av), len, sizeof(SV *), sortcmp);

#endif  /* USE_ITHREADS */

#else /* PATCHLEVEL > 6 */

#define STORE_HASH_SORT \
        sortsv(AvARRAY(av), len, Perl_sv_cmp);  

#endif /* PATCHLEVEL <= 6 */

#if (PATCHLEVEL <= 6)

/*
 * sortcmp
 *
 * Sort two SVs
 * Borrowed from perl source file pp_ctl.c, where it is used by pp_sort.
 */
static int
sortcmp(const void *a, const void *b)
{
#if defined(USE_ITHREADS)
        dTHX;
#endif /* USE_ITHREADS */
        return sv_cmp(*(SV * const *) a, *(SV * const *) b);
}

#endif /* PATCHLEVEL <= 6 */
