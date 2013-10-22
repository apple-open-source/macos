# $Id: Makefile.PL,v 1.5 2010-06-11 20:00:24 roderick Exp $
#
# Copyright (c) 1997 Roderick Schertler.  All rights reserved.  This
# program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME		=> 'String::ShellQuote',
    VERSION_FROM	=> 'ShellQuote.pm',
    (
	$^O eq 'MSWin32'
	    ? ()
	    : (EXE_FILES => ['shell-quote'])
    ),
    dist		=> {
    	PREOP		=> '$(MAKE) ci',
	CI		=> 'cvs commit',
	RCS_LABEL	=> 'cvs tag v$(VERSION_SYM)',
    },
    test		=> { TESTS => 'test.t' },
);
