#!perl -w
$|=1;

use strict;

use DBI;

use Test::More;

BEGIN {
    if ($DBI::PurePerl) {
        plan skip_all => 'profiling not supported for DBI::PurePerl';
    }
    else {
        plan tests => 31;
    }
}

BEGIN {
    use_ok( 'DBI::ProfileDumper' );
    use_ok( 'DBI::ProfileData' );
}

my $sql = "select mode,size,name from ?";

my $dbh = DBI->connect("dbi:ExampleP:", '', '', 
                       { RaiseError=>1, Profile=>"6/DBI::ProfileDumper" });
isa_ok( $dbh, 'DBI::db', 'Created connection' );

# do a little work, but enough to ensure we don't get 0's on systems with low res timers
foreach (1..6) {
  $dbh->do("set dummy=$_");
  my $sth = $dbh->prepare($sql);
  for my $loop (1..50) {  
    $sth->execute(".");
    $sth->fetchrow_hashref;
    $sth->finish;
  }
  $sth->{Profile}->flush_to_disk();
}
$dbh->disconnect;
undef $dbh;


# wrote the profile to disk?
ok(-s "dbi.prof", "Profile written to disk, non-zero size" );

# load up
my $prof = DBI::ProfileData->new(
    Filter => sub {
        my ($path_ref, $data_ref) = @_;
        $path_ref->[0] =~ s/set dummy=\d/set dummy=N/;
    },
);
isa_ok( $prof, 'DBI::ProfileData' );
cmp_ok( $prof->count, '>=', 3, 'At least 3 profile data items' );

# try a few sorts
my $nodes = $prof->nodes;
$prof->sort(field => "longest");
my $longest = $nodes->[0][4];
ok($longest);
$prof->sort(field => "longest", reverse => 1);
cmp_ok( $nodes->[0][4], '<', $longest );

$prof->sort(field => "count");
my $most = $nodes->[0];
ok($most);
$prof->sort(field => "count", reverse => 1);
cmp_ok( $nodes->[0][0], '<', $most->[0] );

# remove the top count and make sure it's gone
my $clone = $prof->clone();
isa_ok( $clone, 'DBI::ProfileData' );
$clone->sort(field => "count");
ok($clone->exclude(key1 => $most->[7]));

# compare keys of the new first element and the old one to make sure
# exclude works
ok($clone->nodes()->[0][7] ne $most->[7] &&
   $clone->nodes()->[0][8] ne $most->[8]);

# there can only be one
$clone = $prof->clone();
isa_ok( $clone, 'DBI::ProfileData' );
ok($clone->match(key1 => $clone->nodes->[0][7]));
ok($clone->match(key2 => $clone->nodes->[0][8]));
ok($clone->count == 1);

# take a look through Data
my $Data = $prof->Data;
print "SQL: $_\n" for keys %$Data;
ok(exists($Data->{$sql}));
ok(exists($Data->{$sql}{execute}));

# did the Filter convert set dummy=1 (etc) into set dummy=N?
ok(exists($Data->{"set dummy=N"}));

# test escaping of \n and \r in keys
$dbh = DBI->connect("dbi:ExampleP:", '', '', 
                    { RaiseError=>1, Profile=>"6/DBI::ProfileDumper" });
isa_ok( $dbh, 'DBI::db', 'Created connection' );

my $sql2 = 'select size from . where name = "LITERAL: \r\n"';
my $sql3 = "select size from . where name = \"EXPANDED: \r\n\"";

# do a little work
foreach (1,2,3) {
  my $sth2 = $dbh->prepare($sql2);
  isa_ok( $sth2, 'DBI::st' );
  $sth2->execute();
  $sth2->fetchrow_hashref;
  $sth2->finish;
  my $sth3 = $dbh->prepare($sql3);
  isa_ok( $sth3, 'DBI::st' );
  $sth3->execute();
  $sth3->fetchrow_hashref;
  $sth3->finish;
}
$dbh->disconnect;
undef $dbh;

# load dbi.prof
$prof = DBI::ProfileData->new( DeleteFiles => 1 );
isa_ok( $prof, 'DBI::ProfileData' );

ok(not(-e "dbi.prof"), "file should be deleted when DeleteFiles set" );


# make sure the keys didn't get garbled
$Data = $prof->Data;
ok(exists $Data->{$sql2});
ok(exists $Data->{$sql3});

1;
