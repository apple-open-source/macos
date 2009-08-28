use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_ORA_${_}" } qw/DSN USER PASS/};

plan skip_all => 'Set $ENV{DBICTEST_ORA_DSN}, _USER and _PASS to run this test. ' .
  'Warning: This test drops and creates tables called \'artist\', \'cd\' and \'track\''
  unless ($dsn && $user && $pass);

plan tests => 7;

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

my $dbh = $schema->storage->dbh;

eval {
  $dbh->do("DROP SEQUENCE artist_seq");
  $dbh->do("DROP TABLE artist");
  $dbh->do("DROP TABLE cd");
  $dbh->do("DROP TABLE track");
};
$dbh->do("CREATE SEQUENCE artist_seq START WITH 1 MAXVALUE 999999 MINVALUE 0");
$dbh->do("CREATE TABLE artist (artistid NUMBER(12), name VARCHAR(255))");
$dbh->do("CREATE TABLE cd (cdid NUMBER(12), artist NUMBER(12), title VARCHAR(255), year VARCHAR(4))");
$dbh->do("CREATE TABLE track (trackid NUMBER(12), cd NUMBER(12), position NUMBER(12), title VARCHAR(255), last_updated_on DATE)");

$dbh->do("ALTER TABLE artist ADD (CONSTRAINT artist_pk PRIMARY KEY (artistid))");
$dbh->do(qq{
  CREATE OR REPLACE TRIGGER artist_insert_trg
  BEFORE INSERT ON artist
  FOR EACH ROW
  BEGIN
    IF :new.artistid IS NULL THEN
      SELECT artist_seq.nextval
      INTO :new.artistid
      FROM DUAL;
    END IF;
  END;
});

# This is in Core now, but it's here just to test that it doesn't break
$schema->class('Artist')->load_components('PK::Auto');
# These are compat shims for PK::Auto...
$schema->class('CD')->load_components('PK::Auto::Oracle');
$schema->class('Track')->load_components('PK::Auto::Oracle');

# test primary key handling
my $new = $schema->resultset('Artist')->create({ name => 'foo' });
is($new->artistid, 1, "Oracle Auto-PK worked");

# test join with row count ambiguity
my $cd = $schema->resultset('CD')->create({ cdid => 1, artist => 1, title => 'EP C', year => '2003' });
my $track = $schema->resultset('Track')->create({ trackid => 1, cd => 1, position => 1, title => 'Track1' });
my $tjoin = $schema->resultset('Track')->search({ 'me.title' => 'Track1'},
        { join => 'cd',
          rows => 2 }
);

is($tjoin->next->title, 'Track1', "ambiguous column ok");

# check count distinct with multiple columns
my $other_track = $schema->resultset('Track')->create({ trackid => 2, cd => 1, position => 1, title => 'Track2' });
my $tcount = $schema->resultset('Track')->search(
    {},
    {
        select => [{count => {distinct => ['position', 'title']}}],
        as => ['count']
    }
  );

is($tcount->next->get_column('count'), 2, "multiple column select distinct ok");

# test LIMIT support
for (1..6) {
    $schema->resultset('Artist')->create({ name => 'Artist ' . $_ });
}
my $it = $schema->resultset('Artist')->search( {},
    { rows => 3,
      offset => 2,
      order_by => 'artistid' }
);
is( $it->count, 3, "LIMIT count ok" );
is( $it->next->name, "Artist 2", "iterator->next ok" );
$it->next;
$it->next;
is( $it->next, undef, "next past end of resultset ok" );

{
  my $rs = $schema->resultset('Track')->search( undef, { columns=>[qw/trackid position/], group_by=> [ qw/trackid position/ ] , rows => 2, offset=>1 });
  my @results = $rs->all;
  is( scalar @results, 1, "Group by with limit OK" );
}

# clean up our mess
END {
    if($dbh) {
        $dbh->do("DROP SEQUENCE artist_seq");
        $dbh->do("DROP TABLE artist");
        $dbh->do("DROP TABLE cd");
        $dbh->do("DROP TABLE track");
    }
}

