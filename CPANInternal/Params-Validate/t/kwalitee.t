use strict;
use warnings;

use Test::More;


plan skip_all => 'This test is only run for the module author'
    unless -d '.svn' || $ENV{IS_MAINTAINER};

eval { require Test::Kwalitee; Test::Kwalitee->import() };
plan skip_all => "Test::Kwalitee needed for testing kwalitee"
    if $@;
