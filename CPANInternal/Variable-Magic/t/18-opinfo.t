#!perl -T

use strict;
use warnings;

use Test::More tests => 17 * (3 + 4) + 5;

use Config qw/%Config/;

use Variable::Magic qw/wizard cast dispell VMG_OP_INFO_NAME VMG_OP_INFO_OBJECT/;

sub Variable::Magic::TestPkg::foo { }

my $is_5130_release = ($] == 5.013 && !$Config{git_describe}) ? 1 : 0;

my $aelem     = $] <= 5.008003 ? 'aelem'
                               : ($] < 5.013 or $is_5130_release) ? 'aelemfast'
                                                                  : 'sassign';
my $aelemf    = ($] < 5.013 or $is_5130_release) ? 'aelemfast' : 'sassign';
my $aelemf_op = $aelemf eq 'sassign'
                   ? 'B::BINOP' : $Config{useithreads} ? 'B::PADOP' : 'B::SVOP';

our @o;

my @tests = (
 [ 'len', '@c',    'my @c',     'my $x = @c',      [ 'padav',   'B::OP'     ] ],
 [ 'get', '$c[0]', 'my @c',     'my $x = $c[0]',   [ $aelem,    'B::OP'     ] ],
 [ 'get', '$o[0]', 'local @o',  'my $x = $o[0]',   [ $aelemf,   $aelemf_op  ] ],
 [ 'get', '$c',    'my $c = 1', '++$c',            [ 'preinc',  'B::UNOP'   ] ],
 [ 'get', '$c',    'my $c = 1', '$c ** 2',         [ 'pow',     'B::BINOP'  ] ],
 [ 'get', '$c',    'my $c = 1', 'my $x = $c',      [ 'sassign', 'B::BINOP'  ] ],
 [ 'get', '$c',    'my $c = 1', '1 if $c',         [ 'and',     'B::LOGOP'  ] ],
 [ 'get', '$c',    'my $c = []','ref $c',          [ 'ref',     'B::UNOP'   ] ],
 [ 'get', '$c',    'my $c = $0','-f $c',           [ 'ftfile',  'B::UNOP'   ] ],
 [ 'get', '$c',    'my $c = "Z"',
                   'my $i = 1; Z:goto $c if $i--', [ 'goto',    'B::UNOP'   ] ],
 [ 'set', '$c',    'my $c = 1', 'bless \$c, "main"',
                                                   [ 'bless',   'B::LISTOP' ] ],
 [ 'get', '$c',    'my $c = ""','$c =~ /x/',       [ 'match',   'B::PMOP'   ] ],
 [ 'get', '$c',    'my $c = "Variable::Magic::TestPkg"',
                                '$c->foo()',  [ 'method_named', 'B::SVOP'   ] ],
 [ 'get', '$c',    'my $c = ""','$c =~ y/x/y/',    [ 'trans',   'B::PVOP'   ] ],
 [ 'get', '$c',    'my $c = 1', '1 for 1 .. $c',
                                                 [ 'enteriter', 'B::LOOP'   ] ],
 [ 'free','$c',    'my $c = 1', 'last',            [ 'last',    'B::OP'     ] ],
 [ 'free','$c', 'L:{my $c = 1', 'last L}',         [ 'last',    'B::OP'     ] ],
);

our $done;

for (@tests) {
 my ($key, $var, $init, $test, $exp) = @$_;

 for my $op_info (VMG_OP_INFO_NAME, VMG_OP_INFO_OBJECT) {
  my $wiz;

  # We must test for the $op correctness inside the callback because, if we
  # bring it out, it will go outside of the eval STRING scope, and what it
  # points to will no longer exist.
  eval {
   $wiz = wizard $key => sub {
    return if $done;
    my $op = $_[-1];
    my $desc = "$key magic with op_info == $op_info";
    if ($op_info == VMG_OP_INFO_NAME) {
     is $op, $exp->[0], "$desc gets the right op info";
    } elsif ($op_info == VMG_OP_INFO_OBJECT) {
     isa_ok $op, $exp->[1], $desc;
     is $op->name, $exp->[0], "$desc gets the right op info";
    } else {
     is $op, undef, "$desc gets the right op info";
    }
    $done = 1;
    ()
   }, op_info => $op_info
  };
  is $@, '', "$key wizard with op_info == $op_info doesn't croak";

  local $done = 0;

  my $testcase = "{ $init; cast $var, \$wiz; $test }";

  eval $testcase;
  is $@, '', "$key magic with op_info == $op_info doesn't croak";
  diag $testcase if $@;
 }
}

{
 my $c;

 my $wiz = eval {
  wizard get => sub {
    is $_[-1], undef, 'get magic with out of bounds op_info';
   },
   op_info => 3;
 };
 is $@, '', "get wizard with out of bounds op_info doesn't croak";

 eval { cast $c, $wiz };
 is $@, '', "get cast with out of bounds op_info doesn't croak";

 eval { my $x = $c };
 is $@, '', "get magic with out of bounds op_info doesn't croak";

 eval { dispell $c, $wiz };
 is $@, '', "get dispell with out of bounds op_info doesn't croak";
}
