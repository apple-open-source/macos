use strict;
use warnings;
use Module::Build;

my $builder = Module::Build->new(
    module_name         => 'Class::Std::Utils',
    license             => 'perl',
    dist_author         => 'Damian Conway <DCONWAY@cpan.org>',
    dist_version_from   => 'lib/Class/Std/Utils.pm',
    requires => {
        'Test::More' => 0,
        'List::Util' => 0,
        'Scalar::Util' => 0,
        'version'    => 0,
    },
    add_to_cleanup      => [ 'Class-Std-Utils-*' ],
);

$builder->create_build_script();
