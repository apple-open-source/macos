#!perl -w                                         # -*- perl -*-
# vim:sw=4:ts=8
$|=1;

use strict;
use warnings;

use DBI;
use Data::Dumper;
use Test::More;
sub between_ok;

# here we test the DBI_GOFER_RANDOM mechanism
# and how gofer deals with failures

plan skip_all => "requires Callbacks which are not supported with PurePerl" if $DBI::PurePerl;

if (my $ap = $ENV{DBI_AUTOPROXY}) { # limit the insanity
    plan skip_all => "Gofer DBI_AUTOPROXY" if $ap =~ /^dbi:Gofer/i;

    # this means we have DBD::Gofer => DBD::Gofer => DBD::whatever
    # rather than disable it we let it run because we're twisted
    # and because it helps find more bugs (though debugging can be painful)
    warn "\n$0 is running with DBI_AUTOPROXY enabled ($ENV{DBI_AUTOPROXY})\n"
        unless $0 =~ /\bzv/; # don't warn for t/zvg_85gofer.t
}

plan 'no_plan';

my $tmp;
my $dbh;
my $fails;

# we'll use the null transport for simplicity and speed
# and the rush policy to limit the number of interactions with the gofer executor

# silence the "DBI_GOFER_RANDOM..." warnings
my @warns;
$SIG{__WARN__} = sub { ("@_" =~ /^DBI_GOFER_RANDOM/) ? push(@warns, @_) : warn @_; };

# --- 100% failure rate

($fails, $dbh) = trial_impact("fail=100%,do", 10, "", sub { $_->do("set foo=1") });
is $fails, 100, 'should fail 100% of the time';
ok   $@, '$@ should be set';
like $@, '/fake error from do method induced by DBI_GOFER_RANDOM/';
ok   $dbh->errstr, 'errstr should be set';
like $dbh->errstr, '/DBI_GOFER_RANDOM/', 'errstr should contain DBI_GOFER_RANDOM';
ok !$dbh->{go_response}->executed_flag_set, 'go_response executed flag should be false';


# XXX randomness can't be predicted, so it's just possible these will fail
srand(42); # try to limit occasional failures (effect will vary by platform etc)

sub trial_impact {
    my ($spec, $count, $dsn_attr, $code, $verbose) = @_;
    local $ENV{DBI_GOFER_RANDOM} = $spec;
    my $dbh = dbi_connect("policy=rush;$dsn_attr");
    local $_ = $dbh;
    my $fail_percent = percentage_exceptions(200, $code, $verbose);
    return $fail_percent unless wantarray;
    return ($fail_percent, $dbh);
}

# --- 50% failure rate, with no retries

$fails = trial_impact("fail=50%,do", 200, "retry_limit=0", sub { $_->do("set foo=1") });
print "target approx 50% random failures, got $fails%\n";
between_ok $fails, 10, 90, "should fail about 50% of the time, but at least between 10% and 90%";

# --- 50% failure rate, with many retries (should yield low failure rate)

$fails = trial_impact("fail=50%,prepare", 200, "retry_limit=5", sub { $_->prepare("set foo=1") });
print "target less than 20% effective random failures (ideally 0), got $fails%\n";
cmp_ok $fails, '<', 20, 'should fail < 20%';

# --- 10% failure rate, with many retries (should yield zero failure rate)

$fails = trial_impact("fail=10,do", 200, "retry_limit=10", sub { $_->do("set foo=1") });
cmp_ok $fails, '<', 1, 'should fail < 1%';

# --- 50% failure rate, test is_idempotent

$ENV{DBI_GOFER_RANDOM} = "fail=50%,do";   # 50%

# test go_retry_hook and that ReadOnly => 1 retries a non-idempotent statement
ok my $dbh_50r1ro = dbi_connect("policy=rush;retry_limit=1", {
    go_retry_hook => sub { return ($_[0]->is_idempotent) ? 1 : 0 },
    ReadOnly => 1,
} );
between_ok percentage_exceptions(100, sub { $dbh_50r1ro->do("set foo=1") }),
    10, 40, 'should fail ~25% (ie 50% with one retry)';
between_ok $dbh_50r1ro->{go_transport}->meta->{request_retry_count},
    20, 80, 'transport request_retry_count should be around 50';

# test as above but with ReadOnly => 0
ok my $dbh_50r1rw = dbi_connect("policy=rush;retry_limit=1", {
    go_retry_hook => sub { return ($_[0]->is_idempotent) ? 1 : 0 },
    ReadOnly => 0,
} );
between_ok percentage_exceptions(100, sub { $dbh_50r1rw->do("set foo=1") }),
    20, 80, 'should fail ~50%, ie no retries';
ok !$dbh_50r1rw->{go_transport}->meta->{request_retry_count},
    'transport request_retry_count should be zero or undef';


# --- check random is random and non-random is non-random

my %fail_percents;
for (1..5) {
    $fails = trial_impact("fail=50%,do", 10, "", sub { $_->do("set foo=1") });
    ++$fail_percents{$fails};
}
cmp_ok scalar keys %fail_percents, '>=', 2, 'positive percentage should fail randomly';

%fail_percents = ();
for (1..5) {
    $fails = trial_impact("fail=-50%,do", 10, "", sub { $_->do("set foo=1") });
    ++$fail_percents{$fails};
}
is scalar keys %fail_percents, 1, 'negative percentage should fail non-randomly';

# ---
print "Testing random delay\n";

$ENV{DBI_GOFER_RANDOM} = "delay0.1=51%,do"; # odd percentage to force warn()s
@warns = ();
ok $dbh = dbi_connect("policy=rush;retry_limit=0");
is percentage_exceptions(20, sub { $dbh->do("set foo=1") }),
    0, "should not fail for DBI_GOFER_RANDOM='$ENV{DBI_GOFER_RANDOM}'";
my $delays = grep { m/delaying execution/ } @warns;
between_ok $delays, 1, 19, 'should be delayed around 5 times';

exit 0;

# --- subs ---
#
sub between_ok {
    my ($got, $min, $max, $label) = @_;
    local $Test::Builder::Level = 2;
    cmp_ok $got, '>=', $min, "$label (got $got)";
    cmp_ok $got, '<=', $max, "$label (got $got)";
}

sub dbi_connect {
    my ($gdsn, $attr) = @_;
    return DBI->connect("dbi:Gofer:transport=null;$gdsn;dsn=dbi:ExampleP:", 0, 0, {
        RaiseError => 1, PrintError => 0, ($attr) ? %$attr : ()
    });
}

sub percentage_exceptions {
    my ($count, $sub, $verbose) = @_;
    my $i = $count;
    my $exceptions = 0;
    while ($i--) {
        eval { $sub->() };
        warn sprintf("percentage_exceptions $i: %s\n", $@|| $DBI::errstr || '')  if $verbose;
        if ($@) {
            die "Unexpected failure: $@" unless $@ =~ /DBI_GOFER_RANDOM/;
            ++$exceptions;
        }
    }
    warn sprintf "percentage_exceptions %f/%f*100 = %f\n",
        $exceptions, $count, $exceptions/$count*100
        if $verbose;
    return $exceptions/$count*100;
}
