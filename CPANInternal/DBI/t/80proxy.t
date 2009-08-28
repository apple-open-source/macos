#!perl -w                                         # -*- perl -*-
# vim:sw=4:ts=8

require 5.004;
use strict;


use DBI;
use Config;
require VMS::Filespec if $^O eq 'VMS';
require Cwd;

my $haveFileSpec = eval { require File::Spec };
my $failed_tests = 0;

$| = 1;
$^W = 1;

# $\ = "\n"; # XXX Triggers bug, check this later (JW, 1998-12-28)

# Can we load the modules? If not, exit the test immediately:
# Reason is most probable a missing prerequisite.
#
# Is syslog available (required for the server)?

eval {
    local $SIG{__WARN__} = sub { $@ = shift };
    require Storable;
    require DBD::Proxy;
    require DBI::ProxyServer;
    require RPC::PlServer;
    require Net::Daemon::Test;
};
if ($@) {
    if ($@ =~ /^Can't locate (\S+)/) {
	print "1..0 # Skipped: modules required for proxy are probably not installed (e.g., $1)\n";
	exit 0;
    }
    die $@;
}

if ($DBI::PurePerl) {
    # XXX temporary I hope
    print "1..0 # Skipped: DBD::Proxy currently has a problem under DBI::PurePerl\n";
    exit 0;
}

{
    my $numTest = 0;
    sub _old_Test($;$) {
	my $result = shift; my $str = shift || '';
	printf("%sok %d%s\n", ($result ? "" : "not "), ++$numTest, $str);
	$result;
    }
    sub Test ($;$) {
	my($ok, $msg) = @_;
	$msg = ($msg) ? " ($msg)" : "";
	my $line = (caller)[2];
	++$numTest;
	($ok) ? print "ok $numTest at line $line\n" : print "not ok $numTest\n";
	warn "# failed test $numTest at line ".(caller)[2]."$msg\n" unless $ok;
        ++$failed_tests unless $ok;
	return $ok;
    }
}


# Create an empty config file to make sure that settings aren't
# overloaded by /etc/dbiproxy.conf
my $config_file = "dbiproxytst.conf";
unlink $config_file;
(open(FILE, ">$config_file")  and
 (print FILE "{}\n")          and
 close(FILE))
    or die "Failed to create config file $config_file: $!";

my $debug = ($ENV{DBI_TRACE}||=0) ? 1 : 0;
my $dbitracelog = "dbiproxy.dbilog";

my ($handle, $port, @child_args);

my $numTests = 139;

if (@ARGV) {
    $port = $ARGV[0];
}
else {

    unlink $dbitracelog;
    unlink "dbiproxy.log";
    unlink "dbiproxy.truss";

    # Uncommentand adjust this to isolate pure-perl client from server settings:
    # local $ENV{DBI_PUREPERL} = 0;

    # If desperate uncomment this and add '-d' after $^X below:
    # local $ENV{PERLDB_OPTS} = "AutoTrace NonStop=1 LineInfo=dbiproxy.dbg";

    # pass our @INC to children (e.g., so -Mblib passes through)
    $ENV{PERL5LIB} = join($Config{path_sep}, @INC);

    # server DBI trace level always at least 1
    my $dbitracelevel = DBI->trace(0) || 1;
    @child_args = (
	#'truss', '-o', 'dbiproxy.truss',
	$^X, 'dbiproxy', '--test', # --test must be first command line arg
	"--dbitrace=$dbitracelevel=$dbitracelog", # must be second arg
	'--configfile', $config_file,
	($dbitracelevel >= 2 ? ('--debug') : ()),
	'--mode=single',
	'--logfile=STDERR',
	'--timeout=60'
    );
    warn " starting test dbiproxy process: @child_args\n" if DBI->trace(0);
    ($handle, $port) = Net::Daemon::Test->Child($numTests, @child_args);
}

my $dsn = "DBI:Proxy:hostname=127.0.0.1;port=$port;debug=$debug;dsn=DBI:ExampleP:";

print "Making a first connection and closing it immediately.\n";
Test(eval { DBI->connect($dsn, '', '', { 'PrintError' => 1 }) })
    or print "Connect error: " . $DBI::errstr . "\n";

print "Making a second connection.\n";
my $dbh;
Test($dbh = eval { DBI->connect($dsn, '', '', { 'PrintError' => 0 }) })
    or print "Connect error: " . $DBI::errstr . "\n";

print "example_driver_path=$dbh->{example_driver_path}\n";
Test($dbh->{example_driver_path});

print "Setting AutoCommit\n";
$@ = "old-error";	# should be preserved across DBI calls
Test($dbh->{AutoCommit} = 1);
Test($dbh->{AutoCommit});
Test($@ eq "old-error", "\$@ now '$@'");
#$dbh->trace(2);

eval {
    local $dbh->{ AutoCommit } = 1;   # This breaks die!
    die "BANG!!!\n";
};
Test($@ eq "BANG!!!\n", "\$@ value lost");


print "begin_work...\n";
Test($dbh->{AutoCommit});
Test(!$dbh->{BegunWork});

Test($dbh->begin_work);
Test(!$dbh->{AutoCommit});
Test($dbh->{BegunWork});

$dbh->commit;
Test(!$dbh->{BegunWork});
Test($dbh->{AutoCommit});

Test($dbh->begin_work({}));
$dbh->rollback;
Test($dbh->{AutoCommit});
Test(!$dbh->{BegunWork});


print "Doing a ping.\n";
$_ = $dbh->ping;
Test($_);
Test($_ eq '2'); # ping was DBD::ExampleP's ping

print "Ensure CompatMode enabled.\n";
Test($dbh->{CompatMode});

print "Trying local quote.\n";
$dbh->{'proxy_quote'} = 'local';
Test($dbh->quote("quote's") eq "'quote''s'");
Test($dbh->quote(undef)     eq "NULL");

print "Trying remote quote.\n";
$dbh->{'proxy_quote'} = 'remote';
Test($dbh->quote("quote's") eq "'quote''s'");
Test($dbh->quote(undef)     eq "NULL");

# XXX the $optional param is undocumented and may be removed soon
Test($dbh->quote_identifier('foo')    eq '"foo"',  $dbh->quote_identifier('foo'));
Test($dbh->quote_identifier('f"o')    eq '"f""o"', $dbh->quote_identifier('f"o'));
Test($dbh->quote_identifier('foo','bar') eq '"foo"."bar"');
Test($dbh->quote_identifier('foo',undef,'bar') eq '"foo"."bar"');
Test($dbh->quote_identifier(undef,undef,'bar') eq '"bar"');

print "Trying commit with invalid number of parameters.\n";
eval { $dbh->commit('dummy') };
Test($@ =~ m/^DBI commit: invalid number of arguments:/)
    unless $DBI::PurePerl && Test(1);

print "Trying select with unknown field name.\n";
my $cursor_e = $dbh->prepare("select unknown_field_name from ?");
Test(defined $cursor_e);
Test(!$cursor_e->execute('a'));
Test($DBI::err);
Test($DBI::err == $dbh->err);
Test($DBI::errstr =~ m/unknown_field_name/, $DBI::errstr);

Test($DBI::errstr eq $dbh->errstr);
Test($dbh->errstr eq $dbh->func('errstr'));

my $dir = Cwd::cwd();	# a dir always readable on all platforms
$dir = VMS::Filespec::unixify($dir) if $^O eq 'VMS';

print "Trying a real select.\n";
my $csr_a = $dbh->prepare("select mode,size,name from ?");
Test(ref $csr_a);
Test($csr_a->execute($dir))
    or print "Execute failed: ", $csr_a->errstr(), "\n";

print "Repeating the select with second handle.\n";
my $csr_b = $dbh->prepare("select mode,size,name from ?");
Test(ref $csr_b);
Test($csr_b->execute($dir));
Test($csr_a != $csr_b);
Test($csr_a->{NUM_OF_FIELDS} == 3);
if ($DBI::PurePerl) { 
    $csr_a->trace(2);
    use Data::Dumper;
    warn Dumper($csr_a->{Database});
}
Test($csr_a->{Database}->{Driver}->{Name} eq 'Proxy', "Name=$csr_a->{Database}->{Driver}->{Name}");
$csr_a->trace(0), die if $DBI::PurePerl;

my($col0, $col1, $col2);
my(@row_a, @row_b);

#$csr_a->trace(2);
print "Trying bind_columns.\n";
Test($csr_a->bind_columns(undef, \($col0, $col1, $col2)) );
Test($csr_a->execute($dir));
@row_a = $csr_a->fetchrow_array;
Test(@row_a);
Test($row_a[0] eq $col0);
Test($row_a[1] eq $col1);
Test($row_a[2] eq $col2);

print "Trying bind_param.\n";
Test($csr_b->bind_param(1, $dir));
Test($csr_b->execute());
@row_b = @{ $csr_b->fetchrow_arrayref };
Test(@row_b);

Test("@row_a" eq "@row_b");
@row_b = $csr_b->fetchrow_array;
Test("@row_a" ne "@row_b")
    or printf("Expected something different from '%s', got '%s'\n", "@row_a",
              "@row_b");

print "Trying fetchrow_hashref.\n";
Test($csr_b->execute());
my $row_b = $csr_b->fetchrow_hashref;
Test($row_b);
print "row_a: @{[ @row_a  ]}\n";
print "row_b: @{[ %$row_b ]}\n";
Test($row_b->{mode} == $row_a[0]);
Test($row_b->{size} == $row_a[1]);
Test($row_b->{name} eq $row_a[2]);

print "Trying fetchrow_hashref with FetchHashKeyName.\n";
do {
#local $dbh->{TraceLevel} = 9;
local $dbh->{FetchHashKeyName} = 'NAME_uc';
Test($dbh->{FetchHashKeyName} eq 'NAME_uc');
my $csr_c = $dbh->prepare("select mode,size,name from ?");
Test($csr_c->execute($dir), $DBI::errstr);
$row_b = $csr_c->fetchrow_hashref;
Test($row_b);
print "row_b: @{[ %$row_b ]}\n";
Test($row_b->{MODE} eq $row_a[0]);
};

print "Trying finish.\n";
Test($csr_a->finish);
#Test($csr_b->finish);
Test(1);

print "Forcing destructor.\n";
$csr_a = undef;	# force destruction of this cursor now
Test(1);

print "Trying fetchall_arrayref.\n";
Test($csr_b->execute());
my $r = $csr_b->fetchall_arrayref;
Test($r);
Test(@$r);
Test($r->[0]->[0] == $row_a[0]);
Test($r->[0]->[1] == $row_a[1]);
Test($r->[0]->[2] eq $row_a[2]);

Test($csr_b->finish);


print "Retrying unknown field name.\n";
my $csr_c;
$csr_c = $dbh->prepare("select unknown_field_name1 from ?");
Test($csr_c);
Test(!$csr_c->execute($dir));
Test($DBI::errstr =~ m/Unknown field names: unknown_field_name1/)
    or printf("Wrong error string: %s", $DBI::errstr);

print "Trying RaiseError.\n";
$dbh->{RaiseError} = 1;
Test($dbh->{RaiseError});
Test($csr_c = $dbh->prepare("select unknown_field_name2 from ?"));
Test(!eval { $csr_c->execute(); 1 });
#print "$@\n";
Test($@ =~ m/Unknown field names: unknown_field_name2/);
$dbh->{RaiseError} = 0;
Test(!$dbh->{RaiseError});

print "Trying warnings.\n";
{
  my @warn;
  local($SIG{__WARN__}) = sub { push @warn, @_ };
  $dbh->{PrintError} = 1;
  Test($dbh->{PrintError});
  Test(($csr_c = $dbh->prepare("select unknown_field_name3 from ?")));
  Test(!$csr_c->execute());
  Test("@warn" =~ m/Unknown field names: unknown_field_name3/);
  $dbh->{PrintError} = 0;
  Test(!$dbh->{PrintError});
}
$csr_c->finish();


print "Trying type_info_all.\n";
my $array = $dbh->type_info_all();
Test($array  and  ref($array) eq 'ARRAY')
    or printf("Expected ARRAY, got %s, error %s\n", DBI::neat($array),
	      $dbh->errstr());
Test($array->[0]  and  ref($array->[0]) eq 'HASH');
my $ok = 1;
for (my $i = 1;  $i < @{$array};  $i++) {
    print "$array->[$i]\n";
    $ok = 0  unless ($array->[$i]  and  ref($array->[$i]) eq 'ARRAY');
    print "$ok\n";
}
Test($ok);

# Test the table_info method
# First generate a list of all subdirectories
$dir = $haveFileSpec ? File::Spec->curdir() : ".";
Test(opendir(DIR, $dir));
my(%dirs, %unexpected, %missing);
while (defined(my $file = readdir(DIR))) {
    $dirs{$file} = 1 if -d $file;
}
closedir(DIR);
my $sth = $dbh->table_info(undef, undef, undef, undef);
Test($sth) or warn "table_info failed: ", $dbh->errstr(), "\n";
%missing = %dirs;
%unexpected = ();
while (my $ref = $sth->fetchrow_hashref()) {
    print "table_info: Found table $ref->{'TABLE_NAME'}\n";
    if (exists($missing{$ref->{'TABLE_NAME'}})) {
	delete $missing{$ref->{'TABLE_NAME'}};
    } else {
	$unexpected{$ref->{'TABLE_NAME'}} = 1;
    }
}
Test(!$sth->errstr())
    or print "Fetching table_info rows failed: ", $sth->errstr(), "\n";
Test(keys %unexpected == 0)
    or print "Unexpected directories: ", join(",", keys %unexpected), "\n";
Test(keys %missing == 0)
    or print "Missing directories: ", join(",", keys %missing), "\n";

# Test the tables method
%missing = %dirs;
%unexpected = ();
print "Expecting directories ", join(",", keys %dirs), "\n";
foreach my $table ($dbh->tables()) {
    print "tables: Found table $table\n";
    if (exists($missing{$table})) {
	delete $missing{$table};
    } else {
	$unexpected{$table} = 1;
    }
}
Test(!$sth->errstr())
    or print "Fetching table_info rows failed: ", $sth->errstr(), "\n";
Test(keys %unexpected == 0)
    or print "Unexpected directories: ", join(",", keys %unexpected), "\n";
Test(keys %missing == 0)
    or print "Missing directories: ", join(",", keys %missing), "\n";


# Test large recordsets
for (my $i = 0;  $i <= 300;  $i += 100) {
    print "Testing the fake directories ($i).\n";
    Test($csr_a = $dbh->prepare("SELECT name, mode FROM long_list_$i"));
    Test($csr_a->execute(), $DBI::errstr);
    my $ary = $csr_a->fetchall_arrayref;
    Test(!$DBI::errstr, $DBI::errstr);
    Test(@$ary == $i, "expected $i got ".@$ary);
    if ($i) {
        my @n1 = map { $_->[0] } @$ary;
        my @n2 = reverse map { "file$_" } 1..$i;
        Test("@n1" eq "@n2");
    }
    else {
        Test(1);
    }
}


# Test the RowCacheSize attribute
Test($csr_a = $dbh->prepare("SELECT * FROM ?"));
Test($dbh->{'RowCacheSize'} == 20);
Test($csr_a->{'RowCacheSize'} == 20);
Test($csr_a->execute('long_list_50'));
Test($csr_a->fetchrow_arrayref());
Test($csr_a->{'proxy_data'}  and  @{$csr_a->{'proxy_data'}} == 19);
Test($csr_a->finish());

Test($dbh->{'RowCacheSize'} = 30);
Test($dbh->{'RowCacheSize'} == 30);
Test($csr_a->{'RowCacheSize'} == 30);
Test($csr_a->execute('long_list_50'));
Test($csr_a->fetchrow_arrayref());
Test($csr_a->{'proxy_data'}  and  @{$csr_a->{'proxy_data'}} == 29)
    or print("Expected 29 records in cache, got " . @{$csr_a->{'proxy_data'}} .
	     "\n");
Test($csr_a->finish());


Test($csr_a->{'RowCacheSize'} = 10);
Test($dbh->{'RowCacheSize'} == 30);
Test($csr_a->{'RowCacheSize'} == 10);
Test($csr_a->execute('long_list_50'));
Test($csr_a->fetchrow_arrayref());
Test($csr_a->{'proxy_data'}  and  @{$csr_a->{'proxy_data'}} == 9)
    or print("Expected 9 records in cache, got " . @{$csr_a->{'proxy_data'}} .
	     "\n");
Test($csr_a->finish());

$dbh->disconnect;

# Test $dbh->func()
#  print "Testing \$dbh->func().\n";
#  my %tables = map { $_ =~ /lib/ ? ($_, 1) : () } $dbh->tables();
#  $ok = 1;
#  foreach my $t ($dbh->func('lib', 'examplep_tables')) {
#      defined(delete $tables{$t}) or print "Unexpected table: $t\n";
#  }
#  Test(%tables == 0);

if ($failed_tests) {
    warn "Proxy: @child_args\n";
    for my $class (qw(Net::Daemon RPC::PlServer Storable)) {
        (my $pm = $class) =~ s/::/\//g; $pm .= ".pm";
        my $version = eval { $class->VERSION } || '?';
        warn sprintf "Using %-13s %-6s  %s\n", $class, $version, $INC{$pm};
    }
    warn join(", ", map { "$_=$ENV{$_}" } grep { /^LC_|LANG/ } keys %ENV)."\n";
    warn "More info can be found in $dbitracelog\n";
}


END {
    local $?;
    $handle->Terminate() if $handle;
    undef $handle;
    unlink $config_file if $config_file;
    if (!$failed_tests) {
        unlink 'dbiproxy.log';
        unlink $dbitracelog if $dbitracelog;
    }
};

1;
