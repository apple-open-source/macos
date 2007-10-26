#!/usr/bin/perl

use Test;
plan tests => 2;

ok 1;

require Module::Build;
ok $INC{'Module/Build.pm'}, qr/blib/, 'Module::Build should be loaded from blib';
print "# Cwd: ", Module::Build->cwd, "\n";
print "# \@INC: (@INC)\n";
print "Done.\n";  # t/compat.t looks for this
