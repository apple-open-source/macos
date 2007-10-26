use Module::Build;

my $build = Module::Build->new(
    module_name        => 'Text::WordDiff',
    license            => 'perl',
    requires           => {
        Algorithm::Diff => '1.19',
        Term::ANSIColor => '0',
        HTML::Entities  => '0',
    },

    build_requires     => {
        Test::Simple => '0.17',
        File::Spec   => '0',
    },
    create_makefile_pl => 'passthrough',
    create_readme      => 1,
);
$build->create_build_script;
