use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;


my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_PG_${_}" } qw/DSN USER PASS/};

plan skip_all => <<EOM unless $dsn && $user;
Set \$ENV{DBICTEST_PG_DSN}, _USER and _PASS to run this test
( NOTE: This test drops and creates tables called 'artist', 'casecheck',
  'array_test' and 'sequence_test' as well as following sequences:
  'pkid1_seq', 'pkid2_seq' and 'nonpkid_seq''.  as well as following
  schemas: 'dbic_t_schema', 'dbic_t_schema_2', 'dbic_t_schema_3',
  'dbic_t_schema_4', and 'dbic_t_schema_5'
)
EOM

### load any test classes that are defined further down in the file via BEGIN blocks

our @test_classes; #< array that will be pushed into by test classes defined in this file
DBICTest::Schema->load_classes( map {s/.+:://;$_} @test_classes ) if @test_classes;


###  pre-connect tests (keep each test separate as to make sure rebless() runs)
{
  my $s = DBICTest::Schema->connect($dsn, $user, $pass);

  ok (!$s->storage->_dbh, 'definitely not connected');

  # Check that datetime_parser returns correctly before we explicitly connect.
  SKIP: {
      eval { require DateTime::Format::Pg };
      skip "DateTime::Format::Pg required", 2 if $@;

      my $store = ref $s->storage;
      is($store, 'DBIx::Class::Storage::DBI', 'Started with generic storage');

      my $parser = $s->storage->datetime_parser;
      is( $parser, 'DateTime::Format::Pg', 'datetime_parser is as expected');
  }

  ok (!$s->storage->_dbh, 'still not connected');
}
{
  my $s = DBICTest::Schema->connect($dsn, $user, $pass);
  # make sure sqlt_type overrides work (::Storage::DBI::Pg does this)
  ok (!$s->storage->_dbh, 'definitely not connected');
  is ($s->storage->sqlt_type, 'PostgreSQL', 'sqlt_type correct pre-connection');
  ok (!$s->storage->_dbh, 'still not connected');
}

### connect, create postgres-specific test schema

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

drop_test_schema($schema);
create_test_schema($schema);

### begin main tests


# run a BIG bunch of tests for last-insert-id / Auto-PK / sequence
# discovery
run_apk_tests($schema); #< older set of auto-pk tests
run_extended_apk_tests($schema); #< new extended set of auto-pk tests





### type_info tests

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
    'rank' => {
        'data_type' => 'integer',
        'is_nullable' => 0,
        'size' => 4,
        'default_value' => 13,

    },
    'charfield' => {
        'data_type' => 'character',
        'is_nullable' => 1,
        'size' => 10,
        'default_value' => undef,
    },
    'arrayfield' => {
        'data_type' => 'integer[]',
        'is_nullable' => 1,
        'size' => undef,
        'default_value' => undef,
    },
};

my $type_info = $schema->storage->columns_info_for('dbic_t_schema.artist');
my $artistid_defval = delete $type_info->{artistid}->{default_value};
like($artistid_defval,
     qr/^nextval\('([^\.]*\.){0,1}artist_artistid_seq'::(?:text|regclass)\)/,
     'columns_info_for - sequence matches Pg get_autoinc_seq expectations');
is_deeply($type_info, $test_type_info,
          'columns_info_for - column data types');




####### Array tests

BEGIN {
  package DBICTest::Schema::ArrayTest;
  push @main::test_classes, __PACKAGE__;

  use strict;
  use warnings;
  use base 'DBIx::Class::Core';

  __PACKAGE__->table('dbic_t_schema.array_test');
  __PACKAGE__->add_columns(qw/id arrayfield/);
  __PACKAGE__->column_info_from_storage(1);
  __PACKAGE__->set_primary_key('id');

}
SKIP: {
  skip "Need DBD::Pg 2.9.2 or newer for array tests", 4 if $DBD::Pg::VERSION < 2.009002;

  lives_ok {
    $schema->resultset('ArrayTest')->create({
      arrayfield => [1, 2],
    });
  } 'inserting arrayref as pg array data';

  lives_ok {
    $schema->resultset('ArrayTest')->update({
      arrayfield => [3, 4],
    });
  } 'updating arrayref as pg array data';

  $schema->resultset('ArrayTest')->create({
    arrayfield => [5, 6],
  });

  my $count;
  lives_ok {
    $count = $schema->resultset('ArrayTest')->search({
      arrayfield => \[ '= ?' => [arrayfield => [3, 4]] ],   #Todo anything less ugly than this?
    })->count;
  } 'comparing arrayref to pg array data does not blow up';
  is($count, 1, 'comparing arrayref to pg array data gives correct result');
}



########## Case check

BEGIN {
  package DBICTest::Schema::Casecheck;
  push @main::test_classes, __PACKAGE__;

  use strict;
  use warnings;
  use base 'DBIx::Class::Core';

  __PACKAGE__->table('dbic_t_schema.casecheck');
  __PACKAGE__->add_columns(qw/id name NAME uc_name/);
  __PACKAGE__->column_info_from_storage(1);
  __PACKAGE__->set_primary_key('id');
}

my $name_info = $schema->source('Casecheck')->column_info( 'name' );
is( $name_info->{size}, 1, "Case sensitive matching info for 'name'" );

my $NAME_info = $schema->source('Casecheck')->column_info( 'NAME' );
is( $NAME_info->{size}, 2, "Case sensitive matching info for 'NAME'" );

my $uc_name_info = $schema->source('Casecheck')->column_info( 'uc_name' );
is( $uc_name_info->{size}, 3, "Case insensitive matching info for 'uc_name'" );




## Test SELECT ... FOR UPDATE

SKIP: {
    if(eval "require Sys::SigAction" && !$@) {
        Sys::SigAction->import( 'set_sig_handler' );
    }
    else {
      skip "Sys::SigAction is not available", 6;
    }

    my ($timed_out, $artist2);

    for my $t (
      {
        # Make sure that an error was raised, and that the update failed
        update_lock => 1,
        test_sub => sub {
          ok($timed_out, "update from second schema times out");
          ok($artist2->is_column_changed('name'), "'name' column is still dirty from second schema");
        },
      },
      {
        # Make sure that an error was NOT raised, and that the update succeeded
        update_lock => 0,
        test_sub => sub {
          ok(! $timed_out, "update from second schema DOES NOT timeout");
          ok(! $artist2->is_column_changed('name'), "'name' column is NOT dirty from second schema");
        },
      },
    ) {
      # create a new schema
      my $schema2 = DBICTest::Schema->connect($dsn, $user, $pass);
      $schema2->source("Artist")->name("dbic_t_schema.artist");

      $schema->txn_do( sub {
        my $artist = $schema->resultset('Artist')->search(
            {
                artistid => 1
            },
            $t->{update_lock} ? { for => 'update' } : {}
        )->first;
        is($artist->artistid, 1, "select returns artistid = 1");

        $timed_out = 0;
        eval {
            my $h = set_sig_handler( 'ALRM', sub { die "DBICTestTimeout" } );
            alarm(2);
            $artist2 = $schema2->resultset('Artist')->find(1);
            $artist2->name('fooey');
            $artist2->update;
            alarm(0);
        };
        $timed_out = $@ =~ /DBICTestTimeout/;
      });

      $t->{test_sub}->();
    }
}


######## other older Auto-pk tests

$schema->source("SequenceTest")->name("dbic_t_schema.sequence_test");
for (1..5) {
    my $st = $schema->resultset('SequenceTest')->create({ name => 'foo' });
    is($st->pkid1, $_, "Oracle Auto-PK without trigger: First primary key");
    is($st->pkid2, $_ + 9, "Oracle Auto-PK without trigger: Second primary key");
    is($st->nonpkid, $_ + 19, "Oracle Auto-PK without trigger: Non-primary key");
}
my $st = $schema->resultset('SequenceTest')->create({ name => 'foo', pkid1 => 55 });
is($st->pkid1, 55, "Oracle Auto-PK without trigger: First primary key set manually");

done_testing;

exit;

END {
    return unless $schema;
    drop_test_schema($schema);
    eapk_drop_all( $schema)
};


######### SUBROUTINES

sub create_test_schema {
    my $schema = shift;
    $schema->storage->dbh_do(sub {
      my (undef,$dbh) = @_;

      local $dbh->{Warn} = 0;

      my $std_artist_table = <<EOS;
(
  artistid serial PRIMARY KEY
  , name VARCHAR(100)
  , rank INTEGER NOT NULL DEFAULT '13'
  , charfield CHAR(10)
  , arrayfield INTEGER[]
)
EOS

      $dbh->do("CREATE SCHEMA dbic_t_schema");
      $dbh->do("CREATE TABLE dbic_t_schema.artist $std_artist_table");
      $dbh->do(<<EOS);
CREATE TABLE dbic_t_schema.sequence_test (
    pkid1 integer
    , pkid2 integer
    , nonpkid integer
    , name VARCHAR(100)
    , CONSTRAINT pk PRIMARY KEY(pkid1, pkid2)
)
EOS
      $dbh->do("CREATE SEQUENCE pkid1_seq START 1 MAXVALUE 999999 MINVALUE 0");
      $dbh->do("CREATE SEQUENCE pkid2_seq START 10 MAXVALUE 999999 MINVALUE 0");
      $dbh->do("CREATE SEQUENCE nonpkid_seq START 20 MAXVALUE 999999 MINVALUE 0");
      $dbh->do(<<EOS);
CREATE TABLE dbic_t_schema.casecheck (
    id serial PRIMARY KEY
    , "name" VARCHAR(1)
    , "NAME" VARCHAR(2)
    , "UC_NAME" VARCHAR(3)
)
EOS
      $dbh->do(<<EOS);
CREATE TABLE dbic_t_schema.array_test (
    id serial PRIMARY KEY
    , arrayfield INTEGER[]
)
EOS
      $dbh->do("CREATE SCHEMA dbic_t_schema_2");
      $dbh->do("CREATE TABLE dbic_t_schema_2.artist $std_artist_table");
      $dbh->do("CREATE SCHEMA dbic_t_schema_3");
      $dbh->do("CREATE TABLE dbic_t_schema_3.artist $std_artist_table");
      $dbh->do('set search_path=dbic_t_schema,public');
      $dbh->do("CREATE SCHEMA dbic_t_schema_4");
      $dbh->do("CREATE SCHEMA dbic_t_schema_5");
      $dbh->do(<<EOS);
 CREATE TABLE dbic_t_schema_4.artist
 (
   artistid integer not null default nextval('artist_artistid_seq'::regclass) PRIMARY KEY
   , name VARCHAR(100)
   , rank INTEGER NOT NULL DEFAULT '13'
   , charfield CHAR(10)
   , arrayfield INTEGER[]
 );
EOS
      $dbh->do('set search_path=public,dbic_t_schema,dbic_t_schema_3');
      $dbh->do('create sequence public.artist_artistid_seq'); #< in the public schema
      $dbh->do(<<EOS);
 CREATE TABLE dbic_t_schema_5.artist
 (
   artistid integer not null default nextval('public.artist_artistid_seq'::regclass) PRIMARY KEY
   , name VARCHAR(100)
   , rank INTEGER NOT NULL DEFAULT '13'
   , charfield CHAR(10)
   , arrayfield INTEGER[]
 );
EOS
      $dbh->do('set search_path=dbic_t_schema,public');
  });
}



sub drop_test_schema {
    my ( $schema, $warn_exceptions ) = @_;

    $schema->storage->dbh_do(sub {
        my (undef,$dbh) = @_;

        local $dbh->{Warn} = 0;

        for my $stat (
                      'DROP SCHEMA dbic_t_schema_5 CASCADE',
                      'DROP SEQUENCE public.artist_artistid_seq',
                      'DROP SCHEMA dbic_t_schema_4 CASCADE',
                      'DROP SCHEMA dbic_t_schema CASCADE',
                      'DROP SEQUENCE pkid1_seq',
                      'DROP SEQUENCE pkid2_seq',
                      'DROP SEQUENCE nonpkid_seq',
                      'DROP SCHEMA dbic_t_schema_2 CASCADE',
                      'DROP SCHEMA dbic_t_schema_3 CASCADE',
                     ) {
            eval { $dbh->do ($stat) };
            diag $@ if $@ && $warn_exceptions;
        }
    });
}


###  auto-pk / last_insert_id / sequence discovery
sub run_apk_tests {
    my $schema = shift;

    # This is in Core now, but it's here just to test that it doesn't break
    $schema->class('Artist')->load_components('PK::Auto');
    cmp_ok( $schema->resultset('Artist')->count, '==', 0, 'this should start with an empty artist table');

    # test that auto-pk also works with the defined search path by
    # un-schema-qualifying the table name
    apk_t_set($schema,'artist');

    my $unq_new;
    lives_ok {
        $unq_new = $schema->resultset('Artist')->create({ name => 'baz' });
    } 'insert into unqualified, shadowed table succeeds';

    is($unq_new && $unq_new->artistid, 1, "and got correct artistid");

    my @test_schemas = ( [qw| dbic_t_schema_2    1  |],
                         [qw| dbic_t_schema_3    1  |],
                         [qw| dbic_t_schema_4    2  |],
                         [qw| dbic_t_schema_5    1  |],
                       );
    foreach my $t ( @test_schemas ) {
        my ($sch_name, $start_num) = @$t;
        #test with dbic_t_schema_2
        apk_t_set($schema,"$sch_name.artist");
        my $another_new;
        lives_ok {
            $another_new = $schema->resultset('Artist')->create({ name => 'Tollbooth Willy'});
            is( $another_new->artistid,$start_num, "got correct artistid for $sch_name")
                or diag "USED SEQUENCE: ".($schema->source('Artist')->column_info('artistid')->{sequence} || '<none>');
        } "$sch_name liid 1 did not die"
            or diag "USED SEQUENCE: ".($schema->source('Artist')->column_info('artistid')->{sequence} || '<none>');
        lives_ok {
            $another_new = $schema->resultset('Artist')->create({ name => 'Adam Sandler'});
            is( $another_new->artistid,$start_num+1, "got correct artistid for $sch_name")
                or diag "USED SEQUENCE: ".($schema->source('Artist')->column_info('artistid')->{sequence} || '<none>');
        } "$sch_name liid 2 did not die"
            or diag "USED SEQUENCE: ".($schema->source('Artist')->column_info('artistid')->{sequence} || '<none>');

    }

    lives_ok {
        apk_t_set($schema,'dbic_t_schema.artist');
        my $new = $schema->resultset('Artist')->create({ name => 'foo' });
        is($new->artistid, 4, "Auto-PK worked");
        $new = $schema->resultset('Artist')->create({ name => 'bar' });
        is($new->artistid, 5, "Auto-PK worked");
    } 'old auto-pk tests did not die either';
}

# sets the artist table name and clears sequence name cache
sub apk_t_set {
    my ( $s, $n ) = @_;
    $s->source("Artist")->name($n);
    $s->source('Artist')->column_info('artistid')->{sequence} = undef; #< clear sequence name cache
}


######## EXTENDED AUTO-PK TESTS

my @eapk_id_columns;
BEGIN {
  package DBICTest::Schema::ExtAPK;
  push @main::test_classes, __PACKAGE__;

  use strict;
  use warnings;
  use base 'DBIx::Class::Core';

  __PACKAGE__->table('apk');

  @eapk_id_columns = qw( id1 id2 id3 id4 );
  __PACKAGE__->add_columns(
    map { $_ => { data_type => 'integer', is_auto_increment => 1 } }
       @eapk_id_columns
  );

  __PACKAGE__->set_primary_key('id2'); #< note the SECOND column is
                                       #the primary key
}

my @eapk_schemas;
BEGIN{ @eapk_schemas = map "dbic_apk_$_", 0..5 }
my %seqs; #< hash of schema.table.col => currval of its (DBIC) primary key sequence

sub run_extended_apk_tests {
  my $schema = shift;

  #save the search path and reset it at the end
  my $search_path_save = eapk_get_search_path($schema);

  eapk_drop_all($schema);

  # make the test schemas and sequences
  $schema->storage->dbh_do(sub {
    my ( undef, $dbh ) = @_;

    $dbh->do("CREATE SCHEMA $_")
        for @eapk_schemas;

    $dbh->do("CREATE SEQUENCE $eapk_schemas[5].fooseq");
    $dbh->do("SELECT setval('$eapk_schemas[5].fooseq',400)");
    $seqs{"$eapk_schemas[1].apk.id2"} = 400;

    $dbh->do("CREATE SEQUENCE $eapk_schemas[4].fooseq");
    $dbh->do("SELECT setval('$eapk_schemas[4].fooseq',300)");
    $seqs{"$eapk_schemas[3].apk.id2"} = 300;

    $dbh->do("CREATE SEQUENCE $eapk_schemas[3].fooseq");
    $dbh->do("SELECT setval('$eapk_schemas[3].fooseq',200)");
    $seqs{"$eapk_schemas[4].apk.id2"} = 200;

    $dbh->do("SET search_path = ".join ',', reverse @eapk_schemas );
  });

  # clear our search_path cache
  $schema->storage->{_pg_search_path} = undef;

  eapk_create( $schema,
               with_search_path => [0,1],
             );
  eapk_create( $schema,
               with_search_path => [1,0,'public'],
               nextval => "$eapk_schemas[5].fooseq",
             );
  eapk_create( $schema,
               with_search_path => ['public',0,1],
               qualify_table => 2,
             );
  eapk_create( $schema,
               with_search_path => [3,1,0,'public'],
               nextval => "$eapk_schemas[4].fooseq",
             );
  eapk_create( $schema,
               with_search_path => [3,1,0,'public'],
               nextval => "$eapk_schemas[3].fooseq",
               qualify_table => 4,
             );

  eapk_poke( $schema );
  eapk_poke( $schema, 0 );
  eapk_poke( $schema, 2 );
  eapk_poke( $schema, 4 );
  eapk_poke( $schema, 1 );
  eapk_poke( $schema, 0 );
  eapk_poke( $schema, 1 );
  eapk_poke( $schema );
  eapk_poke( $schema, 4 );
  eapk_poke( $schema, 3 );
  eapk_poke( $schema, 1 );
  eapk_poke( $schema, 2 );
  eapk_poke( $schema, 0 );

  # set our search path back
  eapk_set_search_path( $schema, @$search_path_save );
}

# do a DBIC create on the apk table in the given schema number (which is an
# index of @eapk_schemas)

sub eapk_poke {
  my ($s, $schema_num) = @_;

  my $schema_name = defined $schema_num
      ? $eapk_schemas[$schema_num]
      : '';

  my $schema_name_actual = $schema_name || eapk_find_visible_schema($s);

  $s->source('ExtAPK')->name($schema_name ? $schema_name.'.apk' : 'apk');
  #< clear sequence name cache
  $s->source('ExtAPK')->column_info($_)->{sequence} = undef
      for @eapk_id_columns;

  no warnings 'uninitialized';
  lives_ok {
    my $new;
    for my $inc (1,2,3) {
      $new = $schema->resultset('ExtAPK')->create({ id1 => 1});
      my $proper_seqval = ++$seqs{"$schema_name_actual.apk.id2"};
      is( $new->id2, $proper_seqval, "$schema_name_actual.apk.id2 correct inc $inc" )
          or eapk_seq_diag($s,$schema_name);
      $new->discard_changes;
      is( $new->id1, 1 );
      for my $id ('id3','id4') {
        my $proper_seqval = ++$seqs{"$schema_name_actual.apk.$id"};
        is( $new->$id, $proper_seqval, "$schema_name_actual.apk.$id correct inc $inc" )
            or eapk_seq_diag($s,$schema_name);
      }
    }
  } "create in schema '$schema_name' lives"
      or eapk_seq_diag($s,$schema_name);
}

# print diagnostic info on which sequences were found in the ExtAPK
# class
sub eapk_seq_diag {
    my $s = shift;
    my $schema = shift || eapk_find_visible_schema($s);

    diag "$schema.apk sequences: ",
        join(', ',
             map "$_:".($s->source('ExtAPK')->column_info($_)->{sequence} || '<none>'),
             @eapk_id_columns
            );
}

# get the postgres search path as an arrayref
sub eapk_get_search_path {
    my ( $s ) = @_;
    # cache the search path as ['schema','schema',...] in the storage
    # obj

    return $s->storage->dbh_do(sub {
        my (undef, $dbh) = @_;
        my @search_path;
        my ($sp_string) = $dbh->selectrow_array('SHOW search_path');
        while ( $sp_string =~ s/("[^"]+"|[^,]+),?// ) {
            unless( defined $1 and length $1 ) {
                die "search path sanity check failed: '$1'";
            }
            push @search_path, $1;
        }
        \@search_path
    });
}
sub eapk_set_search_path {
    my ($s,@sp) = @_;
    my $sp = join ',',@sp;
    $s->storage->dbh_do( sub { $_[1]->do("SET search_path = $sp") } );
}

# create the apk table in the given schema, can set whether the table name is qualified, what the nextval is for the second ID
sub eapk_create {
    my ($schema, %a) = @_;

    $schema->storage->dbh_do(sub {
        my (undef,$dbh) = @_;

        my $searchpath_save;
        if ( $a{with_search_path} ) {
            ($searchpath_save) = $dbh->selectrow_array('SHOW search_path');

            my $search_path = join ',',map {/\D/ ? $_ : $eapk_schemas[$_]} @{$a{with_search_path}};

            $dbh->do("SET search_path = $search_path");
        }

        my $table_name = $a{qualify_table}
            ? ($eapk_schemas[$a{qualify_table}] || die). ".apk"
            : 'apk';
        local $_[1]->{Warn} = 0;

        my $id_def = $a{nextval}
            ? "integer not null default nextval('$a{nextval}'::regclass)"
            : 'serial';
        $dbh->do(<<EOS);
CREATE TABLE $table_name (
  id1 serial
  , id2 $id_def
  , id3 serial primary key
  , id4 serial
)
EOS

        if( $searchpath_save ) {
            $dbh->do("SET search_path = $searchpath_save");
        }
    });
}

sub eapk_drop_all {
    my ( $schema, $warn_exceptions ) = @_;

    $schema->storage->dbh_do(sub {
        my (undef,$dbh) = @_;

        local $dbh->{Warn} = 0;

        # drop the test schemas
        for (@eapk_schemas ) {
            eval{ $dbh->do("DROP SCHEMA $_ CASCADE") };
            diag $@ if $@ && $warn_exceptions;
        }


    });
}

sub eapk_find_visible_schema {
    my ($s) = @_;

    my ($schema) =
        $s->storage->dbh_do(sub {
            $_[1]->selectrow_array(<<EOS);
SELECT n.nspname
FROM pg_catalog.pg_namespace n
JOIN pg_catalog.pg_class c ON c.relnamespace = n.oid
WHERE c.relname = 'apk'
  AND pg_catalog.pg_table_is_visible(c.oid)
EOS
        });
    return $schema;
}
