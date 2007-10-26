use strict;
use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME                => 'Class::Std::Utils',
    AUTHOR              => 'Damian Conway <DCONWAY@cpan.org>',
    VERSION_FROM        => 'lib/Class/Std/Utils.pm',
    ABSTRACT_FROM       => 'lib/Class/Std/Utils.pm',
    PL_FILES            => {},
    PREREQ_PM => {
        'Test::More' => 0,
        'List::Util' => 0,
        'Scalar::Util' => 0,
        'version'    => 0,
    },
    dist                => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean               => { FILES => 'Class-Std-Utils-*' },
);
