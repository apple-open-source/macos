#ifndef __OSX_ASSUMES_H__
#define __OSX_ASSUMES_H__

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <Availability.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <asl.h>

#if __GNUC__
#define osx_fastpath(x)	((typeof(x))__builtin_expect((long)(x), ~0l))
#define osx_slowpath(x)	((typeof(x))__builtin_expect((long)(x), 0l))
#define osx_constant(x) __builtin_constant_p((x))

#define __OSX_COMPILETIME_ASSERT__(e) ({ \
	char __compile_time_assert__[(e) ? 1 : -1];	\
	(void)__compile_time_assert__; \
})
#else
#define osx_fastpath(x)	(x)
#define osx_slowpath(x)	(x)
#define osx_constant(x) ((long)0)

#define __OSX_COMPILETIME_ASSERT__(e) (e)
#endif /* __GNUC__ */

#define osx_assumes(e) ({ \
	typeof(e) _e = osx_fastpath(e); /* Force evaluation of 'e' */ \
	if (!_e) { \
		if (osx_constant(e)) { \
			__OSX_COMPILETIME_ASSERT__(e); \
		} \
		_osx_assumes_log((uintptr_t)_e); \
	} \
	_e; \
})

#define osx_assumes_zero(e) ({ \
	typeof(e) _e = osx_slowpath(e); /* Force evaluation of 'e' */ \
	if (_e) { \
		if (osx_constant(e)) { \
			__OSX_COMPILETIME_ASSERT__(!e); \
		} \
		_osx_assumes_log((uintptr_t)_e); \
	} \
	_e; \
})

__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3)
extern void
_osx_assumes_log(uint64_t code);

__END_DECLS

#endif /* __OSX_ASSUMES_H__ */
