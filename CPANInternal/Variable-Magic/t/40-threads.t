#!perl -T

use strict;
use warnings;

sub skipall {
 my ($msg) = @_;
 require Test::More;
 Test::More::plan(skip_all => $msg);
}

use Config qw/%Config/;

BEGIN {
 my $t_v  = '1.67';
 my $ts_v = '1.14';
 skipall 'This perl wasn\'t built to support threads'
                                                    unless $Config{useithreads};
 skipall "threads $t_v required to test thread safety"
                                              unless eval "use threads $t_v; 1";
 skipall "threads::shared $ts_v required to test thread safety"
                                     unless eval "use threads::shared $ts_v; 1";
}

use Test::More; # after threads

use Variable::Magic qw/wizard cast dispell getdata VMG_THREADSAFE VMG_OP_INFO_NAME VMG_OP_INFO_OBJECT/;

BEGIN {
 skipall 'This Variable::Magic isn\'t thread safe' unless VMG_THREADSAFE;
 plan tests => (4 * 18 + 1) + (4 * 13 + 1);
 my $v = $threads::VERSION;
 diag "Using threads $v" if defined $v;
 $v = $threads::shared::VERSION;
 diag "Using threads::shared $v" if defined $v;
}

my $destroyed : shared = 0;

sub try {
 my ($dispell, $op_info) = @_;
 my $tid = threads->tid();
 my $c   = 0;
 my $wiz = eval {
  wizard data    => sub { $_[1] + $tid },
         get     => sub { ++$c; 0 },
         set     => sub {
                     my $op = $_[-1];
                     if ($op_info == VMG_OP_INFO_OBJECT) {
                      is_deeply { class => ref($op),   name => $op->name },
                                { class => 'B::BINOP', name => 'sassign' },
                                "op object in thread $tid is correct";
                     } else {
                      is $op, 'sassign', "op name in thread $tid is correct";
                     }
                     0
                    },
         free    => sub { lock $destroyed; ++$destroyed; 0 },
         op_info => $op_info
 };
 is($@,     '',    "wizard in thread $tid doesn't croak");
 isnt($wiz, undef, "wizard in thread $tid is defined");
 is($c,     0,     "wizard in thread $tid doesn't trigger magic");
 my $a = 3;
 my $res = eval { cast $a, $wiz, sub { 5 }->() };
 is($@, '', "cast in thread $tid doesn't croak");
 is($c, 0,  "cast in thread $tid doesn't trigger magic");
 my $b;
 eval { $b = $a };
 is($@, '', "get in thread $tid doesn't croak");
 is($b, 3,  "get in thread $tid returns the right thing");
 is($c, 1,  "get in thread $tid triggers magic");
 my $d = eval { getdata $a, $wiz };
 is($@, '',       "getdata in thread $tid doesn't croak");
 is($d, 5 + $tid, "getdata in thread $tid returns the right thing");
 is($c, 1,        "getdata in thread $tid doesn't trigger magic");
 eval { $a = 9 };
 is($@, '', "set in thread $tid (check opname) doesn't croak");
 if ($dispell) {
  $res = eval { dispell $a, $wiz };
  is($@, '', "dispell in thread $tid doesn't croak");
  is($c, 1,  "dispell in thread $tid doesn't trigger magic");
  undef $b;
  eval { $b = $a };
  is($@, '', "get in thread $tid after dispell doesn't croak");
  is($b, 9,  "get in thread $tid after dispell returns the right thing");
  is($c, 1,  "get in thread $tid after dispell doesn't trigger magic");
 }
 return; # Ugly if not here
}

for my $dispell (1, 0) {
 {
  lock $destroyed;
  $destroyed = 0;
 }

 my @t = map { threads->create(\&try, $dispell, $_) }
                              (VMG_OP_INFO_NAME) x 2, (VMG_OP_INFO_OBJECT) x 2;
 $_->join for @t;

 {
  lock $destroyed;
  is $destroyed, (1 - $dispell) * 4, 'destructors';
 }
}
