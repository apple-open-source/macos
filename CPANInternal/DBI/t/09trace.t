#!perl -w
# vim:sw=4:ts=8

use strict;

use Test::More tests => 67;

## ----------------------------------------------------------------------------
## 09trace.t
## ----------------------------------------------------------------------------
# 
## ----------------------------------------------------------------------------

BEGIN { 
    $ENV{DBI_TRACE} = 0; # for PurePerl - ensure DBI_TRACE is in the env
    use_ok( 'DBI' ); 
}

$|=1;


my $trace_file = "dbitrace.log";

1 while unlink $trace_file;
warn "Can't unlink existing $trace_file: $!" if -e $trace_file;

my $orig_trace_level = DBI->trace;
DBI->trace(3, $trace_file);             # enable trace before first driver load

my $dbh = DBI->connect('dbi:ExampleP(AutoCommit=>1):', undef, undef);
die "Unable to connect to ExampleP driver: $DBI::errstr" unless $dbh;

isa_ok($dbh, 'DBI::db');

$dbh->dump_handle("dump_handle test, write to log file", 2);

DBI->trace(0, undef);   # turn off and restore to STDERR

SKIP: {
        skip "cygwin has buffer flushing bug", 1 if ($^O =~ /cygwin/i);
        ok( -s $trace_file, "trace file size = " . -s $trace_file);
}

DBI->trace($orig_trace_level);  # no way to restore previous outfile XXX


# Clean up when we're done.
END { $dbh->disconnect if $dbh };

## ----------------------------------------------------------------------------
# Check the database handle attributes.

cmp_ok($dbh->{TraceLevel}, '==', $DBI::dbi_debug & 0xF, '... checking TraceLevel attribute');

1 while unlink $trace_file;

$dbh->trace(0, $trace_file);
ok( -f $trace_file, '... trace file successfully created');

my @names = qw(
	SQL
	foo bar baz boo bop
);
my %flag;
my $all_flags = 0;

foreach my $name (@names) {
    print "parse_trace_flag $name\n";
    ok( my $flag1 = $dbh->parse_trace_flag($name) );
    ok( my $flag2 = $dbh->parse_trace_flags($name) );
    is( $flag1, $flag2 );

    $dbh->{TraceLevel} = $flag1;
    is( $dbh->{TraceLevel}, $flag1 );

    $dbh->{TraceLevel} = 0;
    is( $dbh->{TraceLevel}, 0 );

    $dbh->trace($flag1);
    is $dbh->trace,        $flag1;
    is $dbh->{TraceLevel}, $flag1;

    $dbh->{TraceLevel} = $name;		# set by name
    $dbh->{TraceLevel} = undef;		# check no change on undef
    is( $dbh->{TraceLevel}, $flag1 );

    $flag{$name} = $flag1;
    $all_flags |= $flag1
        if defined $flag1; # reduce noise if there's a bug
}

print "parse_trace_flag @names\n";
ok(eq_set([ keys %flag ], [ @names ]), '...');
$dbh->{TraceLevel} = 0;
$dbh->{TraceLevel} = join "|", @names;
is($dbh->{TraceLevel}, $all_flags, '...');

{
    print "inherit\n";
    my $sth = $dbh->prepare("select ctime, name from foo");
    isa_ok( $sth, 'DBI::st' );
    is( $sth->{TraceLevel}, $all_flags );
}

$dbh->{TraceLevel} = 0;
ok !$dbh->{TraceLevel};
$dbh->{TraceLevel} = 'ALL';
ok $dbh->{TraceLevel};

{
    print "test unknown parse_trace_flag\n";
    my $warn = 0;
    local $SIG{__WARN__} = sub {
        if ($_[0] =~ /unknown/i) { ++$warn; print "caught warn: ",@_ }else{ warn @_ }
        };
    is $dbh->parse_trace_flag("nonesuch"), undef;
    is $warn, 0;
    is $dbh->parse_trace_flags("nonesuch"), 0;
    is $warn, 1;
    is $dbh->parse_trace_flags("nonesuch|SQL|nonesuch2"), $dbh->parse_trace_flag("SQL");
    is $warn, 2;
}

$dbh->dump_handle("dump_handle test, write to log file", 2);

$dbh->trace(0);
ok !$dbh->{TraceLevel};
$dbh->trace(undef, "STDERR");	# close $trace_file
ok( -s $trace_file );

1;
# end
