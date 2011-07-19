#!perl -T
use strict;
use warnings;

use Test::More tests => 4;

BEGIN { use_ok("Sub::Exporter::Util", qw(curry_chain)); }

# So, some packages that we'll chain methods through.
{
  package Test::CurryChain::Head;
  sub new          { my ($class, @arg) = @_; bless [ @arg ] => $class; }
  sub next_obj     { shift; return Test::CurryChain::Tail->new(@_); }
  sub false        { return; }
  sub non_invocant { return 1; }

  package Test::CurryChain::Tail;
  sub new      { my ($class, @arg) = @_; bless [ @arg ] => $class; }
  sub rev_guts { return reverse @{shift()}; }
}

{
  # Then the generator which could be put into a Sub::Exporter -setup.
  # This is an optlist.  AREF = args; undef = no args; CODE = args generator
  my $generator = curry_chain(
    next_obj => [ 1, 2, 3 ],
    rev_guts => undef,
  );

  my $curried_sub = $generator->('Test::CurryChain::Head');
  my @result = $curried_sub->();
  is_deeply(
    \@result,
    [ 3, 2, 1],
    "simple curried chain behaves as expected"
  );
}

{
  # This one will fail, beacuse the second call returns false.
  my $generator = curry_chain(
    new       => [ 1, 2, 3 ],
    false     => undef,
    will_fail => undef,
  );

  my $curried_sub = $generator->('Test::CurryChain::Head');

  eval { $curried_sub->() };

  like($@, qr/can't call will_fail/, "exception on broken chain");
}

{
  # This one will fail, beacuse the second call returns a true non-invocant.
  my $generator = curry_chain(
    new          => [ 1, 2, 3 ],
    non_invocant => undef,
    will_fail    => undef,
  );

  my $curried_sub = $generator->('Test::CurryChain::Head');

  eval { $curried_sub->() };

  like($@, qr/can't call will_fail/, "exception on broken chain");
}

