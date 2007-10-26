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

  __PACKAGE__->load_components(qw/PK::Auto Core/);
  __PACKAGE__->table('casecheck');
  __PACKAGE__->add_columns(qw/id name NAME uc_name/);
  __PACKAGE__->set_primary_key('id');

}

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_PG_${_}" } qw/DSN USER PASS/};

#warn "$dsn $user $pass";

plan skip_all => 'Set $ENV{DBICTEST_PG_DSN}, _USER and _PASS to run this test'
 . ' (note: creates and drops tables named artist and casecheck!)' unless ($dsn && $user);

plan tests => 8;

DBICTest::Schema->load_classes( 'Casecheck' );
DBICTest::Schema->compose_connection('PgTest' => $dsn, $user, $pass);

my $dbh = PgTest->schema->storage->dbh;
PgTest->schema->source("Artist")->name("testschema.artist");
$dbh->do("CREATE SCHEMA testschema;");
$dbh->do("CREATE TABLE testschema.artist (artistid serial PRIMARY KEY, name VARCHAR(100), charfield CHAR(10));");
ok ( $dbh->do('CREATE TABLE testschema.casecheck (id serial PRIMARY KEY, "name" VARCHAR(1), "NAME" VARCHAR(2), "UC_NAME" VARCHAR(3));'), 'Creation of casecheck table');

PgTest::Artist->load_components('PK::Auto');

my $new = PgTest::Artist->create({ name => 'foo' });

is($new->artistid, 1, "Auto-PK worked");

$new = PgTest::Artist->create({ name => 'bar' });

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


my $type_info = PgTest->schema->storage->columns_info_for('testschema.artist');
my $artistid_defval = delete $type_info->{artistid}->{default_value};
like($artistid_defval,
     qr/^nextval\('([^\.]*\.){0,1}artist_artistid_seq'::(?:text|regclass)\)/,
     'columns_info_for - sequence matches Pg get_autoinc_seq expectations');
is_deeply($type_info, $test_type_info,
          'columns_info_for - column data types');

my $name_info = PgTest::Casecheck->column_info( 'name' );
is( $name_info->{size}, 1, "Case sensitive matching info for 'name'" );

my $NAME_info = PgTest::Casecheck->column_info( 'NAME' );
is( $NAME_info->{size}, 2, "Case sensitive matching info for 'NAME'" );

my $uc_name_info = PgTest::Casecheck->column_info( 'uc_name' );
is( $uc_name_info->{size}, 3, "Case insensitive matching info for 'uc_name'" );

$dbh->do("DROP TABLE testschema.artist;");
$dbh->do("DROP TABLE testschema.casecheck;");
$dbh->do("DROP SCHEMA testschema;");

