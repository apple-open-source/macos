{
  package    # hide from PAUSE
    DBICTest::Schema::ArtistFQN;

  use base 'DBIx::Class::Core';

  __PACKAGE__->table(
      defined $ENV{DBICTEST_ORA_USER}
      ? $ENV{DBICTEST_ORA_USER} . '.artist'
      : 'artist'
  );
  __PACKAGE__->add_columns(
      'artistid' => {
          data_type         => 'integer',
          is_auto_increment => 1,
      },
      'name' => {
          data_type   => 'varchar',
          size        => 100,
          is_nullable => 1,
      },
  );
  __PACKAGE__->set_primary_key('artistid');

  1;
}

use strict;
use warnings;

use Test::Exception;
use Test::More;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_ORA_${_}" } qw/DSN USER PASS/};

plan skip_all => 'Set $ENV{DBICTEST_ORA_DSN}, _USER and _PASS to run this test. ' .
  'Warning: This test drops and creates tables called \'artist\', \'cd\', \'track\' and \'sequence_test\''.
  ' as well as following sequences: \'pkid1_seq\', \'pkid2_seq\' and \'nonpkid_seq\''
  unless ($dsn && $user && $pass);

DBICTest::Schema->load_classes('ArtistFQN');
my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

my $dbh = $schema->storage->dbh;

eval {
  $dbh->do("DROP SEQUENCE artist_seq");
  $dbh->do("DROP SEQUENCE cd_seq");
  $dbh->do("DROP SEQUENCE pkid1_seq");
  $dbh->do("DROP SEQUENCE pkid2_seq");
  $dbh->do("DROP SEQUENCE nonpkid_seq");
  $dbh->do("DROP TABLE artist");
  $dbh->do("DROP TABLE sequence_test");
  $dbh->do("DROP TABLE track");
  $dbh->do("DROP TABLE cd");
};
$dbh->do("CREATE SEQUENCE artist_seq START WITH 1 MAXVALUE 999999 MINVALUE 0");
$dbh->do("CREATE SEQUENCE cd_seq START WITH 1 MAXVALUE 999999 MINVALUE 0");
$dbh->do("CREATE SEQUENCE pkid1_seq START WITH 1 MAXVALUE 999999 MINVALUE 0");
$dbh->do("CREATE SEQUENCE pkid2_seq START WITH 10 MAXVALUE 999999 MINVALUE 0");
$dbh->do("CREATE SEQUENCE nonpkid_seq START WITH 20 MAXVALUE 999999 MINVALUE 0");

$dbh->do("CREATE TABLE artist (artistid NUMBER(12), name VARCHAR(255), rank NUMBER(38), charfield VARCHAR2(10))");
$dbh->do("ALTER TABLE artist ADD (CONSTRAINT artist_pk PRIMARY KEY (artistid))");

$dbh->do("CREATE TABLE sequence_test (pkid1 NUMBER(12), pkid2 NUMBER(12), nonpkid NUMBER(12), name VARCHAR(255))");
$dbh->do("ALTER TABLE sequence_test ADD (CONSTRAINT sequence_test_constraint PRIMARY KEY (pkid1, pkid2))");

$dbh->do("CREATE TABLE cd (cdid NUMBER(12), artist NUMBER(12), title VARCHAR(255), year VARCHAR(4), genreid NUMBER(12), single_track NUMBER(12))");
$dbh->do("ALTER TABLE cd ADD (CONSTRAINT cd_pk PRIMARY KEY (cdid))");

$dbh->do("CREATE TABLE track (trackid NUMBER(12), cd NUMBER(12) REFERENCES cd(cdid) DEFERRABLE, position NUMBER(12), title VARCHAR(255), last_updated_on DATE, last_updated_at DATE, small_dt DATE)");

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
$dbh->do(qq{
  CREATE OR REPLACE TRIGGER cd_insert_trg
  BEFORE INSERT ON cd
  FOR EACH ROW
  BEGIN
    IF :new.cdid IS NULL THEN
      SELECT cd_seq.nextval
      INTO :new.cdid
      FROM DUAL;
    END IF;
  END;
});

{
    # Swiped from t/bindtype_columns.t to avoid creating my own Resultset.

    local $SIG{__WARN__} = sub {};
    eval { $dbh->do('DROP TABLE bindtype_test') };

    $dbh->do(qq[
        CREATE TABLE bindtype_test
        (
            id              integer      NOT NULL   PRIMARY KEY,
            bytea           integer      NULL,
            blob            blob         NULL,
            clob            clob         NULL
        )
    ],{ RaiseError => 1, PrintError => 1 });
}

# This is in Core now, but it's here just to test that it doesn't break
$schema->class('Artist')->load_components('PK::Auto');
# These are compat shims for PK::Auto...
$schema->class('CD')->load_components('PK::Auto::Oracle');
$schema->class('Track')->load_components('PK::Auto::Oracle');

# test primary key handling
my $new = $schema->resultset('Artist')->create({ name => 'foo' });
is($new->artistid, 1, "Oracle Auto-PK worked");

my $cd = $schema->resultset('CD')->create({ artist => 1, title => 'EP C', year => '2003' });
is($cd->cdid, 1, "Oracle Auto-PK worked - using scalar ref as table name");

# test again with fully-qualified table name
$new = $schema->resultset('ArtistFQN')->create( { name => 'bar' } );
is( $new->artistid, 2, "Oracle Auto-PK worked with fully-qualified tablename" );

# test rel names over the 30 char limit
my $query = $schema->resultset('Artist')->search({
  artistid => 1 
}, {
  prefetch => 'cds_very_very_very_long_relationship_name'
});

lives_and {
  is $query->first->cds_very_very_very_long_relationship_name->first->cdid, 1
} 'query with rel name over 30 chars survived and worked';

# rel name over 30 char limit with user condition
# This requires walking the SQLA data structure.
{
  local $TODO = 'user condition on rel longer than 30 chars';

  $query = $schema->resultset('Artist')->search({
    'cds_very_very_very_long_relationship_name.title' => 'EP C'
  }, {
    prefetch => 'cds_very_very_very_long_relationship_name'
  });

  lives_and {
    is $query->first->cds_very_very_very_long_relationship_name->first->cdid, 1
  } 'query with rel name over 30 chars and user condition survived and worked';
}

# test join with row count ambiguity

my $track = $schema->resultset('Track')->create({ trackid => 1, cd => 1,
    position => 1, title => 'Track1' });
my $tjoin = $schema->resultset('Track')->search({ 'me.title' => 'Track1'},
        { join => 'cd',
          rows => 2 }
);

ok(my $row = $tjoin->next);

is($row->title, 'Track1', "ambiguous column ok");

# check count distinct with multiple columns
my $other_track = $schema->resultset('Track')->create({ trackid => 2, cd => 1, position => 1, title => 'Track2' });

my $tcount = $schema->resultset('Track')->search(
  {},
  {
    select => [ qw/position title/ ],
    distinct => 1,
  }
);
is($tcount->count, 2, 'multiple column COUNT DISTINCT ok');

$tcount = $schema->resultset('Track')->search(
  {},
  {
    columns => [ qw/position title/ ],
    distinct => 1,
  }
);
is($tcount->count, 2, 'multiple column COUNT DISTINCT ok');

$tcount = $schema->resultset('Track')->search(
  {},
  {
     group_by => [ qw/position title/ ]
  }
);
is($tcount->count, 2, 'multiple column COUNT DISTINCT using column syntax ok');

# test LIMIT support
for (1..6) {
    $schema->resultset('Artist')->create({ name => 'Artist ' . $_ });
}
my $it = $schema->resultset('Artist')->search( {},
    { rows => 3,
      offset => 3,
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

# test with_deferred_fk_checks
lives_ok {
  $schema->storage->with_deferred_fk_checks(sub {
    $schema->resultset('Track')->create({
      trackid => 999, cd => 999, position => 1, title => 'deferred FK track'
    });
    $schema->resultset('CD')->create({
      artist => 1, cdid => 999, year => '2003', title => 'deferred FK cd'
    });
  });
} 'with_deferred_fk_checks code survived';

is eval { $schema->resultset('Track')->find(999)->title }, 'deferred FK track',
   'code in with_deferred_fk_checks worked'; 

throws_ok {
  $schema->resultset('Track')->create({
    trackid => 1, cd => 9999, position => 1, title => 'Track1'
  });
} qr/constraint/i, 'with_deferred_fk_checks is off';

# test auto increment using sequences WITHOUT triggers
for (1..5) {
    my $st = $schema->resultset('SequenceTest')->create({ name => 'foo' });
    is($st->pkid1, $_, "Oracle Auto-PK without trigger: First primary key");
    is($st->pkid2, $_ + 9, "Oracle Auto-PK without trigger: Second primary key");
    is($st->nonpkid, $_ + 19, "Oracle Auto-PK without trigger: Non-primary key");
}
my $st = $schema->resultset('SequenceTest')->create({ name => 'foo', pkid1 => 55 });
is($st->pkid1, 55, "Oracle Auto-PK without trigger: First primary key set manually");

SKIP: {
  my %binstr = ( 'small' => join('', map { chr($_) } ( 1 .. 127 )) );
  $binstr{'large'} = $binstr{'small'} x 1024;

  my $maxloblen = length $binstr{'large'};
  note "Localizing LongReadLen to $maxloblen to avoid truncation of test data";
  local $dbh->{'LongReadLen'} = $maxloblen;

  my $rs = $schema->resultset('BindType');
  my $id = 0;

  if ($DBD::Oracle::VERSION eq '1.23') {
    throws_ok { $rs->create({ id => 1, blob => $binstr{large} }) }
      qr/broken/,
      'throws on blob insert with DBD::Oracle == 1.23';

    skip 'buggy BLOB support in DBD::Oracle 1.23', 7;
  }

  foreach my $type (qw( blob clob )) {
    foreach my $size (qw( small large )) {
      $id++;

      lives_ok { $rs->create( { 'id' => $id, $type => $binstr{$size} } ) }
      "inserted $size $type without dying";

      ok($rs->find($id)->$type eq $binstr{$size}, "verified inserted $size $type" );
    }
  }
}

done_testing;

# clean up our mess
END {
    if($schema && ($dbh = $schema->storage->dbh)) {
        $dbh->do("DROP SEQUENCE artist_seq");
        $dbh->do("DROP SEQUENCE cd_seq");
        $dbh->do("DROP SEQUENCE pkid1_seq");
        $dbh->do("DROP SEQUENCE pkid2_seq");
        $dbh->do("DROP SEQUENCE nonpkid_seq");
        $dbh->do("DROP TABLE artist");
        $dbh->do("DROP TABLE sequence_test");
        $dbh->do("DROP TABLE track");
        $dbh->do("DROP TABLE cd");
        $dbh->do("DROP TABLE bindtype_test");
    }
}

