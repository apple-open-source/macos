#!/usr/local/bin/perl -w

# $Id: test.pl 9451 2007-04-25 15:57:06Z timbo $
#
# Copyright (c) 1994-1998 Tim Bunce
#
# See COPYRIGHT section in DBI.pm for usage and distribution rights.


# This is now mostly an empty shell I experiment with.
# The real tests have moved to t/*.t
# See t/*.t for more detailed tests.


BEGIN {
    print "$0 @ARGV\n";
    print q{DBI test application $Revision: 9451 $}."\n";
    $| = 1;
}

use blib;

use DBI;

use DBI::DBD;	# simple test to make sure it's okay

use Config;
use Getopt::Long;
use strict;

our $has_devel_leak = eval {
    local $^W = 0; # silence "Use of uninitialized value $DynaLoader::args[0] in subroutine entry";
    require Devel::Leak;
};

$::opt_d = 0;
$::opt_l = '';
$::opt_h = 0;
$::opt_m = 0;		# basic memory leak test: "perl test.pl -m NullP"
$::opt_t = 0;		# thread test
$::opt_n = 0;		# counter for other options

GetOptions(qw(d=i h=i l=s m=i t=i n=i))
    or die "Usage: $0 [-d n] [-h n] [-m] [-t n] [-n n] [drivername]\n";

my $count = 0;
my $ps = (-d '/proc') ? "ps -lp " : "ps -l";
my $driver = $ARGV[0] || ($::opt_m ? 'NullP' : 'ExampleP');

# Now ask for some information from the DBI Switch
my $switch = DBI->internal;
$switch->trace($::opt_h); # 2=detailed handle trace

DBI->trace($::opt_d, $::opt_l) if $::opt_d || $::opt_l;

print "Switch: $switch->{'Attribution'}, $switch->{'Version'}\n";

print "Available Drivers: ",join(", ",DBI->available_drivers(1)),"\n";


my $dbh = DBI->connect("dbi:$driver:", '', '') or die;
$dbh->trace($::opt_h);

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
    my $level = $::opt_m;
    my $cnt = 10000;
    print "Using $driver, same dbh...\n";
    for (my $i=0; $i<$cnt; ++$i) { mem_test($dbh, undef, $level, undef, undef, undef) }
    print "Using NullP, reconnecting each time...\n";
    for (my $i=0; $i<$cnt; ++$i) { mem_test(undef, ['dbi:NullP:'], $level, undef, undef, undef) }
    print "Using ExampleP, reconnecting each time...\n";
    my $r_develleak = 0;
    mem_test(undef, ['dbi:NullP:'], $level, undef, undef, \$r_develleak) while 1;
    #mem_test(undef, ['dbi:mysql:VC'], $level, "select * from campaigns where length(?)>0", 0, undef) while 1;
}
elsif ($::opt_t) {
	thread_test();
}
else {
    
    # new experimental connect_test_perf method
    DBI->connect_test_perf("dbi:$driver:", '', '', {
	dbi_loops=>3, dbi_par=>20, dbi_verb=>1
    });

    require Benchmark;
    print "Testing handle creation speed...\n";
    my $null_dbh = DBI->connect('dbi:NullP:','','');
    my $null_sth = $null_dbh->prepare('');	# create one to warm up
    $count = 20_000;
    $count /= 10 if $ENV{DBI_AUTOPROXY};
    my $i = $count;
    my $t1 = new Benchmark;
    $null_dbh->prepare('') while $i--;
    my $td = Benchmark::timediff(Benchmark->new, $t1);
    my $tds= Benchmark::timestr($td);
    my $dur = $td->cpu_a || (1/$count); # fudge if cpu_a==0

    printf "%5d NullP sth/s perl %8s %s (%s %s %s) %fs\n\n",
	    $count/$dur, $], $Config{archname},
	    $Config{gccversion} ? 'gcc' : $Config{cc},
	    (split / /, $Config{gccversion}||$Config{ccversion}||'')[0]||'',
	    $Config{optimize},
            $dur/$count;

    $null_dbh->disconnect;
}

$dbh->disconnect;

#DBI->trace(4);
print "$0 done\n";
exit 0;


sub mem_test {	# harness to help find basic leaks
    my ($orig_dbh, $connect, $level, $select, $params, $r_develleak) = @_;
    $select ||= "select mode,ino,name from ?";
    $params ||= [ '.' ];

    # this can be used to force a 'leak' to check memory use reporting
    #$main::leak .= " " x 1000;
    system("echo $count; $ps$$") if (($count++ % 500) == 0);

    my $dbh = $orig_dbh || DBI->connect(@$connect);
    $dbh->{RaiseError} = 1;
    my $cursor_a;

    my ($dl_count, $dl_handle);
    if ($$r_develleak++) {
        $dbh->trace(2);
        $dl_count = Devel::Leak::NoteSV($dl_handle);
    }

    $cursor_a = $dbh->prepare($select)		if $level >= 2;
    $cursor_a->execute(@$params)		if $level >= 3;
    $cursor_a->fetchrow_hashref()        	if $level >= 4;
    my $rows = $cursor_a->fetchall_arrayref({})	if $level >= 4;
    $cursor_a->finish if $cursor_a && $cursor_a->{Active};
    undef $cursor_a;

    @{$dbh->{ChildHandles}} = ();

    die Devel::Leak::CheckSV($dl_handle)-$dl_count
        if $dl_handle;

    $dbh->disconnect unless $orig_dbh;
    undef $dbh;

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
