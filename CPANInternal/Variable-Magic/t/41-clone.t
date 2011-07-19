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
 plan tests => 2 * 3 + 2 * (2 * 10 + 2) + 2 * (2 * 7 + 2);
 my $v = $threads::VERSION;
 diag "Using threads $v" if defined $v;
 $v = $threads::shared::VERSION;
 diag "Using threads::shared $v" if defined $v;
}

my $destroyed : shared = 0;
my $c         : shared = 0;

sub spawn_wiz {
 my ($op_info) = @_;

 my $wiz = eval {
  wizard data    => sub { $_[1] + threads->tid() },
         get     => sub { lock $c; ++$c; 0 },
         set     => sub {
                     my $op = $_[-1];
                     my $tid = threads->tid();
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
 is($@,     '',    "wizard with op_info $op_info in main thread doesn't croak");
 isnt($wiz, undef, "wizard with op_info $op_info in main thread is defined");
 is($c,     0,     "wizard with op_info $op_info in main thread doesn't trigger magic");

 return $wiz;
}

sub try {
 my ($dispell, $wiz) = @_;
 my $tid = threads->tid();
 my $a   = 3;
 my $res = eval { cast $a, $wiz, sub { 5 }->() };
 is($@, '', "cast in thread $tid doesn't croak");
 my $b;
 eval { $b = $a };
 is($@, '', "get in thread $tid doesn't croak");
 is($b, 3,  "get in thread $tid returns the right thing");
 my $d = eval { getdata $a, $wiz };
 is($@, '',       "getdata in thread $tid doesn't croak");
 is($d, 5 + $tid, "getdata in thread $tid returns the right thing");
 eval { $a = 9 };
 is($@, '', "set in thread $tid (check opname) doesn't croak");
 if ($dispell) {
  $res = eval { dispell $a, $wiz };
  is($@, '', "dispell in thread $tid doesn't croak");
  undef $b;
  eval { $b = $a };
  is($@, '', "get in thread $tid after dispell doesn't croak");
  is($b, 9,  "get in thread $tid after dispell returns the right thing");
 }
 return; # Ugly if not here
}

my $wiz_name = spawn_wiz VMG_OP_INFO_NAME;
my $wiz_obj  = spawn_wiz VMG_OP_INFO_OBJECT;

for my $dispell (1, 0) {
 for my $wiz ($wiz_name, $wiz_obj) {
  {
   lock $c;
   $c = 0;
  }
  {
   lock $destroyed;
   $destroyed = 0;
  }

  my @t = map { threads->create(\&try, $dispell, $wiz) } 1 .. 2;
  $_->join for @t;

  {
   lock $c;
   is $c, 2, "get triggered twice";
  }
  {
   lock $destroyed;
   is $destroyed, (1 - $dispell) * 2, 'destructors';
  }
 }
}
