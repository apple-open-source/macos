use strict;
use Module::Build;

my %arguments = (
    license            => 'perl',
    module_name        => 'DBIx::Class::Schema::Loader',
    requires           => {
        'File::Spec'                    => 0,
        'Scalar::Util'                  => 0,
        'Data::Dump'                    => 1.06,
        'UNIVERSAL::require'            => 0.10,
        'Lingua::EN::Inflect'           => 1.89,
        'Lingua::EN::Inflect::Number'   => 1.1,
        'Text::Balanced'                => 0,
        'Class::Accessor'               => 0.22,
        'Class::Data::Accessor'         => 0.02,
        'Class::C3'                     => 0.11,
        'Carp::Clan'                    => 0,
        'DBIx::Class'                   => 0.06003,
    },
    recommends         => {
        'Class::Inspector'              => 0,
        'DBI'                           => 1.50,
        'DBD::SQLite'                   => 1.12,
        'DBD::mysql'                    => 3.0003,
        'DBD::Pg'                       => 1.49,
        'DBD::DB2'                      => 0.78,
    },
    build_requires     => {
        'Test::More'                    => 0.32,
        'DBI'                           => 1.50,
        'DBD::SQLite'                   => 1.12,
        'File::Path'                    => 0,
    },
    create_makefile_pl => 'passthrough',
    create_readme      => 1,
);

Module::Build->new(%arguments)->create_build_script;
