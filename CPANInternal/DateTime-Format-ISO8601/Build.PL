use Module::Build;
my $build = new Module::Build
    (
        module_name         => 'DateTime::Format::ISO8601',
        dist_version_from   => 'lib/DateTime/Format/ISO8601.pm',
        author              => 'Joshua Hoblitt <jhoblitt@cpan.org>',
        license             => 'perl',
        requires            => {
            DateTime                    => '0.18',
            DateTime::Format::Builder   => '0.77',
        },
        recommends          => {
            Test::Pod                   => '0.95',
            File::Find::Rule            => '0.24',
        },
    );
$build->create_build_script;
