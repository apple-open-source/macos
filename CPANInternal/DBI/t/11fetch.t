#!perl -w
# vim:ts=8:sw=4
$|=1;

use strict;

use Test::More;
use DBI;
use Storable qw(dclone);
use Data::Dumper;

$Data::Dumper::Indent = 1;
$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Quotekeys = 0;

plan tests => 24;

my $dbh = DBI->connect("dbi:Sponge:foo","","", {
        PrintError => 0,
        RaiseError => 1,
});

my $source_rows = [ # data for DBD::Sponge to return via fetch
    [ 41,	"AAA",	9	],
    [ 41,	"BBB",	9	],
    [ 42,	"BBB",	undef	],
    [ 43,	"ccc",	7	],
    [ 44,	"DDD",	6	],
];

sub go {
    my $source = shift || $source_rows;
    my $sth = $dbh->prepare("foo", {
	rows => dclone($source),
	NAME => [ qw(C1 C2 C3) ],
    });
    ok($sth->execute(), $DBI::errstr);
    return $sth;
}

my($sth, $col0, $col1, $col2, $rows);

# --- fetchrow_arrayref
# --- fetchrow_array
# etc etc

# --- fetchall_hashref
my @fetchall_hashref_results = (	# single keys
  C1 => {
    41  => { C1 => 41, C2 => 'BBB', C3 => 9 },
    42  => { C1 => 42, C2 => 'BBB', C3 => undef },
    43  => { C1 => 43, C2 => 'ccc', C3 => 7 },
    44  => { C1 => 44, C2 => 'DDD', C3 => 6 }
  },
  C2 => {
    AAA => { C1 => 41, C2 => 'AAA', C3 => 9 },
    BBB => { C1 => 42, C2 => 'BBB', C3 => undef },
    DDD => { C1 => 44, C2 => 'DDD', C3 => 6 },
    ccc => { C1 => 43, C2 => 'ccc', C3 => 7 }
  },
  [ 'C2' ] => {				# single key within arrayref
    AAA => { C1 => 41, C2 => 'AAA', C3 => 9 },
    BBB => { C1 => 42, C2 => 'BBB', C3 => undef },
    DDD => { C1 => 44, C2 => 'DDD', C3 => 6 },
    ccc => { C1 => 43, C2 => 'ccc', C3 => 7 }
  },
);
push @fetchall_hashref_results, (	# multiple keys
  [ 'C1', 'C2' ] => {
    '41' => {
      AAA => { C1 => '41', C2 => 'AAA', C3 => 9 },
      BBB => { C1 => '41', C2 => 'BBB', C3 => 9 }
    },
    '42' => {
      BBB => { C1 => '42', C2 => 'BBB', C3 => undef }
    },
    '43' => {
      ccc => { C1 => '43', C2 => 'ccc', C3 => 7 }
    },
    '44' => {
      DDD => { C1 => '44', C2 => 'DDD', C3 => 6 }
    }
  },
);

my %dump;

while (my $keyfield = shift @fetchall_hashref_results) {
    my $expected = shift @fetchall_hashref_results;
    my $k = (ref $keyfield) ? "[@$keyfield]" : $keyfield;
    print "# fetchall_hashref($k)\n";
    ok($sth = go());
    my $result = $sth->fetchall_hashref($keyfield);
    ok($result);
    is_deeply($result, $expected);
    # $dump{$k} = dclone $result; # just for adding tests
}

warn Dumper \%dump if %dump;

# test assignment to NUM_OF_FIELDS automatically alters the row buffer
$sth = go();
my $row = $sth->fetchrow_arrayref;
is scalar @$row, 3;
is $sth->{NUM_OF_FIELDS}, 3;
is scalar @{ $sth->_get_fbav }, 3;
$sth->{NUM_OF_FIELDS} = 4;
is $sth->{NUM_OF_FIELDS}, 4;
is scalar @{ $sth->_get_fbav }, 4;
$sth->{NUM_OF_FIELDS} = 2;
is $sth->{NUM_OF_FIELDS}, 2;
is scalar @{ $sth->_get_fbav }, 2;

$sth->finish;


if (0) {
    my @perf = map { [ int($_/100), $_, $_ ] } 0..10000;
    require Benchmark;
    Benchmark::timethis(10, sub { go(\@perf)->fetchall_hashref([ 'C1','C2','C3' ]) });
}


1; # end
