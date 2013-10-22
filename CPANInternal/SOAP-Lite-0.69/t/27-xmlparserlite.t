#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

unless (eval { require XML::Parser::Lite }) {
  print "1..0 # Skip: ", $@, "\n"; 
  exit;
}

plan tests => 17;

my($s, $c, $e, $a);

($s, $c, $e) = (0) x 3;
my $p1 = new XML::Parser::Lite;
$p1->setHandlers(
  Start => sub { shift; $s++; print "start: @_\n" },
  Char => sub { shift; $c++; print "char: @_\n" },
  End => sub { shift; $e++; print "end: @_\n" },
);
$p1->parse('<foo id="me">Hello World!</foo>');
ok(1 => 1);
ok($s == 1);
ok($c == 1);
ok($e == 1);

($s, $c, $e) = (0) x 3;
my %foo;
my $p2 = new XML::Parser::Lite
  Handlers => {
    Start => sub { shift; $s++; %foo = @_[1..$#_] if $_[0] eq 'foo'; print "start: @_\n" },
    Char => sub { shift; $c++; print "char: @_\n" },
    End => sub { shift; $e++; print "end: @_\n" },
  }
;
$p2->parse('<foo id="me" root="0" empty="">Hello <bar>cruel</bar> <foobar/> World!</foo>');
ok(1 => 1);
ok($s == 3);
ok($c == 4);
ok($e == 3);
ok($foo{id} eq 'me');
ok(defined $foo{root} && $foo{root} eq '0');
ok(defined $foo{empty} && $foo{empty} eq '');

$p2->setHandlers;

# check for junk before
eval { $p2->parse('foo<foo id="me">Hello World!</foo>') };
ok($@ =~ /^junk .+ before/);

# check for junk after
eval { $p2->parse('<foo id="me">Hello World!</foo>bar') };
ok($@ =~ /^junk .+ after/);

# check for non-closed tag
eval { $p2->parse('<foo id="me">Hello World!') };
ok($@ =~ /^not properly closed tag 'foo'/);

# check for non properly closed tag
eval { $p2->parse('<foo id="me">Hello World!<bar></foo></bar>') };
ok($@ =~ /^mismatched tag 'foo'/);

# check for unwanted tag
eval { $p2->parse('<foo id="me">Hello World!</foo><bar></bar>') };
ok($@ =~ /^multiple roots, wrong element 'bar'/);

# check for string without elements
eval { $p2->parse('  ') };
ok($@ =~ /^no element found/);
