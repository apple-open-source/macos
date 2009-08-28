use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

{
  package DBICTest::Schema::Casecheck;

  use strict;
  use warnings;
  use base 'DBIx::Class';

  __PACKAGE__->load_components(qw/Core/);
  __PACKAGE__->table('casecheck');
  __PACKAGE__->add_columns(qw/id name NAME uc_name/);
  __PACKAGE__->column_info_from_storage(1);
  __PACKAGE__->set_primary_key('id');

}

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_PG_${_}" } qw/DSN USER PASS/};

#warn "$dsn $user $pass";

plan skip_all => 'Set $ENV{DBICTEST_PG_DSN}, _USER and _PASS to run this test'
 . ' (note: creates and drops tables named artist and casecheck!)' unless ($dsn && $user);

plan tests => 16;

DBICTest::Schema->load_classes( 'Casecheck' );
my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

# Check that datetime_parser returns correctly before we explicitly connect.
SKIP: {
    eval { require DateTime::Format::Pg };
    skip "DateTime::Format::Pg required", 2 if $@;

    my $store = ref $schema->storage;
    is($store, 'DBIx::Class::Storage::DBI', 'Started with generic storage');

    my $parser = $schema->storage->datetime_parser;
    is( $parser, 'DateTime::Format::Pg', 'datetime_parser is as expected');
}

my $dbh = $schema->storage->dbh;
$schema->source("Artist")->name("testschema.artist");
$dbh->do("CREATE SCHEMA testschema;");
$dbh->do("CREATE TABLE testschema.artist (artistid serial PRIMARY KEY, name VARCHAR(100), charfield CHAR(10));");
ok ( $dbh->do('CREATE TABLE testschema.casecheck (id serial PRIMARY KEY, "name" VARCHAR(1), "NAME" VARCHAR(2), "UC_NAME" VARCHAR(3));'), 'Creation of casecheck table');

# This is in Core now, but it's here just to test that it doesn't break
$schema->class('Artist')->load_components('PK::Auto');

my $new = $schema->resultset('Artist')->create({ name => 'foo' });

is($new->artistid, 1, "Auto-PK worked");

$new = $schema->resultset('Artist')->create({ name => 'bar' });

is($new->artistid, 2, "Auto-PK worked");

my $test_type_info = {
    'artistid' => {
        'data_type' => 'integer',
        'is_nullable' => 0,
        'size' => 4,
    },
    'name' => {
        'data_type' => 'character varying',
        'is_nullable' => 1,
        'size' => 100,
        'default_value' => undef,
    },
    'charfield' => {
        'data_type' => 'character',
        'is_nullable' => 1,
        'size' => 10,
        'default_value' => undef,
    },
};


my $type_info = $schema->storage->columns_info_for('testschema.artist');
my $artistid_defval = delete $type_info->{artistid}->{default_value};
like($artistid_defval,
     qr/^nextval\('([^\.]*\.){0,1}artist_artistid_seq'::(?:text|regclass)\)/,
     'columns_info_for - sequence matches Pg get_autoinc_seq expectations');
is_deeply($type_info, $test_type_info,
          'columns_info_for - column data types');

my $name_info = $schema->source('Casecheck')->column_info( 'name' );
is( $name_info->{size}, 1, "Case sensitive matching info for 'name'" );

my $NAME_info = $schema->source('Casecheck')->column_info( 'NAME' );
is( $NAME_info->{size}, 2, "Case sensitive matching info for 'NAME'" );

my $uc_name_info = $schema->source('Casecheck')->column_info( 'uc_name' );
is( $uc_name_info->{size}, 3, "Case insensitive matching info for 'uc_name'" );

# Test SELECT ... FOR UPDATE
my $HaveSysSigAction = eval "require Sys::SigAction" && !$@;
if ($HaveSysSigAction) {
    Sys::SigAction->import( 'set_sig_handler' );
}

SKIP: {
    skip "Sys::SigAction is not available", 3 unless $HaveSysSigAction;
    # create a new schema
    my $schema2 = DBICTest::Schema->connect($dsn, $user, $pass);
    $schema2->source("Artist")->name("testschema.artist");

    $schema->txn_do( sub {
        my $artist = $schema->resultset('Artist')->search(
            {
                artistid => 1
            },
            {
                for => 'update'
            }
        )->first;
        is($artist->artistid, 1, "select for update returns artistid = 1");

        my $artist_from_schema2;
        my $error_ok = 0;
        eval {
            my $h = set_sig_handler( 'ALRM', sub { die "DBICTestTimeout" } );
            alarm(2);
            $artist_from_schema2 = $schema2->resultset('Artist')->find(1);
            $artist_from_schema2->name('fooey');
            $artist_from_schema2->update;
            alarm(0);
        };
        if (my $e = $@) {
            $error_ok = $e =~ /DBICTestTimeout/;
        }

        # Make sure that an error was raised, and that the update failed
        ok($error_ok, "update from second schema times out");
        ok($artist_from_schema2->is_column_changed('name'), "'name' column is still dirty from second schema");
    });
}

SKIP: {
    skip "Sys::SigAction is not available", 3 unless $HaveSysSigAction;
    # create a new schema
    my $schema2 = DBICTest::Schema->connect($dsn, $user, $pass);
    $schema2->source("Artist")->name("testschema.artist");

    $schema->txn_do( sub {
        my $artist = $schema->resultset('Artist')->search(
            {
                artistid => 1
            },
        )->first;
        is($artist->artistid, 1, "select for update returns artistid = 1");

        my $artist_from_schema2;
        my $error_ok = 0;
        eval {
            my $h = set_sig_handler( 'ALRM', sub { die "DBICTestTimeout" } );
            alarm(2);
            $artist_from_schema2 = $schema2->resultset('Artist')->find(1);
            $artist_from_schema2->name('fooey');
            $artist_from_schema2->update;
            alarm(0);
        };
        if (my $e = $@) {
            $error_ok = $e =~ /DBICTestTimeout/;
        }

        # Make sure that an error was NOT raised, and that the update succeeded
        ok(! $error_ok, "update from second schema DOES NOT timeout");
        ok(! $artist_from_schema2->is_column_changed('name'), "'name' column is NOT dirty from second schema");
    });
}

END {
    if($dbh) {
        $dbh->do("DROP TABLE testschema.artist;");
        $dbh->do("DROP TABLE testschema.casecheck;");
        $dbh->do("DROP SCHEMA testschema;");
    }
}

