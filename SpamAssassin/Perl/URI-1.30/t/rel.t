#!/usr/bin/perl -w

print "1..4\n";

use strict;
use URI;

my $uri = URI->new("http://www.example.com/foo/bar/");

print "not " unless $uri->rel("http://www.example.com/foo/bar/") eq "./";
print "ok 1\n";

print "not " unless $uri->rel("HTTP://WWW.EXAMPLE.COM/foo/bar/") eq "./";
print "ok 2\n";

print "not " unless $uri->rel("HTTP://WWW.EXAMPLE.COM/FOO/BAR/") eq "../../foo/bar/";
print "ok 3\n";

print "not " unless $uri->rel("HTTP://WWW.EXAMPLE.COM:80/foo/bar/") eq "./";
print "ok 4\n";

