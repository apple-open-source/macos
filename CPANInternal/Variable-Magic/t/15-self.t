#!perl -T

use strict;
use warnings;

use Test::More tests => 17;

use Variable::Magic qw/wizard cast dispell getdata/;

my $c = 0;

{
 my $wiz = eval {
  wizard data => sub { $_[0] },
         get  => sub { ++$c },
         free => sub { --$c }
 };
 is($@, '',             'wizard creation error doesn\'t croak');
 ok(defined $wiz,       'wizard is defined');
 is(ref $wiz, 'SCALAR', 'wizard is a scalar ref');

 my $res = eval { cast $wiz, $wiz };
 is($@, '', 'cast on self doesn\'t croak');
 ok($res,   'cast on self is valid');

 my $w = $wiz;
 is($c, 1, 'magic works correctly on self');

 $res = eval { dispell $wiz, $wiz };
 is($@, '', 'dispell on self doesn\'t croak');
 ok($res,   'dispell on self is valid');

 $w = $wiz;
 is($c, 1, 'magic is no longer invoked on self when dispelled');

 $res = eval { cast $wiz, $wiz, $wiz };
 is($@, '', 're-cast on self doesn\'t croak');
 ok($res,   're-cast on self is valid');

 $w = getdata $wiz, $wiz;
 is($c, 1, 'getdata on magical self doesn\'t trigger callbacks');

 $res = eval { dispell $wiz, $wiz };
 is($@, '', 're-dispell on self doesn\'t croak');
 ok($res,   're-dispell on self is valid');

 $res = eval { cast $wiz, $wiz };
 is($@, '', 're-re-cast on self doesn\'t croak');
 ok($res,   're-re-cast on self is valid');
}

eval q[
 use lib 't/lib';
 BEGIN { require Variable::Magic::TestDestroyRequired; }
];
is $@, '', 'wizard destruction at the end of BEGIN-time require doesn\'t panic';

if ((defined $ENV{PERL_DESTRUCT_LEVEL} and $ENV{PERL_DESTRUCT_LEVEL} >= 3)
    or eval "use Perl::Destruct::Level level => 3; 1") {
 diag 'Test global destruction';
}
