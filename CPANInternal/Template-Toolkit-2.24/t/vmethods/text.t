#============================================================= -*-perl-*-
#
# t/vmethods/text.t
#
# Testing scalar (text) virtual variable methods.
#
# Written by Andy Wardley <abw@cpan.org>
#
# Copyright (C) 1996-2006 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib ../../lib );
use Template::Test;

# make sure we're using the Perl stash
$Template::Config::STASH = 'Template::Stash';

# define a new text method
$Template::Stash::SCALAR_OPS->{ commify } = sub {
    local $_  = shift;
    my $c = shift || ",";
    my $n = int(shift || 3);
    return $_ if $n<1;
    1 while s/^([-+]?\d+)(\d{$n})/$1$c$2/;
    return $_;
};


my $tt = Template->new();
my $tc = $tt->context();

# define vmethods using define_vmethod() interface.
$tc->define_vmethod( item =>
                     commas =>
                     $Template::Stash::SCALAR_OPS->{ commify } );

my $params = {
    undef    => undef,
    zero     => 0,
    one      => 1,
    animal   => 'cat',
    string   => 'The cat sat on the mat',
    spaced   => '  The dog sat on the log',
    word     => 'bird',                       # The bird is the word
    WORD     => 'BIRD',
    the_bird => "\n  The  bird\n  is  the  word  \n  ",
    quotable => "Tim O'Reilly said \"Oh really?\"",
    markup   => 'a < b > & c "d" \'e\'',
};

test_expect(\*DATA, undef, $params);

__DATA__

-- test --
-- name text.defined implicit undef --
[% notdef.defined ? 'def' : 'undef' %]
-- expect --
undef

-- test --
-- name text.defined explicit undef --
[% undef.defined ? 'def' : 'undef' %]
-- expect --
undef

-- test --
-- name text.defined zero --
[% zero.defined ? 'def' : 'undef' %]
-- expect --
def

-- test --
-- name text.defined one --
[% one.defined ? 'def' : 'undef' %]
-- expect --
def

-- test --
-- name string.length --
[% string.length %]
-- expect --
22

-- test --
-- name text.upper --
[% string.upper %]
-- expect --
THE CAT SAT ON THE MAT

-- test --
-- name text.lower --
[% string.lower %]
-- expect --
the cat sat on the mat

-- test --
-- name text.ucfirst --
[% word.ucfirst %]
[% WORD.ucfirst %]
[% WORD.lower.ucfirst %]
-- expect --
Bird
BIRD
Bird

-- test --
-- name text.lcfirst --
[% word.lcfirst %]
[% WORD.lcfirst %]
-- expect --
bird
bIRD

-- test --
-- name text.trim --
>[% the_bird.trim %]<
-- expect --
>The  bird
  is  the  word<

-- test --
-- name text.collapse --
>[% the_bird.collapse %]<
-- expect --
>The bird is the word<

-- test --
-- name text.sort.join --
[% string.sort.join %]
-- expect --
The cat sat on the mat

-- test --
-- name text.split.join a --
[% string.split.join('_') %]
-- expect --
The_cat_sat_on_the_mat

-- test --
-- name text.split.join b --
[% string.split(' ', 3).join('_') %]
-- expect --
The_cat_sat on the mat

-- test --
-- name text.split.join c --
[% spaced.split.join('_') %]
-- expect --
The_dog_sat_on_the_log

-- test --
-- name text.split.join d --
[% spaced.split(' ').join('_') %]
-- expect --
__The_dog_sat_on_the_log

-- test --
-- name text.list --
[% string.list.join %]
-- expect --
The cat sat on the mat

-- test --
-- name text.hash --
[% string.hash.value %]
-- expect --
The cat sat on the mat

-- test --
-- name text.size --
[% string.size %]
-- expect --
1

-- test --
-- name text.squote --
[% quotable %]
[% quotable.squote %]
-- expect --
Tim O'Reilly said "Oh really?"
Tim O\'Reilly said "Oh really?"

-- test --
-- name text.dquote --
[% quotable %]
[% quotable.dquote %]
-- expect --
Tim O'Reilly said "Oh really?"
Tim O'Reilly said \"Oh really?\"

-- test --
-- name text.html --
[% markup.html %]
-- expect --
a &lt; b &gt; &amp; c &quot;d&quot; 'e'

-- test --
-- name text.xml --
[% markup.xml %]
-- expect --
a &lt; b &gt; &amp; c &quot;d&quot; &apos;e&apos;


-- test --
-- name text.repeat --
[% animal.repeat(3) %]
-- expect --
catcatcat

-- test --
-- name text.search --
[% animal.search('at$') ? "found 'at\$'" : "didn't find 'at\$'" %]
-- expect --
found 'at$'

-- test --
-- name text.search --
[% animal.search('^at') ? "found '^at'" : "didn't find '^at'" %]
-- expect --
didn't find '^at'

-- test --
-- name text.match an --
[% text = 'bandanna';
   text.match('an') ? 'match' : 'no match'
%]
-- expect --
match

-- test --
-- name text.match on --
[% text = 'bandanna';
   text.match('on') ? 'match' : 'no match'
%]
-- expect --
no match

-- test --
-- name text.match global an --
[% text = 'bandanna';
   text.match('an', 1).size %] matches
-- expect --
2 matches

-- test --
-- name text.match global an --
[% text = 'bandanna' -%]
matches are [% text.match('an+', 1).join(', ') %]
-- expect --
matches are an, ann

-- test --
-- name text.match global on --
[% text = 'bandanna';
   text.match('on+', 1) ? 'match' : 'no match'
%]
-- expect --
no match

-- test --
-- name: text substr method --
[% text = 'Hello World' -%]
a: [% text.substr(6) %]!
b: [% text.substr(0, 5) %]!
c: [% text.substr(0, 5, 'Goodbye') %]!
d: [% text %]!
-- expect --
a: World!
b: Hello!
c: Goodbye World!
d: Hello World!

-- test --
-- name: another text substr method --
[% text = 'foo bar baz wiz waz woz' -%]
a: [% text.substr(4, 3) %]
b: [% text.substr(12) %]
c: [% text.substr(0, 11, 'FOO') %]
d: [% text %]
-- expect --
a: bar
b: wiz waz woz
c: FOO wiz waz woz
d: foo bar baz wiz waz woz


-- test --
-- name: text.remove --
[% text = 'hello world!';
   text.remove('\s+world')
%]
-- expect --
hello!



-- test --
-- name chunk left --
[% string = 'TheCatSatTheMat' -%]
[% string.chunk(3).join(', ') %]
-- expect --
The, Cat, Sat, The, Mat

-- test --
-- name chunk leftover --
[% string = 'TheCatSatonTheMat' -%]
[% string.chunk(3).join(', ') %]
-- expect --
The, Cat, Sat, onT, heM, at

-- test --
-- name chunk right --
[% string = 'TheCatSatTheMat' -%]
[% string.chunk(-3).join(', ') %]
-- expect --
The, Cat, Sat, The, Mat

-- test --
-- name chunk rightover --
[% string = 'TheCatSatonTheMat' -%]
[% string.chunk(-3).join(', ') %]
-- expect --
Th, eCa, tSa, ton, The, Mat

-- test --
-- name chunk ccard  --
[% ccard_no = "1234567824683579";
   ccard_no.chunk(4).join
%]
-- expect --
1234 5678 2468 3579


-- test --
[% string = 'foo' -%]
[% string.repeat(3) %]
-- expect --
foofoofoo

-- test --
[% string1 = 'foobarfoobarfoo'
   string2 = 'foobazfoobazfoo'
-%]
[% string1.search('bar') ? 'ok' : 'not ok' %]
[% string2.search('bar') ? 'not ok' : 'ok' %]
[% string1.replace('bar', 'baz') %]
[% string2.replace('baz', 'qux') %]
-- expect --
ok
ok
foobazfoobazfoo
fooquxfooquxfoo

-- test --
[% string1 = 'foobarfoobarfoo'
   string2 = 'foobazfoobazfoo'
-%]
[% string1.match('bar') ? 'ok' : 'not ok' %]
[% string2.match('bar') ? 'not ok' : 'ok' %]
-- expect --
ok
ok

-- test --
[% string = 'foo     bar   ^%$ baz' -%]
[% string.replace('\W+', '_') %]
-- expect --
foo_bar_baz

-- test --
[% var = 'value99' ;
   var.replace('value', '')
%]
-- expect --
99

-- test --
[% bob = "0" -%]
bob: [% bob.replace('0','') %].
-- expect --
bob: .

-- test --
[% string = 'The cat sat on the mat';
   match  = string.match('The (\w+) (\w+) on the (\w+)');
-%]
[% match.0 %].[% match.1 %]([% match.2 %])
-- expect --
cat.sat(mat)

-- test --
[% string = 'The cat sat on the mat' -%]
[% IF (match  = string.match('The (\w+) sat on the (\w+)')) -%]
matched animal: [% match.0 %]  place: [% match.1 %]
[% ELSE -%]
no match
[% END -%]
[% IF (match  = string.match('The (\w+) shat on the (\w+)')) -%]
matched animal: [% match.0 %]  place: [% match.1 %]
[% ELSE -%]
no match
[% END -%]
-- expect --
matched animal: cat  place: mat
no match


-- test --
[% big_num = "1234567890"; big_num.commify %]
-- expect --
1,234,567,890

-- test --
[% big_num = "1234567890"; big_num.commify(":", 2) %]
-- expect --
12:34:56:78:90

-- test --
[% big_num = "1234567812345678"; big_num.commify(" ", 4) %]
-- expect --
1234 5678 1234 5678

-- test --
[% big_num = "hello world"; big_num.commify %]
-- expect --
hello world

-- test --
[% big_num = "1234567890"; big_num.commas %]
-- expect --
1,234,567,890

