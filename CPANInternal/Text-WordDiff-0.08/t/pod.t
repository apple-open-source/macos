#!/usr/bin/perl -w

use strict;
use Test::More;
eval "use Test::Pod 1.41";
plan skip_all => "Test::Pod 1.41 required for testing POD" if $@;
eval 'use Encode';
plan skip_all => 'Encode 1.20 required for testing POD because it has UTF-8 charactters' if $@;
all_pod_files_ok();
