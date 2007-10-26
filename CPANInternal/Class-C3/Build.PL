use Module::Build;

use strict;

my $build = Module::Build->new(
    module_name => 'Class::C3',
    license => 'perl',
    requires => {
        'Scalar::Util'    => 1.10,
        'Algorithm::C3'   => 0.05,
    },
    optional => {},
    build_requires => {
        'Test::More' => '0.47',
        'Test::Exception' => 0.15,
    },
    create_makefile_pl => 'traditional',
    recursive_test_files => 1,
    add_to_cleanup => [
        'META.yml', '*.bak', '*.gz', 'Makefile.PL',
    ],
);

$build->create_build_script;

