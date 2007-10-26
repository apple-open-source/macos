use ExtUtils::MakeMaker;

WriteMakefile(
    dist	=> {
	            PREOP	=> 'pod2text Rijndael.pm | tee README >$(DISTVNAME)/README; chmod -R u=rwX,go=rX . ;',
	            COMPRESS	=> 'gzip -9v',
	            SUFFIX	=> '.gz',
	           },
    NAME => "Crypt::Rijndael",
    OBJECT => q[_rijndael$(OBJ_EXT) Rijndael$(OBJ_EXT)],
    VERSION_FROM => "Rijndael.pm",
);

