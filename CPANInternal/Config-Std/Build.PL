use strict;
use warnings;
use Module::Build;

my $builder = Module::Build->new(
    module_name         => 'Config::Std',
    license             => 'perl',
    dist_author         => 'Damian Conway <DCONWAY@cpan.org>',
    dist_version_from   => 'lib/Config/Std.pm',
    requires => {
        'Test::More' => 0,
        'Class::Std' => 0,
        'version'    => 0,
    },
    add_to_cleanup      => [ 'Config-Std-*' ],
);

$builder->create_build_script();
