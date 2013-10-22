use 5.008002;
use ExtUtils::MakeMaker;

WriteMakefile(
    dist	 => {
	             PREOP	=> 'pod2text XS.pm | tee README >$(DISTVNAME)/README; chmod -R u=rwX,go=rX . ;',
	             COMPRESS	=> 'gzip -9v',
	             SUFFIX	=> '.gz',
	            },
    EXE_FILES    => [ "bin/json_xs" ],
    VERSION_FROM => "XS.pm",
    NAME         => "JSON::XS",
    PREREQ_PM    => {
       common::sense => 0,
    },
);

