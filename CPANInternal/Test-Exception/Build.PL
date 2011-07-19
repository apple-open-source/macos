use Module::Build;

my $build = Module::Build->new(
    module_name => 'Test::Exception',
    license => 'perl',
    requires => {
        # 'perl'                  => '5.6.1',
        'Test::Simple'          => '0.7',
        'Test::Builder'         => '0.7',
        'Test::Builder::Tester' => '1.07',
        'Test::More'            => '0.7',
        'Test::Harness'         => '2.03',
        'Sub::Uplevel'          => '0.18',
    },
    create_makefile_pl => 'traditional',
    create_readme => 1,
);
$build->create_build_script;

