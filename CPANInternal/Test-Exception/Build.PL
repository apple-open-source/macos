use Module::Build;

my $build = Module::Build->new(
    module_name => 'Test::Exception',
    license => 'perl',
    requires => {
        'perl'                  => '5.6.1',
        'Test::Builder'         => '0.33',
        'Test::Builder::Tester' => '1.04',
        'Test::More'            => '0.44',
        'Test::Harness'         => '2.03',
        'Sub::Uplevel'          => '0.13',
    },
    create_makefile_pl => 'traditional',
    create_readme => 1,
);
$build->create_build_script;

