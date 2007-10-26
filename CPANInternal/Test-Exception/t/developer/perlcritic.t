#! /usr/bin/perl -Tw

use strict;
use warnings;

use Test::More;
eval "use Test::Perl::Critic (-profile => 't/developer/perlcriticrc')";
plan skip_all => "Test::Perl::Critic required for criticism" if $@;
all_critic_ok();


