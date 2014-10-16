#!perl -w

use strict;
use Test::More;
eval "use Test::YAML::Meta";
plan skip_all => "Test::YAML::Meta required for testing META.yml" if $@;
meta_yaml_ok();
