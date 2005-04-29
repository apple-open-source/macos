#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * expression library
 */

#include <exlib.h>

/*
 * library error handler
 */

void
exerror(const char* format, ...)
{
	Sfio_t*	sp;

	if (expr.program->disc->errorf && !expr.program->errors && (sp = sfstropen()))
	{
		va_list	ap;
		char*	s;
		char	buf[64];

		expr.program->errors = 1;
		excontext(expr.program, buf, sizeof(buf));
		sfputr(sp, buf, -1);
		sfputr(sp, "\n -- ", -1);
		va_start(ap, format);
		sfvprintf(sp, format, ap);
		va_end(ap);
		s = sfstruse(sp);
		(*expr.program->disc->errorf)(expr.program, expr.program->disc, (expr.program->disc->flags & EX_FATAL) ? ERROR_FATAL : ERROR_ERROR, "%s", s);
		sfclose(sp);
	}
	else if (expr.program->disc->flags & EX_FATAL)
		exit(1);
}

void
exwarn(const char* format, ...)
{
	Sfio_t*	sp;

	if (expr.program->disc->errorf && (sp = sfstropen()))
	{
		va_list	ap;
		char*	s;
		char	buf[64];

		excontext(expr.program, buf, sizeof(buf));
		sfputr(sp, buf, -1);
		sfputr(sp, "\n -- ", -1);
		va_start(ap, format);
		sfvprintf(sp, format, ap);
		va_end(ap);
		s = sfstruse(sp);
		(*expr.program->disc->errorf)(expr.program, expr.program->disc, ERROR_WARNING, "%s", s);
		sfclose(sp);
	}
}
