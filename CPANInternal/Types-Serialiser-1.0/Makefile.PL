use ExtUtils::MakeMaker;

WriteMakefile(
    dist	 => {
	             PREOP	=> 'pod2text Serialiser.pm | tee README >$(DISTVNAME)/README; chmod -R u=rwX,go=rX . ;',
	             COMPRESS	=> 'gzip -9v',
	             SUFFIX	=> '.gz',
	            },
    VERSION_FROM => "Serialiser.pm",
    NAME         => "Types::Serialiser",
    PREREQ_PM    => {
       common::sense     => 0,
    },
);

