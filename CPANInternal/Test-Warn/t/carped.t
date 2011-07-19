#!/usr/bin/perl

use strict;
use warnings;

use Carp;

use Test::Builder::Tester tests => 6;
Test::Builder::Tester::color 'on';
use Test::Warn;

sub foo {
    warn "Warning 1";
    carp "Carping 2";
    carp "Carping 3";
    warn "Warning 4";
}

#use File::Spec;
#my $tcarped = File::Spec->catfile('t','carped.t');
#$tcarped =~ s/\\/\//g if $^O eq 'MSWin32';
#also will not work on VMS
my $tcarped = 't/carped.t';

test_out "ok 1";
warnings_like {foo()} [map {qr/$_/} (1 .. 4)];
test_test "Warnings and Carpings mixed, asked only for like warnings";

test_out "not ok 1";
test_fail +10;
test_diag 
"found warning: Warning 1 at $tcarped line 13.",
"found carped warning: Carping 2 at $tcarped line 14",
"found carped warning: Carping 3 at $tcarped line 15",
"found warning: Warning 4 at $tcarped line 16.",
"expected to find carped warning: (?-xism:1)",
"expected to find carped warning: (?-xism:2)",
"expected to find carped warning: (?-xism:3)",
"expected to find carped warning: (?-xism:4)";
warnings_like {foo()} [{carped => [map {qr/$_/} (1 .. 4)]}];
test_test "Warnings and Carpings mixed, asked only for like carpings";

test_out "ok 1";
warnings_like {foo()} [qr/1/, {carped => [qr/2/, qr/3/]}, qr/4/];
test_test "Warnings and Carpings mixed, asked for the right likes";

my @msg = ("Warning 1", "Carping 2", "Carping 3", "Warning 4");
test_out "ok 1";
warnings_are {foo()} \@msg;
test_test "Warnings and Carpings mixed, asked only for warnings";

test_out "not ok 1";
test_fail +10;
test_diag 
"found warning: Warning 1 at $tcarped line 13.",
"found carped warning: Carping 2 at $tcarped line 14",
"found carped warning: Carping 3 at $tcarped line 15",
"found warning: Warning 4 at $tcarped line 16.",
"expected to find carped warning: Warning 1",
"expected to find carped warning: Carping 2",
"expected to find carped warning: Carping 3",
"expected to find carped warning: Warning 4";
warnings_are {foo()} {carped => \@msg};
test_test "Warnings and Carpings mixed, asked only for carpings";

test_out "ok 1";
warnings_are {foo()} [$msg[0], {carped => [@msg[1..2]]}, $msg[3]];
test_test "Warnings and Carpings mixed, asked for the right ones";
