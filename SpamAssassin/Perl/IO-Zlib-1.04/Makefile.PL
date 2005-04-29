use ExtUtils::MakeMaker;

WriteMakefile(
	NAME         => 'IO::Zlib',
	VERSION_FROM => 'Zlib.pm',
        PREREQ_PM    => { 'Compress::Zlib' => 0 },
	dist         => { COMPRESS => 'gzip', SUFFIX => 'gz' },
);
