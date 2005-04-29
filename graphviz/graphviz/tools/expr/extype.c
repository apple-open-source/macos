#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * expression library C program generator
 */

#include "exlib.h"

#define str(s)		# s
#define xstr(s)		str(s)

/*
 * return C type name for type
 */

char*
extype(int type)
{
	switch (type)
	{
	case FLOATING:
		return "double";
	case STRING:
		return "char*";
	case UNSIGNED:
		return xstr(unsigned _ast_intmax_t);
	}
	return xstr(_ast_intmax_t);
}
