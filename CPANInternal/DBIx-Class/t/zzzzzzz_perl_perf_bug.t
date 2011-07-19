use strict;
use warnings;
use Test::More;
use Benchmark;
use lib qw(t/lib);
use DBICTest; # do not remove even though it is not used

# This is a rather unusual test.
# It does not test any aspect of DBIx::Class, but instead tests the
# perl installation this is being run under to see if it is:-
#  1. Potentially affected by a RH perl build bug
#  2. If so we do a performance test for the effect of
#     that bug.
#
# You can skip these tests by setting the DBIC_NO_WARN_BAD_PERL env
# variable
#
# If these tests fail then please read the section titled
# Perl Performance Issues on Red Hat Systems in
# L<DBIx::Class::Manual::Troubleshooting>

plan skip_all =>
  'Skipping RH perl performance bug tests as DBIC_NO_WARN_BAD_PERL set'
  if ( $ENV{DBIC_NO_WARN_BAD_PERL} );

plan skip_all => 'Skipping as AUTOMATED_TESTING is set'
  if ( $ENV{AUTOMATED_TESTING} );

plan tests => 3;

ok( 1, 'Dummy - prevents next test timing out' );

# we do a benchmark test filling an array with blessed/overloaded references,
# against an array filled with array refs.
# On a sane system the ratio between these operation sets is 1 - 1.5,
# whereas a bugged system gives a ratio of around 8
# we therefore consider there to be a problem if the ratio is >= 2

my $results = timethese(
    -1,    # run for 1 CPU second each
    {
        no_bless => sub {
            my %h;
            for ( my $i = 0 ; $i < 10000 ; $i++ ) {
                $h{$i} = [];
            }
        },
        bless_overload => sub {
            use overload q(<) => sub { };
            my %h;
            for ( my $i = 0 ; $i < 10000 ; $i++ ) {
                $h{$i} = bless [] => 'main';
            }
        },
    },
);

my $ratio = $results->{no_bless}->iters / $results->{bless_overload}->iters;

ok( ( $ratio < 2 ), 'Overload/bless performance acceptable' )
  || diag(
    "\n",
    "This perl has a substantial slow down when handling large numbers\n",
    "of blessed/overloaded objects.  This can severely adversely affect\n",
    "the performance of DBIx::Class programs.  Please read the section\n",
    "in the Troubleshooting POD documentation entitled\n",
    "'Perl Performance Issues on Red Hat Systems'\n",
    "As this is an extremely serious condition, the only way to skip\n",
    "over this test is to --force the installation, or to look in the test\n",
    "file " . __FILE__ . "\n",
  );

# We will only check for the difference in bless handling (whether the
# bless applies to the reference or the referent) if we have seen a
# performance issue...

SKIP: {
    skip "Not checking for bless handling as performance is OK", 1
      if ( $ratio < 2 );

    {
        package    # don't want this in PAUSE
          TestRHBug;
        use overload bool => sub { 0 }
    }

    sub _has_bug_34925 {
        my %thing;
        my $r1 = \%thing;
        my $r2 = \%thing;
        bless $r1 => 'TestRHBug';
        return !!$r2;
    }

    sub _possibly_has_bad_overload_performance {
        return $] < 5.008009 && !_has_bug_34925();
    }

    # If this next one fails then you almost certainly have a RH derived
    # perl with the performance bug
    # if this test fails, look at the section titled
    # "Perl Performance Issues on Red Hat Systems" in
    # L<DBIx::Class::Manual::Troubleshooting>
    # Basically you may suffer severe performance issues when running
    # DBIx::Class (and many other) modules.  Look at getting a fixed
    # version of the perl interpreter for your system.
    #
    ok( !_possibly_has_bad_overload_performance(),
        'Checking whether bless applies to reference not object' )
      || diag(
        "\n",
        "This perl is probably derived from a buggy Red Hat perl build\n",
        "Please read the section in the Troubleshooting POD documentation\n",
        "entitled 'Perl Performance Issues on Red Hat Systems'\n",
        "As this is an extremely serious condition, the only way to skip\n",
        "over this test is to --force the installation, or to look in the test\n",
        "file " . __FILE__ . "\n",
      );
}
