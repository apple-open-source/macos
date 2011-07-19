use Module::Build;

Module::Build->new(
    module_name         => 'DateTime::Format::ISO8601',
    dist_version_from   => 'lib/DateTime/Format/ISO8601.pm',
    author              => 'Joshua Hoblitt <jhoblitt@cpan.org>',
    license             => 'perl',
    create_makefile_pl  => 'passthrough',
    requires            => {
        DateTime                    => '0.18',
        DateTime::Format::Builder   => '0.77',
    },
    recommends          => {
        'Test::Pod'                 => '0.95',
        'File::Find::Rule'          => '0.24',
        'Test::Distribution'        => '1.22',
    },
)->create_build_script;
