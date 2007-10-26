#!perl -Tw

use lib qw(blib/arch blib/lib);	# needed since -T ignores PERL5LIB
use DBI qw(:sql_types);
use Config;
use Cwd;

$^W = 1;

my $haveFileSpec = eval { require File::Spec };
require VMS::Filespec if $^O eq 'VMS';

# originally 246 tests
use Test::More tests => 253;
#use Test::More 'no_plan';

# "globals"
my ($r, $dbh);

## testing tracing to file
sub trace_to_file {

	my $trace_file = "dbitrace.log";

	SKIP: {
		skip "no trace file to clean up", 2 unless (-e $trace_file);
	
		is(unlink( $trace_file ), 1, "Remove trace file: $trace_file" );
		ok( !-e $trace_file, "Trace file actually gone" );
	}

	my $orig_trace_level = DBI->trace;
	DBI->trace(3, $trace_file);		# enable trace before first driver load
	
	$dbh = DBI->connect('dbi:ExampleP(AutoCommit=>1):', undef, undef);
	die "Unable to connect to ExampleP driver: $DBI::errstr" unless $dbh;

	isa_ok($dbh, 'DBI::db');

	$dbh->dump_handle("dump_handle test, write to log file", 2);

	DBI->trace(0, undef);	# turn off and restore to STDERR
	
	SKIP: {
		skip "cygwin has buffer flushing bug", 1 if ($^O =~ /cygwin/i);
		ok( -s $trace_file, "trace file size = " . -s $trace_file);
	}

	is( unlink( $trace_file ), 1, "Remove trace file: $trace_file" );
	ok( !-e $trace_file, "Trace file actually gone" );

	DBI->trace($orig_trace_level);	# no way to restore previous outfile XXX
}

trace_to_file();

# internal hack to assist debugging using DBI_TRACE env var. See DBI.pm.
DBI->trace(@DBI::dbi_debug) if @DBI::dbi_debug;

my $dbh2;
eval {
    $dbh2 = DBI->connect("dbi:NoneSuch:foobar", 1, 1, { RaiseError => 1, AutoCommit => 0 });
};
like($@, qr/install_driver\(NoneSuch\) failed/, '... we should have an exception here');
ok(!$dbh2, '... $dbh2 should not be defined');

$dbh2 = DBI->connect('dbi:ExampleP:', '', '');
ok($dbh ne $dbh2);

sub check_connect_cached {
	# connect_cached
	# ------------------------------------------
	# This test checks that connect_cached works
	# and how it then relates to the CachedKids 
	# attribute for the driver.

	my $dbh_cached_1 = DBI->connect_cached('dbi:ExampleP:', '', '');
	my $dbh_cached_2 = DBI->connect_cached('dbi:ExampleP:', '', '');
	my $dbh_cached_3 = DBI->connect_cached('dbi:ExampleP:', '', '', { examplep_foo => 1 });
	
	isa_ok($dbh_cached_1, "DBI::db");
	isa_ok($dbh_cached_2, "DBI::db");
	isa_ok($dbh_cached_3, "DBI::db");
	
	is($dbh_cached_1, $dbh_cached_2, '... these 2 handles are cached, so they are the same');
	isnt($dbh_cached_3, $dbh_cached_2, '... this handle was created with different parameters, so it is not the same');

	my $drh = $dbh->{Driver};
	isa_ok($drh, "DBI::dr");
	
	my @cached_kids = values %{$drh->{CachedKids}};	
	ok(eq_set(\@cached_kids, [ $dbh_cached_1, $dbh_cached_3 ]), '... these are our cached kids');

	$drh->{CachedKids} = {};	
	cmp_ok(scalar(keys %{$drh->{CachedKids}}), '==', 0, '... we have emptied out cache');	
}

check_connect_cached();

$dbh->{AutoCommit} = 1;
$dbh->{PrintError} = 0;

ok($dbh->{AutoCommit} == 1);
cmp_ok($dbh->{PrintError}, '==', 0, '... PrintError should be 0');

SKIP: {
	skip "cant test this if we have DBI::PurePerl", 1 if $DBI::PurePerl;
	$dbh->{Taint} = 1;	
	ok($dbh->{Taint}      == 1);
}

is($dbh->{FetchHashKeyName}, 'NAME', '... FetchHashKey is NAME');
like($dbh->{example_driver_path}, qr/DBD\/ExampleP\.pm$/, '... checking the example driver_path');

sub check_quote {
	# checking quote
	is($dbh->quote("quote's"),         "'quote''s'", '... quoting strings with embedded single quotes');
	is($dbh->quote("42", SQL_VARCHAR), "'42'",       '... quoting number as SQL_VARCHAR');
	is($dbh->quote("42", SQL_INTEGER), "42",         '... quoting number as SQL_INTEGER');
	is($dbh->quote(undef),			   "NULL",		 '... quoting undef as NULL');
}

check_quote();

my $get_info = $dbh->{examplep_get_info} || {};

sub check_quote_identifier {
	# quote_identifier
	$get_info->{29}  ='"';					# SQL_IDENTIFIER_QUOTE_CHAR
	$dbh->{examplep_get_info} = $get_info;	# trigger STORE
	
	is($dbh->quote_identifier('foo'),             '"foo"',       '... properly quotes foo as "foo"');
	is($dbh->quote_identifier('f"o'),             '"f""o"',      '... properly quotes f"o as "f""o"');
	is($dbh->quote_identifier('foo','bar'),       '"foo"."bar"', '... properly quotes foo, bar as "foo"."bar"');
	is($dbh->quote_identifier(undef,undef,'bar'), '"bar"',       '... properly quotes undef, undef, bar as "bar"');

	is($dbh->quote_identifier('foo',undef,'bar'), '"foo"."bar"', '... properly quotes foo, undef, bar as "foo"."bar"');

	$get_info->{41}  ='@';                  # SQL_CATALOG_NAME_SEPARATOR
	$get_info->{114} = 2;                   # SQL_CATALOG_LOCATION
	$dbh->{examplep_get_info} = $get_info;	# trigger STORE

	# force cache refresh
	$dbh->{dbi_quote_identifier_cache} = undef; 
	is($dbh->quote_identifier('foo',undef,'bar'), '"bar"@"foo"', '... now quotes it as "bar"@"foo" after flushing cache');
}

check_quote_identifier();


print "others\n";
eval { $dbh->commit('dummy') };
ok($@ =~ m/DBI commit: invalid number of arguments:/, $@)
	unless $DBI::PurePerl && ok(1);

ok($dbh->ping, "ping should return true");

# --- errors
my $cursor_e = $dbh->prepare("select unknown_field_name from ?");
is($cursor_e, undef, "prepare should fail");
ok($dbh->err, "sth->err should be true");
ok($DBI::err, "DBI::err should be true");
cmp_ok($DBI::err,    'eq', $dbh->err   , "\$DBI::err should match \$dbh->err");
like($DBI::errstr, qr/Unknown field names: unknown_field_name/, "\$DBI::errstr should contain error string");
cmp_ok($DBI::errstr, 'eq', $dbh->errstr, "\$DBI::errstr should match \$dbh->errstr");


# --- func
ok($dbh->errstr eq $dbh->func('errstr'));

my $std_sql = "select mode,size,name from ?";
my $csr_a = $dbh->prepare($std_sql);
ok(ref $csr_a);
ok($csr_a->{NUM_OF_FIELDS} == 3);

SKIP: {
	skip "dont test for DBI::PurePerl", 3 if $DBI::PurePerl;
    ok(tied %{ $csr_a->{Database} });	# ie is 'outer' handle
    ok($csr_a->{Database} eq $dbh, "$csr_a->{Database} ne $dbh")
	unless $dbh->{mx_handle_list} && ok(1); # skip for Multiplex tests
    ok(tied %{ $csr_a->{Database}->{Driver} });	# ie is 'outer' handle
}

my $driver_name = $csr_a->{Database}->{Driver}->{Name};
ok($driver_name eq 'ExampleP');

# --- FetchHashKeyName
$dbh->{FetchHashKeyName} = 'NAME_uc';
my $csr_b = $dbh->prepare($std_sql);
ok(ref $csr_b);

ok($csr_a != $csr_b);

ok("@{$csr_b->{NAME_lc}}" eq "mode size name");	# before NAME
ok("@{$csr_b->{NAME_uc}}" eq "MODE SIZE NAME");
ok("@{$csr_b->{NAME}}"    eq "mode size name");
ok("@{$csr_b->{ $csr_b->{FetchHashKeyName} }}" eq "MODE SIZE NAME");

ok("@{[sort keys   %{$csr_b->{NAME_lc_hash}}]}" eq "mode name size");
ok("@{[sort values %{$csr_b->{NAME_lc_hash}}]}" eq "0 1 2");
ok("@{[sort keys   %{$csr_b->{NAME_uc_hash}}]}" eq "MODE NAME SIZE");
ok("@{[sort values %{$csr_b->{NAME_uc_hash}}]}" eq "0 1 2");

SKIP: {
	skip "do not test with DBI::PurePerl", 15 if $DBI::PurePerl;
	
    # Check Taint* attribute switching

    #$dbh->{'Taint'} = 1; # set in connect
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
}

# get a dir always readable on all platforms
my $dir = getcwd() || cwd();
$dir = VMS::Filespec::unixify($dir) if $^O eq 'VMS';
# untaint $dir
$dir =~ m/(.*)/; $dir = $1 || die;


# ---

my($col0, $col1, $col2, $rows);
my(@row_a, @row_b);

ok($csr_a->{Taint} = 1) unless $DBI::PurePerl && ok(1);
#$csr_a->trace(5);
ok($csr_a->bind_columns(undef, \($col0, $col1, $col2)) );
ok($csr_a->execute( $dir ), $DBI::errstr);

@row_a = $csr_a->fetchrow_array;
ok(@row_a);

# check bind_columns
ok($row_a[0] eq $col0) or print "$row_a[0] ne $col0\n";
ok($row_a[1] eq $col1) or print "$row_a[1] ne $col1\n";
ok($row_a[2] eq $col2) or print "$row_a[2] ne $col2\n";
#$csr_a->trace(0);


SKIP: {

    # Check Taint attribute works. This requires this test to be run
    # manually with the -T flag: "perl -T -Mblib t/examp.t"
    sub is_tainted {
	my $foo;
	return ! eval { ($foo=join('',@_)), kill 0; 1; };
    }

    skip " Taint attribute tests skipped\n", 19 unless(is_tainted($^X) && !$DBI::PurePerl);

    $dbh->{'Taint'} = 0;
    my $st;
    eval { $st = $dbh->prepare($std_sql); };
    ok(ref $st);

    ok($st->{'Taint'} == 0);

    ok($st->execute( $dir ));

    my @row = $st->fetchrow_array;
    ok(@row);

    ok(!is_tainted($row[0]));
    ok(!is_tainted($row[1]));
    ok(!is_tainted($row[2]));

    $st->{'TaintIn'} = 1;

    @row = $st->fetchrow_array;
    ok(@row);

    ok(!is_tainted($row[0]));
    ok(!is_tainted($row[1]));
    ok(!is_tainted($row[2]));

    $st->{'TaintOut'} = 1;

    @row = $st->fetchrow_array;
    ok(@row);

    ok(is_tainted($row[0]));
    ok(is_tainted($row[1]));
    ok(is_tainted($row[2]));

    $st->finish;

    # check simple method call values
    #ok(1);
    # check simple attribute values
    #ok(1); # is_tainted($dbh->{AutoCommit}) );
    # check nested attribute values (where a ref is returned)
    #ok(is_tainted($csr_a->{NAME}->[0]) );
    # check checking for tainted values

    $dbh->{'Taint'} = $csr_a->{'Taint'} = 1;
    eval { $dbh->prepare($^X); 1; };
    ok($@ =~ /Insecure dependency/, $@);
    eval { $csr_a->execute($^X); 1; };
    ok($@ =~ /Insecure dependency/, $@);
    undef $@;

    $dbh->{'TaintIn'} = $csr_a->{'TaintIn'} = 0;

    eval { $dbh->prepare($^X); 1; };
    ok(!$@);
    eval { $csr_a->execute($^X); 1; };
    ok(!$@);

    # Reset taint status to what it was before this block, so that
    # tests later in the file don't get confused
    $dbh->{'Taint'} = $csr_a->{'Taint'} = 1;
}


SKIP: {
	skip "do not test with DBI::PurePerl", 1 if $DBI::PurePerl;
    $csr_a->{Taint} = 0;
    ok($csr_a->{Taint} == 0);
}

ok($csr_b->bind_param(1, $dir));
ok($csr_b->execute());
@row_b = @{ $csr_b->fetchrow_arrayref };
ok(@row_b);

ok("@row_a" eq "@row_b");
@row_b = $csr_b->fetchrow_array;
ok("@row_a" ne "@row_b");

ok($csr_a->finish);
ok($csr_b->finish);

$csr_a = undef;	# force destruction of this cursor now
ok(1);

print "fetchrow_hashref('NAME_uc')\n";
ok($csr_b->execute());
my $row_b = $csr_b->fetchrow_hashref('NAME_uc');
ok($row_b);
ok($row_b->{MODE} == $row_a[0]);
ok($row_b->{SIZE} == $row_a[1]);
ok($row_b->{NAME} eq $row_a[2]);

print "fetchrow_hashref('ParamValues')\n";
ok($csr_b->execute());
ok(!defined eval { $csr_b->fetchrow_hashref('ParamValues') } ); # PurePerl croaks

print "FetchHashKeyName\n";
ok($csr_b->execute());
$row_b = $csr_b->fetchrow_hashref();
ok($row_b);
ok(keys(%$row_b) == 3);
ok($row_b->{MODE} == $row_a[0]);
ok($row_b->{SIZE} == $row_a[1]);
ok($row_b->{NAME} eq $row_a[2]);

print "fetchall_arrayref\n";
ok($csr_b->execute());
$r = $csr_b->fetchall_arrayref;
ok($r);
ok(@$r);
ok($r->[0]->[0] == $row_a[0]);
ok($r->[0]->[1] == $row_a[1]);
ok($r->[0]->[2] eq $row_a[2]);

print "fetchall_arrayref array slice\n";
ok($csr_b->execute());
$r = $csr_b->fetchall_arrayref([2,1]);
ok($r && @$r);
ok($r->[0]->[1] == $row_a[1]);
ok($r->[0]->[0] eq $row_a[2]);

print "fetchall_arrayref hash slice\n";
ok($csr_b->execute());
#$csr_b->trace(9);
$r = $csr_b->fetchall_arrayref({ SizE=>1, nAMe=>1});
ok($r && @$r);
ok($r->[0]->{SizE} == $row_a[1]);
ok($r->[0]->{nAMe} eq $row_a[2]);

#$csr_b->trace(4);
print "fetchall_arrayref hash\n";
ok($csr_b->execute());
$r = $csr_b->fetchall_arrayref({});
ok($r);
ok(keys %{$r->[0]} == 3);
ok("@{$r->[0]}{qw(MODE SIZE NAME)}" eq "@row_a", "'@{$r->[0]}{qw(MODE SIZE NAME)}' ne '@row_a'");
#$csr_b->trace(0);

# use Data::Dumper; warn Dumper([\@row_a, $r]);

$rows = $csr_b->rows;
ok($rows > 0, "row count $rows");
ok($rows == @$r, "$rows vs ".@$r);
ok($rows == $DBI::rows, "$rows vs $DBI::rows");
#$csr_b->trace(0);

# ---

print "selectrow_array\n";
@row_b = $dbh->selectrow_array($std_sql, undef, $dir);
ok(@row_b == 3);
ok("@row_b" eq "@row_a");

print "selectrow_hashref\n";
$r = $dbh->selectrow_hashref($std_sql, undef, $dir);
ok(keys %$r == 3);
ok($r->{MODE} eq $row_a[0]);
ok($r->{SIZE} eq $row_a[1]);
ok($r->{NAME} eq $row_a[2]);

print "selectall_arrayref\n";
$r = $dbh->selectall_arrayref($std_sql, undef, $dir);
ok($r);
ok(@{$r->[0]} == 3);
ok("@{$r->[0]}" eq "@row_a");
ok(@$r == $rows);

print "selectall_arrayref Slice array slice\n";
$r = $dbh->selectall_arrayref($std_sql, { Slice => [ 2, 0 ] }, $dir);
ok($r);
ok(@{$r->[0]} == 2);
ok("@{$r->[0]}" eq "$row_a[2] $row_a[0]", qq{"@{$r->[0]}" eq "$row_a[2] $row_a[0]"});
ok(@$r == $rows);

print "selectall_arrayref Columns array slice\n";
$r = $dbh->selectall_arrayref($std_sql, { Columns => [ 3, 1 ] }, $dir);
ok($r);
ok(@{$r->[0]} == 2);
ok("@{$r->[0]}" eq "$row_a[2] $row_a[0]", qq{"@{$r->[0]}" eq "$row_a[2] $row_a[0]"});
ok(@$r == $rows);

print "selectall_arrayref hash slice\n";
$r = $dbh->selectall_arrayref($std_sql, { Columns => { MoDe=>1, NamE=>1 } }, $dir);
ok($r);
ok(keys %{$r->[0]} == 2);
ok(exists $r->[0]{MoDe});
ok(exists $r->[0]{NamE});
ok($r->[0]{MoDe} eq $row_a[0]);
ok($r->[0]{NamE} eq $row_a[2]);
ok(@$r == $rows);

print "selectall_hashref\n";
$r = $dbh->selectall_hashref($std_sql, 'NAME', undef, $dir);
ok($r, "selectall_hashref result");
is(ref $r, 'HASH', "selectall_hashref HASH: ".ref $r);
is(scalar keys %$r, $rows);
is($r->{ $row_a[2] }{SIZE}, $row_a[1], qq{$r->{ $row_a[2] }{SIZE} eq $row_a[1]});

print "selectall_hashref by column number\n";
$r = $dbh->selectall_hashref($std_sql, 3, undef, $dir);
ok($r);
ok($r->{ $row_a[2] }{SIZE} eq $row_a[1], qq{$r->{ $row_a[2] }{SIZE} eq $row_a[1]});

print "selectcol_arrayref\n";
$r = $dbh->selectcol_arrayref($std_sql, undef, $dir);
ok($r);
ok(@$r == $rows);
ok($r->[0] eq $row_b[0]);

print "selectcol_arrayref column slice\n";
$r = $dbh->selectcol_arrayref($std_sql, { Columns => [3,2] }, $dir);
ok($r);
# use Data::Dumper; warn Dumper([\@row_b, $r]);
ok(@$r == $rows * 2);
ok($r->[0] eq $row_b[2]);
ok($r->[1] eq $row_b[1]);

# ---

print "begin_work...\n";
ok($dbh->{AutoCommit});
ok(!$dbh->{BegunWork});

ok($dbh->begin_work);
ok(!$dbh->{AutoCommit});
ok($dbh->{BegunWork});

$dbh->commit;
ok($dbh->{AutoCommit});
ok(!$dbh->{BegunWork});

ok($dbh->begin_work({}));
$dbh->rollback;
ok($dbh->{AutoCommit});
ok(!$dbh->{BegunWork});

# ---

print "others...\n";
my $csr_c;
$csr_c = $dbh->prepare("select unknown_field_name1 from ?");
ok(!defined $csr_c);
ok($DBI::errstr =~ m/Unknown field names: unknown_field_name1/);

print "RaiseError & PrintError & ShowErrorStatement\n";
$dbh->{RaiseError} = 1;
ok($dbh->{RaiseError});
$dbh->{ShowErrorStatement} = 1;
ok($dbh->{ShowErrorStatement});

my $error_sql = "select unknown_field_name2 from ?";

ok(! eval { $csr_c = $dbh->prepare($error_sql); 1; });
#print "$@\n";
ok($@ =~ m/\Q$error_sql/, $@); # ShowErrorStatement
ok($@ =~ m/.*Unknown field names: unknown_field_name2/, $@);

my $se_sth1 = $dbh->prepare("select mode from ?");
ok($se_sth1->{RaiseError});
ok($se_sth1->{ShowErrorStatement});

# check that $dbh->{Statement} tracks last _executed_ sth
ok($se_sth1->{Statement} eq "select mode from ?");
ok($dbh->{Statement}     eq "select mode from ?") or print "got: $dbh->{Statement}\n";
my $se_sth2 = $dbh->prepare("select name from ?");
ok($se_sth2->{Statement} eq "select name from ?");
ok($dbh->{Statement}     eq "select name from ?");
$se_sth1->execute('.');
ok($dbh->{Statement}     eq "select mode from ?");

# show error param values
ok(! eval { $se_sth1->execute('first','second') });	# too many params
ok($@ =~ /\b1='first'/, $@);
ok($@ =~ /\b2='second'/, $@);

$se_sth1->finish;
$se_sth2->finish;

$dbh->{RaiseError} = 0;
ok(!$dbh->{RaiseError});
$dbh->{ShowErrorStatement} = 0;
ok(!$dbh->{ShowErrorStatement});

{
  my @warn;
  local($SIG{__WARN__}) = sub { push @warn, @_ };
  $dbh->{PrintError} = 1;
  ok($dbh->{PrintError});
  ok(! $dbh->selectall_arrayref("select unknown_field_name3 from ?"));
  ok("@warn" =~ m/Unknown field names: unknown_field_name3/);
  $dbh->{PrintError} = 0;
  ok(!$dbh->{PrintError});
}


print "HandleError\n";
my $HandleErrorReturn;
my $HandleError = sub {
    my $msg = sprintf "HandleError: %s [h=%s, rv=%s, #=%d]",
		$_[0],$_[1],(defined($_[2])?$_[2]:'undef'),scalar(@_);
    die $msg   if $HandleErrorReturn < 0;
    print "$msg\n";
    $_[2] = 42 if $HandleErrorReturn == 2;
    return $HandleErrorReturn;
};

$dbh->{HandleError} = $HandleError;
ok($dbh->{HandleError});
ok($dbh->{HandleError} == $HandleError);

$dbh->{RaiseError} = 1;
$dbh->{PrintError} = 0;
$error_sql = "select unknown_field_name2 from ?";

print "HandleError -> die\n";
$HandleErrorReturn = -1;
ok(! eval { $csr_c = $dbh->prepare($error_sql); 1; });
ok($@ =~ m/^HandleError:/, $@);

print "HandleError -> 0 -> RaiseError\n";
$HandleErrorReturn = 0;
ok(! eval { $csr_c = $dbh->prepare($error_sql); 1; });
ok($@ =~ m/^DBD::(ExampleP|Multiplex)::db prepare failed:/, $@);

print "HandleError -> 1 -> return (original)undef\n";
$HandleErrorReturn = 1;
$r = eval { $csr_c = $dbh->prepare($error_sql); };
ok(!$@, $@);
ok(!defined($r), $r);

#$dbh->trace(4);

print "HandleError -> 2 -> return (modified)42\n";
$HandleErrorReturn = 2;
$r = eval { $csr_c = $dbh->prepare($error_sql); };
ok(!$@, $@);
ok($r==42) unless $dbh->{mx_handle_list} && ok(1); # skip for Multiplex

$dbh->{HandleError} = undef;
ok(!$dbh->{HandleError});

#$dbh->trace(0); die;

{
	# dump_results;
	my $sth = $dbh->prepare($std_sql);
	
	isa_ok($sth, "DBI::st");
	
	if ($haveFileSpec && length(File::Spec->updir)) {
	  ok($sth->execute(File::Spec->updir));
	} else {
	  ok($sth->execute('../'));
	}
	
	my $dump_dir = ($ENV{TMP}           || 
					$ENV{TEMP}          || 
					$ENV{TMPDIR}        || 
					$ENV{'SYS$SCRATCH'} || 
					'/tmp');
	my $dump_file = ($haveFileSpec) ? 
						File::Spec->catfile($dump_dir, 'dumpcsr.tst')
						: 
						"$dump_dir/dumpcsr.tst";
	($dump_file) = ($dump_file =~ m/^(.*)$/);	# untaint

	SKIP: {
		skip "# dump_results test skipped: unable to open $dump_file: $!\n", 2 unless (open(DUMP_RESULTS, ">$dump_file"));
		ok($sth->dump_results("10", "\n", ",\t", \*DUMP_RESULTS));
		close(DUMP_RESULTS);
		ok(-s $dump_file > 0);
	}

	is( unlink( $dump_file ), 1, "Remove $dump_file" );
	ok( !-e $dump_file, "Actually gone" );

}

print "table_info\n";
# First generate a list of all subdirectories
$dir = $haveFileSpec ? File::Spec->curdir() : ".";
ok(opendir(DIR, $dir));
my(%dirs, %unexpected, %missing);
while (defined(my $file = readdir(DIR))) {
    $dirs{$file} = 1 if -d $file;
}
closedir(DIR);
my $sth = $dbh->table_info(undef, undef, "%", "TABLE");
ok($sth);
%unexpected = %dirs;
%missing = ();
while (my $ref = $sth->fetchrow_hashref()) {
    if (exists($unexpected{$ref->{'TABLE_NAME'}})) {
		delete $unexpected{$ref->{'TABLE_NAME'}};
    } else {
		$missing{$ref->{'TABLE_NAME'}} = 1;
    }
}
ok(keys %unexpected == 0)
    or print "Unexpected directories: ", join(",", keys %unexpected), "\n";
ok(keys %missing == 0)
    or print "Missing directories: ", join(",", keys %missing), "\n";


print "tables\n";
my @tables_expected = (
    q{"schema"."table"},
    q{"sch-ema"."table"},
    q{"schema"."ta-ble"},
    q{"sch ema"."table"},
    q{"schema"."ta ble"},
);
my @tables = $dbh->tables(undef, undef, "%", "VIEW");
ok(@tables == @tables_expected, "Table count mismatch".@tables_expected." vs ".@tables);
ok($tables[$_] eq $tables_expected[$_], "$tables[$_] ne $tables_expected[$_]")
	foreach (0..$#tables_expected);


for (my $i = 0;  $i < 300;  $i += 100) {
	print "Testing the fake directories ($i).\n";
    ok($csr_a = $dbh->prepare("SELECT name, mode FROM long_list_$i"));
    ok($csr_a->execute(), $DBI::errstr);
    my $ary = $csr_a->fetchall_arrayref;
    ok(@$ary == $i, @$ary." rows instead of $i");
    if ($i) {
		my @n1 = map { $_->[0] } @$ary;
		my @n2 = reverse map { "file$_" } 1..$i;
		ok("@n1" eq "@n2", "'@n1' ne '@n2'");
    }
    else {
		ok(1);
    }
}


print "Testing \$dbh->func().\n";
my %tables;
unless ($dbh->{mx_handle_list}) {
	%tables = map { $_ =~ /lib/ ? ($_, 1) : () } $dbh->tables();
	foreach my $t ($dbh->func('lib', 'examplep_tables')) {
		defined(delete $tables{$t}) or print "Unexpected table: $t\n";
	}
}
ok((%tables == 0));

$dbh->disconnect;
ok(!$dbh->{Active});

1;
