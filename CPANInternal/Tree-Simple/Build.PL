use Module::Build;

use 5.006;

use strict;
use warnings;

my $build = Module::Build->new(
    module_name => 'Tree::Simple',
    dist_version_from => 'lib/Tree/Simple.pm',
    license => 'perl',
    requires => {
        'perl'         => '5.6.0',
        'Scalar::Util' => '1.18',
    },
    build_requires => {
        'Test::More'      => '0.47',
        'Test::Exception' => '0.15',
    },
    create_makefile_pl => 'traditional',
    recursive_test_files => 1,
);

$build->create_build_script;
