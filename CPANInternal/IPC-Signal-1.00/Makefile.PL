# $Id: Makefile.PL,v 1.2 1997-05-15 23:22:49-04 roderick Exp $
#
# Copyright (c) 1997 Roderick Schertler.  All rights reserved.  This
# program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME		=> 'IPC::Signal',
    VERSION_FROM	=> 'Signal.pm',
    dist		=> {
    	COMPRESS 	=> 'gzip --best',
    	DIST_CP		=> 'ln',
    	PREOP		=> '$(MAKE) ci',
    	SUFFIX		=> 'gz',
    },
);
