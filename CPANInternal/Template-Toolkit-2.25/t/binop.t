#============================================================= -*-perl-*-
#
# t/binop.t
#
# Template script testing the conditional binary operators: and/&&, or/||,
# not/!, <, >, <=, >= , == and !=.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2000 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1998-2000 Canon Research Centre Europe Ltd.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ../lib );
use Template::Test;
use Template::Parser;
$^W = 1;

$Template::Test::DEBUG = 0;
$Template::Parser::DEBUG = 0;


my $counter  = 0;
my $params   = {
    'yes'    => 1,
    'no'     => 0,
    'true'   => 'this is true',
    'false'  => '0',
    'happy'  => 'yes',
    'sad'    => '',
    'ten'    => 10,
    'twenty' => 20,
    'alpha'  => sub { return ++$counter },
    'omega'  => sub { $counter += 10; return 0 },
    'count'  => sub { return $counter },
    'reset'  => sub { return $counter == 0 },
};

my $template = Template->new({ INTERPOLATE => 1, POST_CHOMP => 1 });

test_expect(\*DATA, $template, $params);



__DATA__
maybe
[% IF yes %]
yes
[% END %]
-- expect --
maybe
yes

-- test --
[% IF yes %]
yes
[% ELSE %]
no 
[% END %]
-- expect --
yes

-- test --
[% IF yes %]
yes
[% ELSE %]
no 
[% END %]
-- expect --
yes

-- test --
[% IF yes and true %]
yes
[% ELSE %]
no 
[% END %]
-- expect --
yes


-- test --
[% IF yes && true %]
yes
[% ELSE %]
no 
[% END %]
-- expect --
yes

-- test --
[% IF yes && sad || happy %]
yes
[% ELSE %]
no 
[% END %]
-- expect --
yes

-- test --
[% IF yes AND ten && true and twenty && 30 %]
yes
[% ELSE %]
no
[% END %]
-- expect --
yes

-- test --
[% IF ! yes %]
no
[% ELSE %]
yes
[% END %]
-- expect --
yes

-- test --
[% UNLESS yes %]
no
[% ELSE %]
yes
[% END %]
-- expect --
yes

-- test --
[% "yes" UNLESS no %]
-- expect --
yes


-- test --
[% IF ! yes %]
no
[% ELSE %]
yes
[% END %]
-- expect --
yes

-- test --
[% IF yes || no %]
yes
[% ELSE %]
no
[% END %]
-- expect --
yes

-- test --
[% IF yes || no || true || false %]
yes
[% ELSE %]
no
[% END %]
-- expect --
yes

-- test --
[% IF yes or no %]
yes
[% ELSE %]
no
[% END %]
-- expect --
yes

-- test --
[% IF not false and not sad %]
yes
[% ELSE %]
no
[% END %]
-- expect --
yes

-- test --
[% IF ten == 10 %]
yes
[% ELSE %]
no
[% END %]
-- expect --
yes

-- test --
[% IF ten == twenty %]
I canna break the laws of mathematics, Captain.
[% ELSIF ten > twenty %]
Your numerical system is inverted.  Please reboot your Universe.
[% ELSIF twenty < ten %]
Your inverted system is numerical.  Please universe your reboot.
[% ELSE %]
Normality is restored.  Anything you can't cope with is your own problem.
[% END %]
-- expect --
Normality is restored.  Anything you can't cope with is your own problem.

-- test --
[% IF ten >= twenty or false %]
no
[% ELSIF twenty <= ten  %]
nope
[% END %]
nothing
-- expect --
nothing

-- test --
[% IF ten >= twenty or false %]
no
[% ELSIF twenty <= ten  %]
nope
[% END %]
nothing
-- expect --
nothing

-- test --
[% IF ten > twenty %]
no
[% ELSIF ten < twenty  %]
yep
[% END %]
-- expect --
yep

-- test --
[% IF ten != 10 %]
no
[% ELSIF ten == 10  %]
yep
[% END %]
-- expect --
yep



#------------------------------------------------------------------------
# test short-circuit operations
#------------------------------------------------------------------------

-- test --
[% IF alpha AND omega %]
alpha and omega are true
[% ELSE %]
alpha and/or omega are not true
[% END %]
count: [% count %]
-- expect --
alpha and/or omega are not true
count: 11

-- test --
[% IF omega AND alpha %]
omega and alpha are true
[% ELSE %]
omega and/or alpha are not true
[% END %]
count: [% count %]
-- expect --
omega and/or alpha are not true
count: 21

-- test --
[% IF alpha OR omega %]
alpha and/or omega are true
[% ELSE %]
neither alpha nor omega are true
[% END %]
count: [% count %]
-- expect --
alpha and/or omega are true
count: 22

-- test --
[% IF omega OR alpha %]
alpha and/or omega are true
[% ELSE %]
neither alpha nor omega are true
[% END %]
count: [% count %]
-- expect --
alpha and/or omega are true
count: 33

-- test --
[% small = 5
   mid   = 7
   big   = 10
   both  = small + big
   less  = big - mid
   half  = big / small
   left  = big % mid
   mult  = big * small
%]
both: [% both +%]
less: [% less +%]
half: [% half +%]
left: [% left +%]
mult: [% mult +%]
maxi: [% mult + 2 * 2 +%]
mega: [% mult * 2 + 2 * 3 %]

-- expect --
both: 15
less: 3
half: 2
left: 3
mult: 50
maxi: 54
mega: 106

-- test --
[% 10 mod 4 +%] [% 10 MOD 4 +%]
[% 10 div 3 %] [% 10 DIV 3 %]
-- expect --
2 2
3 3


-- stop -- 
# this is for testing the lt operator which isn't enabled by default.
-- test --
[% IF 'one' lt 'two' -%]
one is less than two
[% ELSE -%]
ERROR!
[% END -%]
-- expect --
one is less than two
