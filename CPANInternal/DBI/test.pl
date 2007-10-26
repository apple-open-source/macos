#!/usr/local/bin/perl -w

# $Id: test.pl,v 11.7 2004/02/01 11:16:16 timbo Exp $
#
# Copyright (c) 1994-1998 Tim Bunce
#
# See COPYRIGHT section in DBI.pm for usage and distribution rights.


# This is now mostly an empty shell I experiment with.
# The real tests have moved to t/*.t
# See t/*.t for more detailed tests.


BEGIN {
    print "$0 @ARGV\n";
    print q{DBI test application $Revision: 11.7 $}."\n";
    $| = 1;
}

use blib;

use DBI;

use DBI::DBD;	# simple test to make sure it's okay

use Config;
use Getopt::Long;
use strict;

$::opt_d = 0;
$::opt_l = '';
$::opt_h = 0;
$::opt_m = 0;		# basic memory leak test: "perl test.pl -m NullP"
$::opt_t = 0;		# thread test
$::opt_n = 0;		# counter for other options

GetOptions(qw(d=i h=i l=s m t=i n=i))
    or die "Usage: $0 [-d n] [-h n] [-m] [-t n] [-n n] [drivername]\n";

my $count = 0;
my $ps = (-d '/proc') ? "ps -lp " : "ps -l";
my $driver = $ARGV[0] || ($::opt_m ? 'NullP' : 'ExampleP');

# Now ask for some information from the DBI Switch
my $switch = DBI->internal;
$switch->debug($::opt_h); # 2=detailed handle trace

DBI->trace($::opt_d, $::opt_l) if $::opt_d || $::opt_l;

print "Switch: $switch->{'Attribution'}, $switch->{'Version'}\n";

print "Available Drivers: ",join(", ",DBI->available_drivers(1)),"\n";


my $dbh = DBI->connect("dbi:$driver:", '', ''); # old-style connect syntax
$dbh->debug($::opt_h);

if (0) {
    DBI->trace(3);
    my $h = DBI->connect('dbi:NullP:','','', { RootClass=>'MyTestDBI', DbTypeSubclass=>'foo, bar' });
    DBI->trace(0);
    { # only works after 5.004_04:
	warn "RaiseError= '$h->{RaiseError}' (pre local)\n";
	local($h->{RaiseError});# = undef;
	warn "RaiseError= '$h->{RaiseError}' (post local)\n";
    }
    warn "RaiseError= '$h->{RaiseError}' (post local block)\n";
    exit 1;
}

if ($::opt_m) {
    #$dbh->trace(9);
    my $level = 4;
    my $cnt = 10000;
    print "Using $driver, same dbh...\n";
    #for (my $i=0; $i<$cnt; ++$i) { mem_test($dbh, undef, $level) }
    print "Using NullP, reconnecting each time...\n";
    #for (my $i=0; $i<$cnt; ++$i) { mem_test(undef, ['dbi:NullP:'], $level) }
    print "Using ExampleP, reconnecting each time...\n";
    mem_test(undef, ['dbi:ExampleP:'], $level) while 1;
    #mem_test(undef, ['dbi:mysql:VC'], $level, "select * from campaigns where length(?)>0") while 1;
}
elsif ($::opt_t) {
	thread_test();
}
else {

    # new experimental connect_test_perf method
    DBI->connect_test_perf("dbi:$driver:", '', '', {
	dbi_loops=>5, dbi_par=>20, dbi_verb=>1
    });

    require Benchmark;
    print "Testing handle creation speed...\n";
    my $null_dbh = DBI->connect('dbi:NullP:','','');
    my $null_sth = $null_dbh->prepare('');	# create one to warm up
    $count = 10_000;
    my $i = $count;
    my $t1 = new Benchmark;
    $null_dbh->prepare('') while $i--;
    my $td = Benchmark::timediff(Benchmark->new, $t1);
    my $tds= Benchmark::timestr($td);
    my $dur = $td->cpu_a || (1/$count); # fudge if cpu_a==0

    printf "%5d NullP sth/s perl %8s %s (%s %s %s)\n\n",
	    $count/$dur, $], $Config{archname},
	    $Config{gccversion} ? 'gcc' : $Config{cc},
	    (split / /, $Config{gccversion}||$Config{ccversion}||'')[0]||'',
	    $Config{optimize};

  if (0) {
    $null_dbh = DBI->connect('dbi:mysql:VC_log','','',{RaiseError=>1});
    $null_sth = $null_dbh->prepare('select * from big');
    $null_sth->execute();
    $t1 = new Benchmark;
    1 while ($null_sth->fetchrow_hashref());
    #1 while ($null_sth->fetchrow_arrayref());
    $td = Benchmark::timediff(Benchmark->new, $t1);
    $tds= Benchmark::timestr($td);
    $dur = $td->cpu_a;
    printf "$DBI::rows in $tds\n";
  }
}

#DBI->trace(4);
print "$0 done\n";
exit 0;


sub mem_test {	# harness to help find basic leaks
    my ($orig_dbh, $connect, $level, $select, $params) = @_;
    $select ||= "select mode,ino,name from ?";
    $params ||= [ '.' ];
    my $dbh = $orig_dbh || DBI->connect(@$connect);
    system("echo $count; $ps$$") if (($count++ % 500) == 0);
    my $cursor_a;
    $cursor_a = $dbh->prepare($select)		if $level >= 2;
    $cursor_a->execute(@$params)		if $level >= 3;
    my $rows = $cursor_a->fetchall_arrayref({})	if $level >= 4;
    $cursor_a->finish if $cursor_a && $cursor_a->{Active};
    $dbh->disconnect unless $orig_dbh;
}


sub thread_test {
    require Thread;
    my $dbh = DBI->connect("dbi:ExampleP:.", "", "") || die $DBI::err;
    #$dbh->trace(4);
    my @t;
    print "Starting $::opt_t threads:\n";
    foreach(1..$::opt_t) {
	print "$_\n";
	push @t, Thread->new(\&thread_test_loop, $dbh, $::opt_n||99);
    }
    print "Small sleep to allow threads to progress\n";
    sleep 2;
    print "Joining threads:\n";
    foreach(@t) {
	print "$_\n";
	$_->join
    }
}

sub thread_test_loop {
    my $dbh = shift;
    my $i = shift || 10;
    while($i-- > 0) {
	$dbh->selectall_arrayref("select * from ?", undef, ".");
    }
}

# end.
