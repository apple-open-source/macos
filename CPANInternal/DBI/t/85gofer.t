#!perl -w                                         # -*- perl -*-
# vim:sw=4:ts=8
$|=1;

use strict;
use warnings;

use Cwd;
use Config;
use Data::Dumper;
use Test::More;
use Getopt::Long;

use DBI qw(dbi_time);

if (my $ap = $ENV{DBI_AUTOPROXY}) { # limit the insanity
    plan skip_all => "transport+policy tests skipped with non-gofer DBI_AUTOPROXY"
        if $ap !~ /^dbi:Gofer/i;
    plan skip_all => "transport+policy tests skipped with non-pedantic policy in DBI_AUTOPROXY"
        if $ap !~ /policy=pedantic\b/i;
}

# 0=SQL::Statement if avail, 1=DBI::SQL::Nano
# next line forces use of Nano rather than default behaviour
$ENV{DBI_SQL_NANO}=1;

GetOptions(
    'c|count=i' => \(my $opt_count = (-t STDOUT ? 100 : 0)),
    'dbm=s'     => \my $opt_dbm,
    'v|verbose!' => \my $opt_verbose,
    't|transport=s' => \my $opt_transport,
    'p|policy=s'    => \my $opt_policy,
) or exit 1;


# so users can try others from the command line
if (!$opt_dbm) {
    # pick first available, starting with SDBM_File
    for (qw( SDBM_File GDBM_File DB_File BerkeleyDB )) {
        if (eval { local $^W; require "$_.pm" }) {
            $opt_dbm = ($_);
            last;
        }
    }
    plan skip_all => 'No DBM modules available' if !$opt_dbm;
}
my $remote_driver_dsn = "dbm_type=$opt_dbm;lockfile=0";
my $remote_dsn = "dbi:DBM:$remote_driver_dsn";
my $timeout = 120; # for slow/overloaded systems (incl virtual machines with low priority)

plan 'no_plan';

if ($ENV{DBI_AUTOPROXY}) {
    # this means we have DBD::Gofer => DBD::Gofer => DBD::DBM!
    # rather than disable it we let it run because we're twisted
    # and because it helps find more bugs (though debugging can be painful)
    warn "\n$0 is running with DBI_AUTOPROXY enabled ($ENV{DBI_AUTOPROXY})\n"
        unless $0 =~ /\bzv/; # don't warn for t/zvg_85gofer.t
}

# ensure subprocess (for pipeone and stream transport) will use the same modules as us, ie ./blib
local $ENV{PERL5LIB} = join $Config{path_sep}, @INC;

my %durations;
my $getcwd = getcwd();
my $username = eval { getpwuid($>) } || ''; # fails on windows
my $can_ssh = ($username && $username eq 'timbo' && -d '.svn');
my $perl = "$^X  -Mblib=$getcwd/blib"; # ensure sameperl and our blib (note two spaces)

my %trials = (
    null       => {},
    pipeone    => { perl=>$perl, timeout=>$timeout },
    stream     => { perl=>$perl, timeout=>$timeout },
    stream_ssh => ($can_ssh)
                ? { perl=>$perl, timeout=>$timeout, url => "ssh:$username\@localhost" }
                : undef,
    #http       => { url => "http://localhost:8001/gofer" },
);

# too dependant on local config to make a standard test
delete $trials{http} unless $username eq 'timbo' && -d '.svn';

my @transports = ($opt_transport) ? ($opt_transport) : (sort keys %trials);
print "Transports: @transports\n";
my @policies = ($opt_policy) ? ($opt_policy) : qw(pedantic classic rush);
print "Policies: @policies\n";
print "Count: $opt_count\n";

for my $trial (@transports) {
    (my $transport = $trial) =~ s/_.*//;
    my $trans_attr = $trials{$trial}
        or next;

    # XXX temporary restrictions, hopefully
    if ( ($^O eq 'MSWin32') || ($^O eq 'VMS') ) {
       # stream needs Fcntl macro F_GETFL for non-blocking
       # and pipe seems to hang on some windows systems
        next if $transport eq 'stream' or $transport eq 'pipeone';
    }

    for my $policy_name (@policies) {

        eval { run_tests($transport, $trans_attr, $policy_name) };
        ($@) ? fail("$trial: $@") : pass();

    }
}

# to get baseline for comparisons if doing performance testing
run_tests('no', {}, 'pedantic') if $opt_count;

while ( my ($activity, $stats_hash) = each %durations ) {
    print "\n";
    $stats_hash->{'~baseline~'} = delete $stats_hash->{"no+pedantic"};
    for my $perf_tag (reverse sort keys %$stats_hash) {
        my $dur = $stats_hash->{$perf_tag} || 0.0000001;
        printf "  %6s %-16s: %.6fsec (%5d/sec)",
            $activity, $perf_tag, $dur/$opt_count, $opt_count/$dur;
        my $baseline_dur = $stats_hash->{'~baseline~'};
        printf " %+5.1fms", (($dur-$baseline_dur)/$opt_count)*1000
            unless $perf_tag eq '~baseline~';
        print "\n";
    }
}


sub run_tests {
    my ($transport, $trans_attr, $policy_name) = @_;

    my $policy = get_policy($policy_name);
    my $skip_gofer_checks = ($transport eq 'no');


    my $test_run_tag = "Testing $transport transport with $policy_name policy";
    print "\n$test_run_tag\n";

    my $driver_dsn = "transport=$transport;policy=$policy_name";
    $driver_dsn .= join ";", '', map { "$_=$trans_attr->{$_}" } keys %$trans_attr
        if %$trans_attr;

    my $dsn = "dbi:Gofer:$driver_dsn;dsn=$remote_dsn";
    $dsn = $remote_dsn if $transport eq 'no';
    print " $dsn\n";

    my $dbh = DBI->connect($dsn, undef, undef, { RaiseError => 1, PrintError => 0, ShowErrorStatement => 1 } );
    die "$test_run_tag aborted: $DBI::errstr\n" unless $dbh; # no point continuing
    ok $dbh, sprintf "should connect to %s", $dsn;

    is $dbh->{Name}, ($policy->skip_connect_check)
        ? $driver_dsn
        : $remote_driver_dsn;

    ok $dbh->do("DROP TABLE IF EXISTS fruit");
    ok $dbh->do("CREATE TABLE fruit (dKey INT, dVal VARCHAR(10))");
    die "$test_run_tag aborted\n" if $DBI::err;

    my $sth = do {
        local $dbh->{RaiseError} = 0;
        $dbh->prepare("complete non-sql gibberish");
    };
    ($policy->skip_prepare_check)
        ? isa_ok $sth, 'DBI::st'
        : is $sth, undef, 'should detect prepare failure';

    ok my $ins_sth = $dbh->prepare("INSERT INTO fruit VALUES (?,?)");
    ok $ins_sth->execute(1, 'oranges');
    ok $ins_sth->execute(2, 'oranges');

    my $rowset;
    ok $rowset = $dbh->selectall_arrayref("SELECT dKey, dVal FROM fruit ORDER BY dKey");
    is_deeply($rowset, [ [ '1', 'oranges' ], [ '2', 'oranges' ] ]);

    ok $dbh->do("UPDATE fruit SET dVal='apples' WHERE dVal='oranges'");
    ok $dbh->{go_response}->executed_flag_set, 'go_response executed flag should be true'
        unless $skip_gofer_checks && pass();

    ok $sth = $dbh->prepare("SELECT dKey, dVal FROM fruit");
    ok $sth->execute;
    ok $rowset = $sth->fetchall_hashref('dKey');
    is_deeply($rowset, { '1' => { dKey=>1, dVal=>'apples' }, 2 => { dKey=>2, dVal=>'apples' } });

    if ($opt_count and $transport ne 'pipeone') {
        print "performance check - $opt_count selects and inserts\n";
        my $start = dbi_time();
        $dbh->selectall_arrayref("SELECT dKey, dVal FROM fruit")
            for (1000..1000+$opt_count);
        $durations{select}{"$transport+$policy_name"} = dbi_time() - $start;

        # some rows in to get a (*very* rough) idea of overheads
        $start = dbi_time();
        $ins_sth->execute($_, 'speed')
            for (1000..1000+$opt_count);
        $durations{insert}{"$transport+$policy_name"} = dbi_time() - $start;
    }

    print "Testing go_request_count and caching of simple values\n";
    my $go_request_count = $dbh->{go_request_count};
    ok $go_request_count
        unless $skip_gofer_checks && pass();

    ok $dbh->do("DROP TABLE fruit");
    is ++$go_request_count, $dbh->{go_request_count}
        unless $skip_gofer_checks && pass();

    # tests go_request_count, caching, and skip_default_methods policy
    my $use_remote = ($policy->skip_default_methods) ? 0 : 1;
    printf "use_remote=%s (policy=%s, transport=%s) %s\n",
        $use_remote, $policy_name, $transport, $dbh->{dbi_default_methods}||'';

SKIP: {
    skip "skip_default_methods checking doesn't work with Gofer over Gofer", 3
        if $ENV{DBI_AUTOPROXY} or $skip_gofer_checks;
    $dbh->data_sources({ foo_bar => $go_request_count });
    is $dbh->{go_request_count}, $go_request_count + 1*$use_remote;
    $dbh->data_sources({ foo_bar => $go_request_count }); # should use cache
    is $dbh->{go_request_count}, $go_request_count + 1*$use_remote;
    @_=$dbh->data_sources({ foo_bar => $go_request_count }); # no cached yet due to wantarray
    is $dbh->{go_request_count}, $go_request_count + 2*$use_remote;
}

SKIP: {
    skip "caching of metadata methods returning sth not yet implemented", 2;
    print "Testing go_request_count and caching of sth\n";
    $go_request_count = $dbh->{go_request_count};
    my $sth_ti1 = $dbh->table_info("%", "%", "%", "TABLE", { foo_bar => $go_request_count });
    is $go_request_count + 1, $dbh->{go_request_count};

    my $sth_ti2 = $dbh->table_info("%", "%", "%", "TABLE", { foo_bar => $go_request_count }); # should use cache
    is $go_request_count + 1, $dbh->{go_request_count};
}

    ok $dbh->disconnect;
}

sub get_policy {
    my ($policy_class) = @_;
    $policy_class = "DBD::Gofer::Policy::$policy_class" unless $policy_class =~ /::/;
    _load_class($policy_class) or die $@;
    return $policy_class->new();
}

sub _load_class { # return true or false+$@
    my $class = shift;
    (my $pm = $class) =~ s{::}{/}g;
    $pm .= ".pm"; 
    return 1 if eval { require $pm };
    delete $INC{$pm}; # shouldn't be needed (perl bug?) and assigning undef isn't enough
    undef; # error in $@
}   


1;
