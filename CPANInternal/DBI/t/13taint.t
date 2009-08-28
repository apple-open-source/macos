#!perl -wT

use lib qw(blib/arch blib/lib);	# needed since -T ignores PERL5LIB
use DBI qw(:sql_types);
use Config;
use Cwd;
use strict;


$^W = 1;
$| = 1;

require VMS::Filespec if $^O eq 'VMS';

use Test::More;

# Check Taint attribute works. This requires this test to be run
# manually with the -T flag: "perl -T -Mblib t/examp.t"
sub is_tainted {
    my $foo;
    return ! eval { ($foo=join('',@_)), kill 0; 1; };
}
sub mk_tainted {
    my $string = shift;
    return substr($string.$^X, 0, length($string));
}

plan skip_all => "Taint attributes not supported with DBI::PurePerl" if $DBI::PurePerl;
plan skip_all => "Taint attribute tests require taint mode (perl -T)" unless is_tainted($^X);
plan skip_all => "Taint attribute tests not functional with DBI_AUTOPROXY" if $ENV{DBI_AUTOPROXY};

plan tests => 36;

# get a dir always readable on all platforms
my $dir = getcwd() || cwd();
$dir = VMS::Filespec::unixify($dir) if $^O eq 'VMS';
$dir =~ m/(.*)/; $dir = $1 || die; # untaint $dir

my ($r, $dbh);

$dbh = DBI->connect('dbi:ExampleP:', '', '', { PrintError=>0, RaiseError=>1, Taint => 1 });

my $std_sql = "select mode,size,name from ?";
my $csr_a = $dbh->prepare($std_sql);
ok(ref $csr_a);

ok($dbh->{'Taint'});
ok($dbh->{'TaintIn'} == 1);
ok($dbh->{'TaintOut'} == 1);

$dbh->{'TaintOut'} = 0;
ok($dbh->{'Taint'} == 0);
ok($dbh->{'TaintIn'} == 1);
ok($dbh->{'TaintOut'} == 0);

$dbh->{'Taint'} = 0;
ok($dbh->{'Taint'} == 0);
ok($dbh->{'TaintIn'} == 0);
ok($dbh->{'TaintOut'} == 0);

$dbh->{'TaintIn'} = 1;
ok($dbh->{'Taint'} == 0);
ok($dbh->{'TaintIn'} == 1);
ok($dbh->{'TaintOut'} == 0);

$dbh->{'TaintOut'} = 1;
ok($dbh->{'Taint'} == 1);
ok($dbh->{'TaintIn'} == 1);
ok($dbh->{'TaintOut'} == 1);

$dbh->{'Taint'} = 0;
my $st;
eval { $st = $dbh->prepare($std_sql); };
ok(ref $st);

ok($st->{'Taint'} == 0);

ok($st->execute( $dir ), 'should execute ok');

my @row = $st->fetchrow_array;
ok(@row);

ok(!is_tainted($row[0]));
ok(!is_tainted($row[1]));
ok(!is_tainted($row[2]));

print "TaintIn\n";
$st->{'TaintIn'} = 1;

@row = $st->fetchrow_array;
ok(@row);

ok(!is_tainted($row[0]));
ok(!is_tainted($row[1]));
ok(!is_tainted($row[2]));

print "TaintOut\n";
$st->{'TaintOut'} = 1;

@row = $st->fetchrow_array;
ok(@row);

ok(is_tainted($row[0]));
ok(is_tainted($row[1]));
ok(is_tainted($row[2]));

$st->finish;

my $tainted_sql = mk_tainted($std_sql);
my $tainted_dot = mk_tainted('.');

$dbh->{'Taint'} = $csr_a->{'Taint'} = 1;
eval { $dbh->prepare($tainted_sql); 1; };
ok($@ =~ /Insecure dependency/, $@);
eval { $csr_a->execute($tainted_dot); 1; };
ok($@ =~ /Insecure dependency/, $@);
undef $@;

$dbh->{'TaintIn'} = $csr_a->{'TaintIn'} = 0;

eval { $dbh->prepare($tainted_sql); 1; };
ok(!$@, $@);
eval { $csr_a->execute($tainted_dot); 1; };
ok(!$@, $@);

$csr_a->{Taint} = 0;
ok($csr_a->{Taint} == 0);

$csr_a->finish;

$dbh->disconnect;

1;
