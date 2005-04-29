require 5.005_03;  # need self-tie patch

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME	 => 'IO::String',
    VERSION_FROM => 'String.pm',
    dist         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
);
