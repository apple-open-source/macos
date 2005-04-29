#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * expression library
 */

#include "exlib.h"

/*
 * return the expression for name or sym coerced to type
 */

Exnode_t*
exexpr(Expr_t* ex, const char* name, Exid_t* sym, int type)
{
	if (ex)
	{
		if (!sym)
			sym = name ? (Exid_t*)dtmatch(ex->symbols, name) : &ex->main;
		if (sym && sym->lex == PROCEDURE && sym->value)
		{
			if (type != DELETE)
				return excast(ex, sym->value->data.procedure.body, type, NiL, 0);
			exfreenode(ex, sym->value);
			sym->lex = NAME;
			sym->value = 0;
		}
	}
	return 0;
}
