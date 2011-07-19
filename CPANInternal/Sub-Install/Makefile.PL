use strict;
use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME                => 'Sub::Install',
    AUTHOR              => 'Ricardo Signes <rjbs@cpan.org>',
    VERSION_FROM        => 'lib/Sub/Install.pm',
    ABSTRACT_FROM       => 'lib/Sub/Install.pm',
    LICENSE             => 'perl',
    PL_FILES            => {},
    PREREQ_PM => {
        'Scalar::Util' => 0,
        'Test::More'   => 0,
    },
    (eval { ExtUtils::MakeMaker->VERSION(6.46) }
      ? (META_MERGE => {
          resources => {
            Repository => 'http://github.com/rjbs/sub-install'
          }
        })
      : ()
    ),
    dist                => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean               => { FILES => 'Sub-Install-* cover_db' },
);
