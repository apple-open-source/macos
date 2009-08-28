#!perl -w

use strict;

use Test::More tests => 69;

## ----------------------------------------------------------------------------
## 08keeperr.t
## ----------------------------------------------------------------------------
# 
## ----------------------------------------------------------------------------

BEGIN {
    use_ok('DBI');
}   
 
$|=1;
$^W=1;

## ----------------------------------------------------------------------------
# subclass DBI

# DBI subclass
package My::DBI;
use base 'DBI';

# Database handle subclass
package My::DBI::db;
use base 'DBI::db';

# Statement handle subclass
package My::DBI::st;
use base 'DBI::st';

sub execute {
    my $sth = shift;
    # we localize an attribute here to check that the correpoding STORE
    # at scope exit doesn't clear any recorded error
    local $sth->{Warn} = 0;
    my $rv = $sth->SUPER::execute(@_);
    return $rv;
}


## ----------------------------------------------------------------------------
# subclass the subclass of DBI

package Test;

use strict;
use base 'My::DBI';

use DBI;

my @con_info = ('dbi:ExampleP:.', undef, undef, { PrintError => 0, RaiseError => 1 });

sub test_select {
  my $dbh = shift;
  eval { $dbh->selectrow_arrayref('select * from foo') };
  $dbh->disconnect;
  return $@;
}

my $err1 = test_select( My::DBI->connect(@con_info) );
Test::More::like($err1, qr/^DBD::(ExampleP|Multiplex|Gofer)::db selectrow_arrayref failed: opendir/, '... checking error');

my $err2 = test_select( DBI->connect(@con_info) );
Test::More::like($err2, qr/^DBD::(ExampleP|Multiplex|Gofer)::db selectrow_arrayref failed: opendir/, '... checking error');

package main;

## ----------------------------------------------------------------------------
print "Test HandleSetErr\n";

my $dbh = DBI->connect(@con_info);
isa_ok($dbh, "DBI::db");

$dbh->{RaiseError} = 1;
$dbh->{PrintError} = 1;
$dbh->{PrintWarn}  = 1;

# warning handler
my %warn = ( failed => 0, warning => 0 );
my @handlewarn = (0,0,0);
$SIG{__WARN__} = sub {
    my $msg = shift;
    if ($msg =~ /^DBD::\w+::\S+\s+(\S+)\s+(\w+)/) {
        ++$warn{$2};
        $msg =~ s/\n/\\n/g;
        print "warn: '$msg'\n";
        return;
    }
    warn $msg;
};

# HandleSetErr handler
$dbh->{HandleSetErr} = sub {
    my ($h, $err, $errstr, $state) = @_;
    return 0 
        unless defined $err;
    ++$handlewarn[ $err ? 2 : length($err) ]; # count [info, warn, err] calls
    return 1 
        if $state && $state eq "return";   # for tests
    ($_[1], $_[2], $_[3]) = (99, "errstr99", "OV123")
        if $state && $state eq "override"; # for tests
    return 0 
        if $err; # be transparent for errors
    local $^W;
    print "HandleSetErr called: h=$h, err=$err, errstr=$errstr, state=$state\n";
    return 0;
};

# start our tests 

ok(!defined $DBI::err, '... $DBI::err is not defined');

# ----

$dbh->set_err("", "(got info)");

ok(defined $DBI::err,                '... $DBI::err is defined');	# true
is($DBI::err,    "",                 '... $DBI::err is an empty string');
is($DBI::errstr, "(got info)",       '... $DBI::errstr is as we expected');
is($dbh->errstr, "(got info)",       '... $dbh->errstr matches $DBI::errstr');
cmp_ok($warn{failed},  '==', 0,      '... $warn{failed} is 0');
cmp_ok($warn{warning}, '==', 0,      '... $warn{warning} is 0');
is_deeply(\@handlewarn, [ 1, 0, 0 ], '... the @handlewarn array is (1, 0, 0)');

# ----

$dbh->set_err(0, "(got warn)", "AA001");	# triggers PrintWarn

ok(defined $DBI::err,                '... $DBI::err is defined');
is($DBI::err,    "0",                '... $DBI::err is "0"');
is($DBI::errstr, "(got info)\n(got warn)", 
                                     '... $DBI::errstr is as we expected');
is($dbh->errstr, "(got info)\n(got warn)", 
                                     '... $dbh->errstr matches $DBI::errstr');
is($DBI::state,  "AA001",            '... $DBI::state is AA001');
cmp_ok($warn{warning}, '==', 1,      '... $warn{warning} is 1');
is_deeply(\@handlewarn, [ 1, 1, 0 ], '... the @handlewarn array is (1, 1, 0)');


# ----

$dbh->set_err("", "(got more info)");		# triggers PrintWarn

ok(defined $DBI::err,                '... $DBI::err is defined');
is($DBI::err, "0",                   '... $DBI::err is "0"');	# not "", ie it's still a warn
is($dbh->err, "0",                   '... $dbh->err is "0"');
is($DBI::state, "AA001",             '... $DBI::state is AA001');
is($DBI::errstr, "(got info)\n(got warn)\n(got more info)", 
                                     '... $DBI::errstr is as we expected');
is($dbh->errstr, "(got info)\n(got warn)\n(got more info)", 
                                     '... $dbh->errstr matches $DBI::errstr');
cmp_ok($warn{warning}, '==', 2,      '... $warn{warning} is 2');
is_deeply(\@handlewarn, [ 2, 1, 0 ], '... the @handlewarn array is (2, 1, 0)');


# ----

$dbh->{RaiseError} = 0;
$dbh->{PrintError} = 1;

# ----

$dbh->set_err("42", "(got error)", "AA002");

ok(defined $DBI::err,                '... $DBI::err is defined');
cmp_ok($DBI::err,      '==', 42,     '... $DBI::err is 42');
cmp_ok($warn{warning}, '==', 2,      '... $warn{warning} is 2');
is($dbh->errstr, "(got info)\n(got warn)\n(got more info) [state was AA001 now AA002]\n(got error)", 
                                     '... $dbh->errstr is as we expected');
is($DBI::state, "AA002",             '... $DBI::state is AA002');
is_deeply(\@handlewarn, [ 2, 1, 1 ], '... the @handlewarn array is (2, 1, 1)');

# ----

$dbh->set_err("", "(got info)");

ok(defined $DBI::err,                '... $DBI::err is defined');
cmp_ok($DBI::err,      '==', 42,     '... $DBI::err is 42');
cmp_ok($warn{warning}, '==', 2,      '... $warn{warning} is 2');
is($dbh->errstr, "(got info)\n(got warn)\n(got more info) [state was AA001 now AA002]\n(got error)\n(got info)", 
                                     '... $dbh->errstr is as we expected');
is_deeply(\@handlewarn, [ 3, 1, 1 ], '... the @handlewarn array is (3, 1, 1)');

# ----

$dbh->set_err("0", "(got warn)"); # no PrintWarn because it's already an err

ok(defined $DBI::err,                '... $DBI::err is defined');
cmp_ok($DBI::err,      '==', 42,     '... $DBI::err is 42');
cmp_ok($warn{warning}, '==', 2,      '... $warn{warning} is 2');
is($dbh->errstr, "(got info)\n(got warn)\n(got more info) [state was AA001 now AA002]\n(got error)\n(got info)\n(got warn)", 
                                     '... $dbh->errstr is as we expected');
is_deeply(\@handlewarn, [ 3, 2, 1 ], '... the @handlewarn array is (3, 2, 1)');

# ----

$dbh->set_err("4200", "(got new error)", "AA003");

ok(defined $DBI::err,                '... $DBI::err is defined');
cmp_ok($DBI::err,      '==', 4200,   '... $DBI::err is 4200');
cmp_ok($warn{warning}, '==', 2,      '... $warn{warning} is 2');
is($dbh->errstr, "(got info)\n(got warn)\n(got more info) [state was AA001 now AA002]\n(got error)\n(got info)\n(got warn) [err was 42 now 4200] [state was AA002 now AA003]\n(got new error)", 
                                     '... $dbh->errstr is as we expected');
is_deeply(\@handlewarn, [ 3, 2, 2 ], '... the @handlewarn array is (3, 2, 2)');

# ----

$dbh->set_err(undef, "foo", "bar"); # clear error

ok(!defined $dbh->errstr, '... $dbh->errstr is defined');
ok(!defined $dbh->err,    '... $dbh->err is defined');
is($dbh->state, "",       '... $dbh->state is an empty string');

# ----

%warn = ( failed => 0, warning => 0 );
@handlewarn = (0,0,0);

# ----

my @ret;
@ret = $dbh->set_err(1, "foo");		# PrintError

cmp_ok(scalar(@ret), '==', 1,         '... only returned one value');
ok(!defined $ret[0],                  '... the first value is undefined');
ok(!defined $dbh->set_err(2, "bar"),  '... $dbh->set_err returned undefiend');	# PrintError
ok(!defined $dbh->set_err(3, "baz"),  '... $dbh->set_err returned undefiend');	# PrintError
ok(!defined $dbh->set_err(0, "warn"), '... $dbh->set_err returned undefiend');	# PrintError
is($dbh->errstr, "foo [err was 1 now 2]\nbar [err was 2 now 3]\nbaz\nwarn", 
                                      '... $dbh->errstr is as we expected');
is($warn{failed}, 4,                  '... $warn{failed} is 4');
is_deeply(\@handlewarn, [ 0, 1, 3 ],  '... the @handlewarn array is (0, 1, 3)');

# ----

$dbh->set_err(undef, undef, undef);	# clear error

@ret = $dbh->set_err(1, "foo", "AA123", "method");
cmp_ok(scalar @ret, '==', 1,   '... only returned one value');
ok(!defined $ret[0],           '... the first value is undefined');

@ret = $dbh->set_err(1, "foo", "AA123", "method", "42");
cmp_ok(scalar @ret, '==', 1,   '... only returned one value');
is($ret[0], "42",              '... the first value is "42"');

@ret = $dbh->set_err(1, "foo", "return");
cmp_ok(scalar @ret, '==', 0,   '... returned no values');

# ----

$dbh->set_err(undef, undef, undef);	# clear error

@ret = $dbh->set_err("", "info", "override");
cmp_ok(scalar @ret, '==', 1, '... only returned one value');
ok(!defined $ret[0],         '... the first value is undefined');
cmp_ok($dbh->err, '==', 99,  '... $dbh->err is 99');
is($dbh->errstr, "errstr99", '... $dbh->errstr is as we expected');
is($dbh->state,  "OV123",    '... $dbh->state is as we expected');

$dbh->disconnect;

1;
# end
