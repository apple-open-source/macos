#!/bin/env perl 
#!d:\perl\bin\perl.exe 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use Test::Harness;

@ARGV = sort <*.t> unless @ARGV;

runtests(@ARGV);