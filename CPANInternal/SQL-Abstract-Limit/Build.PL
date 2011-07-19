use strict;
use warnings;
use Module::Build;

my $builder = Module::Build->new(
    module_name         => 'SQL::Abstract::Limit',
    license             => 'perl',
    dist_author         => 'David Baird <cpan@riverside-cms.co.uk>',
    dist_version_from   => 'lib/SQL/Abstract/Limit.pm',
    requires => { 'Test::More' => 0,
                  'Test::Exception' => 0,
                  'Test::Builder' => 0,
                  'Test::Deep' => 0,
                  'SQL::Abstract' => '1.2',
                  'Scalar::Util' => 0,
                  'Data::Dumper' => 0,
                  'DBI' => 0, # for DBI::Const::GetInfoType
                  'SQL::Abstract' => 1.2,
                  },
	create_makefile_pl  => 'traditional',
    add_to_cleanup      => [ 'SQL-Abstract-Limit-*' ],
);

$builder->create_build_script();
