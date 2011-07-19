#!perl -T

use strict;
use warnings;

use Test::More;

use Variable::Magic qw/wizard cast dispell VMG_UVAR VMG_OP_INFO_NAME VMG_OP_INFO_OBJECT/;

my $run;
if (VMG_UVAR) {
 plan tests => 43;
 $run = 1;
} else {
 plan skip_all => 'uvar magic is required to test symbol table hooks';
}

our %mg;

my $code = 'wizard '
        . join (', ', map { <<CB;
$_ => sub {
 my \$d = \$_[1];
 return 0 if \$d->{guard};
 local \$d->{guard} = 1;
 push \@{\$mg{$_}}, \$_[2];
 ()
}
CB
} qw/fetch store exists delete/);

$code .= ', data => sub { +{ guard => 0 } }';

my $wiz = eval $code;
diag $@ if $@;

cast %Hlagh::, $wiz;

{
 local %mg;

 eval q{
  die "ok\n";
  package Hlagh;
  our $thing;
  {
   package NotHlagh;
   our $what = @Hlagh::stuff;
  }
 };

 is $@, "ok\n", 'stash: variables compiled fine';
 is_deeply \%mg, {
  fetch => [ qw/thing stuff/ ],
  store => [ qw/thing stuff/ ],
 }, 'stash: variables';
}

{
 local %mg;

 eval q[
  die "ok\n";
  package Hlagh;
  sub eat;
  sub shoot;
  sub leave { "bye" };
  sub shoot { "bang" };
 ];

 is $@, "ok\n", 'stash: function definitions compiled fine';
 is_deeply \%mg, {
  store => [ qw/eat shoot leave shoot/ ],
 }, 'stash: function definitions';
}

{
 local %mg;

 eval q{
  die "ok\n";
  package Hlagh;
  eat();
  shoot();
  leave();
  roam();
  yawn();
  roam();
 };

 my @calls = qw/eat shoot leave roam yawn roam/;

 is $@, "ok\n", 'stash: function calls compiled fine';
 is_deeply \%mg, {
  fetch => \@calls,
  store => ($] < 5.011002 ? \@calls : [ map { ($_) x 2 } @calls ]),
 }, 'stash: function calls';
}

{
 local %mg;

 eval q{ Hlagh->shoot() };

 is $@, '', 'stash: valid method call ran fine';
 is_deeply \%mg, {
  fetch => [ qw/shoot/ ],
 }, 'stash: valid method call';
}

{
 local %mg;

 eval q{ Hlagh->shoot() };

 is $@, '', 'stash: second valid method call ran fine';
 is_deeply \%mg, {
  fetch => [ qw/shoot/ ],
 }, 'stash: second valid method call';
}

{
 local %mg;

 eval q{ my $meth = 'shoot'; Hlagh->$meth() };

 is $@, '', 'stash: valid dynamic method call ran fine';
 is_deeply \%mg, {
  store => [ qw/shoot/ ],
 }, 'stash: valid dynamic method call';
}

{
 local %mg;

 eval q[
  package Hlagher;
  our @ISA;
  BEGIN { @ISA = 'Hlagh' }
  Hlagher->leave()
 ];

 is $@, '', 'inherited valid method call ran fine';
 is_deeply \%mg, {
  fetch => [ qw/ISA leave/ ],
 }, 'stash: inherited valid method call';
}

{
 local %mg;

 eval q{ Hlagher->leave() };

 is $@, '', 'second inherited valid method call ran fine';
 is_deeply \%mg, { }, 'stash: second inherited valid method call doesn\'t call magic';
}

{
 local %mg;

 eval q{ Hlagher->shoot() };

 is $@, '', 'inherited previously called valid method call ran fine';
 is_deeply \%mg, {
  fetch => [ qw/shoot/ ],
 }, 'stash: inherited previously called valid method call';
}

{
 local %mg;

 eval q{ Hlagher->shoot() };

 is $@, '', 'second inherited previously called valid method call ran fine';
 is_deeply \%mg, { }, 'stash: second inherited previously called valid method call doesn\'t call magic';
}

{
 local %mg;

 eval q{ Hlagh->unknown() };

 like $@, qr/^Can't locate object method "unknown" via package "Hlagh"/, 'stash: invalid method call croaked';
 is_deeply \%mg, {
  fetch => [ qw/unknown/ ],
  store => [ qw/unknown AUTOLOAD/ ],
 }, 'stash: invalid method call';
}

{
 local %mg;

 eval q{ my $meth = 'unknown_too'; Hlagh->$meth() };

 like $@, qr/^Can't locate object method "unknown_too" via package "Hlagh"/, 'stash: invalid dynamic method call croaked';
 is_deeply \%mg, {
  store => [ qw/unknown_too AUTOLOAD/ ],
 }, 'stash: invalid dynamic method call';
}

{
 local %mg;

 eval q{ Hlagher->also_unknown() };

 like $@, qr/^Can't locate object method "also_unknown" via package "Hlagher"/, 'stash: invalid inherited method call croaked';
 is_deeply \%mg, {
  fetch => [ qw/also_unknown AUTOLOAD/ ],
 }, 'stash: invalid method call';
}

{
 local %mg;

 eval q{
  package Hlagh;
  undef &nevermentioned;
  undef &eat;
  undef &shoot;
 };

 is $@, '', 'stash: delete executed fine';
 is_deeply \%mg, {
  store => [
   qw/nevermentioned nevermentioned eat eat shoot shoot nevermentioned/
  ],
 }, 'stash: delete';
}

END {
 is_deeply \%mg, { }, 'stash: magic that remains at END time' if $run;
}

dispell %Hlagh::, $wiz;

{
 package AutoHlagh;

 use vars qw/$AUTOLOAD/;

 sub AUTOLOAD { return $AUTOLOAD }
}

cast %AutoHlagh::, $wiz;

{
 local %mg;

 my $res = eval q{ AutoHlagh->autoloaded() };

 is $@,   '',          'stash: autoloaded method call ran fine';
 is $res, 'AutoHlagh::autoloaded',
                       'stash: autoloaded method call returned the right thing';
 is_deeply \%mg, {
  fetch => [ qw/autoloaded/ ],
  store => [ qw/autoloaded AUTOLOAD AUTOLOAD/ ],
 }, 'stash: autoloaded method call';
}

{
 package AutoHlagher;

 our @ISA;
 BEGIN { @ISA = ('AutoHlagh') }
}

{
 local %mg;

 my $res = eval q{ AutoHlagher->also_autoloaded() };

 is $@,   '',     'stash: inherited autoloaded method call ran fine';
 is $res, 'AutoHlagher::also_autoloaded',
                  'stash: inherited autoloaded method returned the right thing';
 is_deeply \%mg, {
  fetch => [ qw/also_autoloaded AUTOLOAD/ ],
  store => [ qw/AUTOLOAD/ ],
 }, 'stash: inherited autoloaded method call';
}

dispell %AutoHlagh::, $wiz;

my $uo = 0;
$code = 'wizard '
        . join (', ', map { <<CB;
$_ => sub {
 my \$d = \$_[1];
 return 0 if \$d->{guard};
 local \$d->{guard} = 1;
 ++\$uo;
 ()
}
CB
} qw/fetch store exists delete/);

my $uo_exp = $] < 5.011002 ? 2 : 3;

$code .= ', data => sub { +{ guard => 0 } }';

$wiz = eval $code . ', op_info => ' . VMG_OP_INFO_NAME;
diag $@ if $@;

cast %Hlagh::, $wiz;

is $uo, 0, 'stash: no undef op before function call with op name';
eval q{
 die "ok\n";
 package Hlagh;
 meh();
};
is $@,  "ok\n",  'stash: function call with op name compiled fine';
is $uo, $uo_exp, 'stash: undef op after function call with op name';

dispell %Hlagh::, $wiz;
is $uo, $uo_exp, 'stash: undef op after dispell for function call with op name';

$uo = 0;

$wiz = eval $code . ', op_info => ' . VMG_OP_INFO_OBJECT;
diag $@ if $@;

cast %Hlagh::, $wiz;

is $uo, 0, 'stash: no undef op before function call with op object';
eval q{
 die "ok\n";
 package Hlagh;
 wat();
};
is $@,        "ok\n", 'stash: function call with op object compiled fine';
is $uo, $uo_exp,
               'stash: undef op after dispell for function call with op object';

dispell %Hlagh::, $wiz;
is $uo, $uo_exp,
               'stash: undef op after dispell for function call with op object';
