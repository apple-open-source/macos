#============================================================= -*-perl-*-
#
# t/table.t
#
# Tests the 'Table' plugin.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 2000-2006 Andy Wardley. All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use warnings;
use lib qw( ../lib );
use Template::Test;

$Template::Test::DEBUG = 0;

my $params = { 
    alphabet => [ 'a'..'z' ],
    empty    => [ ],
};

test_expect(\*DATA, { POST_CHOMP => 1 }, $params);
 

#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__

-- test --
[% USE table(alphabet, rows=5) %]
[% FOREACH letter = table.col(0) %]
[% letter %]..
[%- END +%]
[% FOREACH letter = table.col(1) %]
[% letter %]..
[%- END %]

-- expect --
a..b..c..d..e..
f..g..h..i..j..

-- test --
[% USE table(alphabet, rows=5) %]
[% FOREACH letter = table.row(0) %]
[% letter %]..
[%- END +%]
[% FOREACH letter = table.row(1) %]
[% letter %]..
[%- END %]

-- expect --
a..f..k..p..u..z..
b..g..l..q..v....

-- test --
[% USE table(alphabet, rows=3) %]
[% FOREACH col = table.cols %]
[% col.0 %] [% col.1 %] [% col.2 +%]
[% END %]

-- expect --
a b c
d e f
g h i
j k l
m n o
p q r
s t u
v w x
y z 

-- test --
[% USE alpha = table(alphabet, cols=3, pad=0) %]
[% FOREACH group = alpha.col %]
[ [% group.first %] - [% group.last %] ([% group.size %] letters) ]
[% END %]

-- expect --
[ a - i (9 letters) ]
[ j - r (9 letters) ]
[ s - z (8 letters) ]

-- test --
[% USE alpha = table(alphabet, rows=5, pad=0, overlap=1) %]
[% FOREACH group = alpha.col %]
[ [% group.first %] - [% group.last %] ([% group.size %] letters) ]
[% END %]

-- expect --
[ a - e (5 letters) ]
[ e - i (5 letters) ]
[ i - m (5 letters) ]
[ m - q (5 letters) ]
[ q - u (5 letters) ]
[ u - y (5 letters) ]
[ y - z (2 letters) ]


-- test --
[% USE table(alphabet, rows=5, pad=0) %]
[% FOREACH col = table.cols %]
[% col.join('-') +%]
[% END %]

-- expect --
a-b-c-d-e
f-g-h-i-j
k-l-m-n-o
p-q-r-s-t
u-v-w-x-y
z

-- test --
[% USE table(alphabet, rows=8, overlap=1 pad=0) %]
[% FOREACH col = table.cols %]
[% FOREACH item = col %][% item %] [% END +%]
[% END %]

-- expect --
a b c d e f g h 
h i j k l m n o 
o p q r s t u v 
v w x y z 

-- test --
[% USE table([1,3,5], cols=5) %]
[% FOREACH t = table.rows %]
[% t.join(', ') %]
[% END %]
-- expect --
1, 3, 5

-- test --
>
[% USE table(empty, rows=3) -%]
[% FOREACH col = table.cols -%]
col
[% FOREACH item = col -%]
item: [% item -%]
[% END -%]
[% END -%]
<
-- expect --
>
<
