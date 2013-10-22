use ExtUtils::MakeMaker;

WriteMakefile(
    dist	=> {
	            PREOP	=> 'pod2text sense.pm | tee README >$(DISTVNAME)/README; chmod -R u=rwX,go=rX . ;',
	            COMPRESS	=> 'gzip -9v',
	            SUFFIX	=> '.gz',
	           },
    NAME        => "common::sense",
    VERSION_FROM => "sense.pm.PL",
    PM          => { "sense.pm" => '$(INST_LIB)/common/sense.pm' },
    clean       => { FILES => "sense.pm" },
);

