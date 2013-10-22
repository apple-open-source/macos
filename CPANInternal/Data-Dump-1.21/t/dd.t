#!perl -w

use strict;
use Test;
plan tests => 1;

use Data::Dump;

print "# ";
dd getlogin;
ddx localtime;
ddx \%Exporter::;

ok(1);
