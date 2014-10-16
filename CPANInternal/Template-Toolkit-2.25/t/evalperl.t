#============================================================= -*-perl-*-
#
# t/evalperl.t
#
# Test the evaluation of PERL and RAWPERL blocks.
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
use lib qw( ./lib ../lib );
use Template::Test;
$^W = 1;

#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;
#$Template::Context::DEBUG = 0;

my $tt_no_perl = Template->new({ 
    INTERPOLATE  => 1, 
    POST_CHOMP   => 1,
    EVAL_PERL    => 0,
    INCLUDE_PATH => -d 't' ? 't/test/lib' : 'test/lib',
});

my $tt_do_perl = Template->new({ 
    INTERPOLATE => 1, 
    POST_CHOMP  => 1,
    EVAL_PERL   => 1,
    INCLUDE_PATH => -d 't' ? 't/test/lib' : 'test/lib',
});

my $ttprocs = [
    no_perl => $tt_no_perl, 
    do_perl => $tt_do_perl,
];

test_expect(\*DATA, $ttprocs, &callsign);

__DATA__

-- test --
[% META 
   author  = 'Andy Wardley'
   title   = 'Test Template $foo #6'
   version = 1.23
%]
[% TRY %]
[% PERL %]
    my $output = "author: [% template.author %]\n";
    $stash->set('a', 'The cat sat on the mat');
    $output .= "more perl generated output\n";
    print $output;
[% END %]
[% CATCH %]
Not allowed: [% error +%]
[% END %]
a: [% a +%]
a: $a
[% TRY %]
[% RAWPERL %]
$output .= "The cat sat on the mouse mat\n";
$stash->set('b', 'The cat sat where?');
[% END %]
[% CATCH %]
Still not allowed: [% error +%]
[% END %]
b: [% b +%]
b: $b
-- expect --
Not allowed: perl error - EVAL_PERL not set
a: alpha
a: alpha
Still not allowed: perl error - EVAL_PERL not set
b: bravo
b: bravo

-- test --
[% TRY %]
nothing
[% PERL %]
We don't care about correct syntax within PERL blocks if EVAL_PERL isn't set.
They're simply ignored.
[% END %]
[% CATCH %]
ERROR: [% error.type %]: [% error.info %]
[% END %]
-- expect --
nothing
ERROR: perl: EVAL_PERL not set

-- test --
some stuff
[% TRY %]
[% INCLUDE badrawperl %]
[% CATCH %]
ERROR: [[% error.type %]] [% error.info %]
[% END %]
-- expect --
some stuff
This is some text
ERROR: [perl] EVAL_PERL not set

-- test --
-- use do_perl --
some stuff
[% TRY %]
[% INCLUDE badrawperl %]
[% CATCH +%]
ERROR: [[% error.type %]]
[% END %]
-- expect --
some stuff
This is some text
more stuff goes here
ERROR: [undef]

-- test --
-- use do_perl --
[% META author = 'Andy Wardley' %]
[% PERL %]
    my $output = "author: [% template.author %]\n";
    $stash->set('a', 'The cat sat on the mat');
    $output .= "more perl generated output\n";
    print $output;
[% END %]
-- expect --
author: Andy Wardley
more perl generated output

-- test --
-- use do_perl --
[% META 
   author  = 'Andy Wardley'
   title   = 'Test Template $foo #6'
   version = 3.14
%]
[% PERL %]
    my $output = "author: [% template.author %]\n";
    $stash->set('a', 'The cat sat on the mat');
    $output .= "more perl generated output\n";
    print $output;
[% END %]
a: [% a +%]
a: $a
[% RAWPERL %]
$output .= "The cat sat on the mouse mat\n";
$stash->set('b', 'The cat sat where?');
[% END %]
b: [% b +%]
b: $b
-- expect --
author: Andy Wardley
more perl generated output
a: The cat sat on the mat
a: The cat sat on the mat
The cat sat on the mouse mat
b: The cat sat where?
b: The cat sat where?

-- test --
[% BLOCK foo %]This is block foo[% END %]
[% PERL %]
print $context->include('foo');
print PERLOUT "\nbar\n";
[% END %]
The end
-- expect --
This is block foo
bar
The end

-- test --
[% TRY %]
   [%- PERL %] die "nothing to live for\n" [% END %]
[% CATCH %]
   error: [% error %]
[% END %]
-- expect --
   error: undef error - nothing to live for



