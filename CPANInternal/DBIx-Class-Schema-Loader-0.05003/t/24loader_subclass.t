use strict;
use warnings;
use Test::More;
use lib qw(t/lib);
use make_dbictest_db;

my %loader_class = ( 'TestLoaderSubclass' => 'TestLoaderSubclass',
                     '::DBI::SQLite'      => 'DBIx::Class::Schema::Loader::DBI::SQLite'
                   );

my %invocations = (
    loader_class => sub {
        package DBICTest::Schema::1;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->naming('current');
        __PACKAGE__->loader_class(shift);
        __PACKAGE__->connect($make_dbictest_db::dsn);
    },
    connect_info => sub {
        package DBICTeset::Schema::2;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->naming('current');
        __PACKAGE__->connect($make_dbictest_db::dsn, { loader_class => shift });
    },
    make_schema_at => sub {
        use DBIx::Class::Schema::Loader qw/ make_schema_at /;
        make_schema_at(
            'DBICTeset::Schema::3',
            { naming => 'current' },
            [ $make_dbictest_db::dsn, { loader_class => shift } ]
        );
    }
);

# one test per invocation/class combo
plan tests => keys(%invocations) * keys(%loader_class);

while (my ($style,$subref) = each %invocations) {
    while (my ($arg, $class) = each %loader_class) {
        my $schema = $subref->($arg);
        $schema = $schema->clone unless ref $schema;
        isa_ok($schema->_loader, $class, "$style($arg)");
    }
}
