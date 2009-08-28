
use inc::Module::Install 0.65;

name           'DBIx-Class-Schema-Loader';
all_from       'lib/DBIx/Class/Schema/Loader.pm';

test_requires 'Test::More'  => '0.47';
test_requires 'DBI'         => '1.56';
test_requires 'DBD::SQLite' => '1.12';
test_requires 'File::Path'  => 0;

requires 'File::Spec'                  => 0;
requires 'Scalar::Util'                => 0;
requires 'Data::Dump'                  => '1.06';
requires 'UNIVERSAL::require'          => '0.11';
requires 'Lingua::EN::Inflect::Number' => '1.1';
requires 'Text::Balanced'              => 0;
requires 'Digest::MD5'                 => '2.36';
requires 'Class::Accessor::Fast'       => '0.30';
requires 'Class::Data::Accessor'       => '0.03';
requires 'Class::C3'                   => '0.18';
requires 'Carp::Clan'                  => 0;
requires 'Class::Inspector'            => 0;
requires 'DBIx::Class'                 => '0.07006';

# This is my manual hack for better feature control
#  If you want to change the default answer for a feature,
#  set the appropriate environment variable, like
#  DBIC_FEATURE_MYSQL.  If you want the defaults to
#  apply automatically without asking any questions,
#  set DBIC_FEATURE_NOQUESTIONS.  Hopefully this will
#  save someone some pain when trying to automate
#  the installation of this software.

# Maintainer shouldn't set these, as they would affect
# the META.yml shipped to CPAN.

my $_features = [
    SQLITE => {
        label => 'SQLite Support',
        def   => $ENV{DBIC_FEATURE_SQLITE} || 0,
        deps  => [
            'DBI'         => '1.56',
            'DBD::SQLite' => '1.12',
        ],
    },
    MYSQL => {
        label => 'MySQL Support',
        def   => $ENV{DBIC_FEATURE_MYSQL} || 0,
        deps  => [
            'DBI'         => '1.56',
            'DBD::mysql'  => '4.004',
        ],
    },
    PG => {
        label => 'PostgreSQL Support',
        def   => $ENV{DBIC_FEATURE_PG} || 0,
        deps  => [
            'DBI'         => '1.56',
            'DBD::Pg'     => '1.49', # Soon to be 1.50
        ],
    },
    DB2 => {
        label => 'DB2 Support',
        def   => $ENV{DBIC_FEATURE_DB2} || 0,
        deps  => [
            'DBI'         => '1.56',
            'DBD::DB2'    => '1.0',
        ],
    },
    ORACLE => {
        label => 'Oracle Support (experimental)',
        def   => $ENV{DBIC_FEATURE_ORACLE} || 0,
        deps  => [
            'DBI'         => '1.56',
            'DBD::Oracle' => '0.19',
        ],
    },
];

for(my $i = 0; $i <= $#$_features - 1; $i += 2) {
    my $name = $_features->[$i];
    my $attrs = $_features->[$i+1];

    if($ENV{DBIC_FEATURE_NOQUESTIONS}) {
        if($attrs->{def}) {
            requires @{$attrs->{deps}};
        }
    }
    else {
        feature $attrs->{label} =>
            -default => $attrs->{def},
            @{$attrs->{deps}};
    }
}

# Rebuild README for maintainers
if(-e 'MANIFEST.SKIP') {
    system("pod2text lib/DBIx/Class/Schema/Loader.pm > README");
}

auto_provides;
auto_install;
WriteAll;
