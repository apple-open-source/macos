use strict;
use warnings;
use Module::Build;

my $builder = Module::Build->new(
    module_name         => 'Class::Std',
    license             => 'perl',
    dist_author         => 'Damian Conway <DCONWAY@cpan.org>',
    dist_version_from   => 'lib/Class/Std.pm',
    requires => {
        'Test::More'   => 0,
        'Scalar::Util' => 0,
        'Data::Dumper' => 0,
    },
    add_to_cleanup      => [ 'Class-Std-*' ],
);

$builder->create_build_script();
