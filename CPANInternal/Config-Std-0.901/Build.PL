use strict;
use warnings;
use Module::Build '0.19';

my $builder = Module::Build->new(
    module_name         => 'Config::Std',
    license             => 'perl',
    create_license      => 'perl',
    dist_author         => [ 	'Damian Conway <DCONWAY@cpan.org>',
				'Bill Ricker <BRICKER@cpan.org>',
				'Tom Metro <tmetro@cpan.org>'],

    dist_version_from   => 'lib/Config/Std.pm',

    requires => {
        'Test::More' => 0,
        'Class::Std' => 0,
        'version'    => 0,
 	'perl'       => '5.7.3',

    },
    recommends => {
	'Test::Pod' => 0,
	'Test::Distribution' => 0,
    },
    add_to_cleanup      => [ 'Config-Std-*' ],
    create_makefile_pl => 'traditional', # Module::Build::Compat
);

$builder->create_build_script();
