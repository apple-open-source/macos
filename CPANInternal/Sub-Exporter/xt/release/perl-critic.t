#!perl
use strict;
use warnings;
use Test::More;

if ($ENV{PERL_TEST_CRITIC}) {
  if (eval { require Test::Perl::Critic }) {
    Test::Perl::Critic::all_critic_ok();
  } else {
    plan skip_all => "couldn't load Test::Perl::Critic";
  }
} else {
  plan skip_all => "define PERL_TEST_CRITIC to run these tests";
}
