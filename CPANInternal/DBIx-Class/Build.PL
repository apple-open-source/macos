use strict;
use Module::Build;

my %arguments = (
    create_makefile_pl => 'passthrough',
    license            => 'perl',
    module_name        => 'DBIx::Class',
    requires           => {
        'Cwd'                       => 3.19,
        'Data::Page'                => 2.00,
        'Scalar::Util'              => 0,
        'SQL::Abstract'             => 1.20,
        'SQL::Abstract::Limit'      => 0.101,
        'Class::C3'                 => 0.13,
        'Storable'                  => 0,
        'Class::Data::Accessor'     => 0.01,
     	'Carp::Clan'		    => 0,
        'DBI'                       => 1.40,
        'Module::Find'              => 0,
        'Class::Inspector'          => 0,
    },
    build_requires      => {
        'DBD::SQLite'               => 1.11,
    },
    create_makefile_pl => 'passthrough',
    create_readme      => 1,
    test_files         => [ glob('t/*.t'), glob('t/*/*.t') ],
    script_files       => [ glob('script/*') ],
);

Module::Build->new(%arguments)->create_build_script;

