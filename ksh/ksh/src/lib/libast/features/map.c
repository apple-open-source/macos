/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#pragma prototyped

#include "FEATURE/vmalloc"

extern int	printf(const char*, ...);

main()
{
#if __MVS__
	printf("\n");
	printf("/* mvs.390 libc.dll routines can't be intercepted by user dlls */\n");
	printf("#undef	_mem_dd_fd_DIR\n");
	printf("#undef	_typ_long_double\n");
	printf("#define basename	_ast_basename\n");
	printf("#define dirname		_ast_dirname\n");
	printf("#define execvpe		_ast_execvpe\n");
	printf("#define fts_children    _ast_fts_children\n");
	printf("#define fts_close       _ast_fts_close\n");
	printf("#define fts_flags       _ast_fts_flags\n");
	printf("#define fts_notify      _ast_fts_notify\n");
	printf("#define fts_open	_ast_fts_open\n");
	printf("#define fts_read	_ast_fts_read\n");
	printf("#define fts_set		_ast_fts_set\n");
	printf("#define ftw		_ast_ftw\n");
	printf("#define ftwalk		_ast_ftwalk\n");
	printf("#define ftwflags	_ast_ftwflags\n");
	printf("#define getcwd		_ast_getcwd\n");
	printf("#define getdate		_ast_getdate\n");
	printf("#define getopt		_ast_getopt\n");
	printf("#define getsubopt       _ast_getsubopt\n");
	printf("#define getwd		_ast_getwd\n");
	printf("#define glob		_ast_glob\n");
	printf("#define globfree	_ast_globfree\n");
	printf("#define memfatal	_ast_memfatal\n");
	printf("#define mkstemp		_ast_mkstemp\n");
	printf("#define mktemp		_ast_mktemp\n");
	printf("#define mktime		_ast_mktime\n");
	printf("#define nftw		_ast_nftw\n");
	printf("#define putenv		_ast_putenv\n");
	printf("#define re_comp		_ast_re_comp\n");
	printf("#define re_exec		_ast_re_exec\n");
	printf("#define realpath	_ast_realpath\n");
	printf("#define regalloc	_ast_regalloc\n");
	printf("#define regclass	_ast_regclass\n");
	printf("#define regcmp		_ast_regcmp\n");
	printf("#define regcollate      _ast_regcollate\n");
	printf("#define regcomb		_ast_regcomb\n");
	printf("#define regcomp		_ast_regcomp\n");
	printf("#define regerror	_ast_regerror\n");
	printf("#define regex		_ast_regex\n");
	printf("#define regexec		_ast_regexec\n");
	printf("#define regfatal	_ast_regfatal\n");
	printf("#define regfatalpat     _ast_regfatalpat\n");
	printf("#define regfree		_ast_regfree\n");
	printf("#define regnexec	_ast_regnexec\n");
	printf("#define regrecord       _ast_regrecord\n");
	printf("#define regrexec	_ast_regrexec\n");
	printf("#define regsub		_ast_regsub\n");
	printf("#define resolvepath	_ast_resolvepath\n");
	printf("#define setenviron      _ast_setenviron\n");
	printf("#define strftime	_ast_strftime\n");
	printf("#define strptime	_ast_strptime\n");
	printf("#define strtol		_ast_strtol\n");
	printf("#define strtoul		_ast_strtoul\n");
	printf("#define strtoll		_ast_strtoll\n");
	printf("#define strtoull	_ast_strtoull\n");
	printf("#define system		_ast_system\n");
	printf("#define tempnam		_ast_tempnam\n");
	printf("#define tmpnam		_ast_tmpnam\n");
	printf("#define touch		_ast_touch\n");
	printf("#define wordexp		_ast_wordexp\n");
	printf("#define wordfree	_ast_wordfree\n");
#endif
#if _std_malloc
	printf("\n");
	printf("/* no local malloc override */\n");
	printf("#define	_std_malloc	1\n");
#else
#if _map_malloc
	printf("\n");
	printf("/* cannot override local malloc */\n");
	printf("#define	_map_malloc	1\n");
	printf("#undef	calloc\n");
	printf("#define calloc		_ast_calloc\n");
	printf("#undef	cfree\n");
	printf("#define cfree		_ast_cfree\n");
	printf("#undef	free\n");
	printf("#define free		_ast_free\n");
#if _lib_mallinfo
	printf("#undef	mallinfo\n");
	printf("#define mallinfo	_ast_mallinfo\n");
#endif
	printf("#undef	malloc\n");
	printf("#define malloc		_ast_malloc\n");
#if _lib_mallopt
	printf("#undef	mallopt\n");
	printf("#define mallopt		_ast_mallopt\n");
#endif
#if _lib_memalign
	printf("#undef	memalign\n");
	printf("#define memalign	_ast_memalign\n");
#endif
#if _lib_mstats
	printf("#undef	mstats\n");
	printf("#define mstats		_ast_mstats\n");
#endif
#if _lib_pvalloc
	printf("#undef	pvalloc\n");
	printf("#define pvalloc		_ast_pvalloc\n");
#endif
	printf("#undef	realloc\n");
	printf("#define realloc		_ast_realloc\n");
	printf("#undef	strdup\n");
	printf("#define strdup		_ast_strdup\n");
#if _lib_valloc
	printf("#undef	valloc\n");
	printf("#define valloc		_ast_valloc\n");
#endif
#endif
#endif
	return 0;
}
