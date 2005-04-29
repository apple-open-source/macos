use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
    dist	=> {
	            COMPRESS	=> 'gzip -9v',
	            SUFFIX	=> '.gz',
	           },
    'NAME'	    => 'Razor2::Preproc::deHTMLxs',
    'OBJECT'    => q[_deHTMLxs$(OBJ_EXT) deHTMLxs$(OBJ_EXT)],
    'VERSION_FROM'      => 'deHTMLxs.pm', # finds $VERSION
    'LIBS'              => [''], # e.g., '-lm'
    'DEFINE'            => '',   # e.g., '-DHAVE_SOMETHING'
    'INC'               => '',   # e.g., '-I/usr/include/other'
);


#PREOP	=> 'pod2text deHTMLxs.pm | tee README >$(DISTVNAME)/README; chmod -R u=rwX,go=rX . ;',
