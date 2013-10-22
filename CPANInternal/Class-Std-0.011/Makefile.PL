use strict;
use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME                => 'Class::Std',
    AUTHOR              => 'Damian Conway <DCONWAY@cpan.org>',
    VERSION_FROM        => 'lib/Class/Std.pm',
    ABSTRACT_FROM       => 'lib/Class/Std.pm',
    PL_FILES            => {},
    PREREQ_PM => {
        'Test::More'   => 0,
        'Scalar::Util' => 0,
        'Data::Dumper' => 0,

    },
    dist                => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean               => { FILES => 'Class-Std-*' },
);

