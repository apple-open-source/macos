#============================================================= -*-perl-*-
#
# t/chomp.t
#
# Test the PRE_CHOMP and POST_CHOMP options.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1996-2009 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib );
use Template::Test;
use Template::Constants qw( :chomp );

# uncomment these lines for debugging the generated Perl code
#$Template::Directive::PRETTY = 1;
#$Template::Parser::DEBUG = 1;

match( CHOMP_NONE, 0 );
match( CHOMP_ONE, 1 );
match( CHOMP_ALL, 1 );
match( CHOMP_COLLAPSE, 2 );
match( CHOMP_GREEDY, 3 );

my $foo     = "\n[% foo %]\n";
my $bar     = "\n[%- bar -%]\n";
my $baz     = "\n[%+ baz +%]\n";
my $ding    = "!\n\n[%~ ding ~%]\n\n!";
my $dong    = "!\n\n[%= dong =%]\n\n!";
my $dang    = "Hello[%# blah blah blah -%]\n!";
my $winsux1 = "[% ding -%]\015\012[% dong %]";
my $winsux2 = "[% ding -%]\015\012\015\012[% dong %]";
my $winsux3 = "[% ding %]\015\012[%- dong %]";
my $winsux4 = "[% ding %]\015\012\015\012[%- dong %]";

my $blocks = {
    foo     => $foo,
    bar     => $bar,
    baz     => $baz,
    ding    => $ding,
    dong    => $dong,
    dang    => $dang,
    winsux1 => $winsux1,
    winsux2 => $winsux2,
    winsux3 => $winsux3,
    winsux4 => $winsux4,
};

# script may be being run in distribution root or 't' directory
my $dir   = -d 't' ? 't/test/lib' : 'test/lib';


#------------------------------------------------------------------------
# tests without any CHOMP options set
#------------------------------------------------------------------------

my $tt2 = Template->new({
    BLOCKS       => $blocks,
    INCLUDE_PATH => $dir,
});
my $vars = {
    foo  => 3.14,
    bar  => 2.718,
    baz  => 1.618,
    ding => 'Hello',
    dong => 'World'
};

my $out;
ok( $tt2->process('foo', $vars, \$out), 'foo' );
match( $out, "\n3.14\n", 'foo out' );

$out = '';
ok( $tt2->process('bar', $vars, \$out), 'bar' );
match( $out, "2.718", 'bar out' );

$out = '';
ok( $tt2->process('baz', $vars, \$out), 'baz' );
match( $out, "\n1.618\n", 'baz out' );

$out = '';
ok( $tt2->process('ding', $vars, \$out), 'ding' );
match( $out, "!Hello!", 'ding out' );

$out = '';
ok( $tt2->process('dong', $vars, \$out), 'dong' );
match( $out, "! World !", 'dong out' );

$out = '';
ok( $tt2->process('dang', $vars, \$out), 'dang' );
match( $out, "Hello!", 'dang out' );

$out = '';
ok( $tt2->process('winsux1', $vars, \$out), 'winsux1' );
match( od($out), "HelloWorld", 'winsux1 out' );

$out = '';
ok( $tt2->process('winsux2', $vars, \$out), 'winsux2' );
match( od($out), 'Hello\015\012World', 'winsux2 out' );

$out = '';
ok( $tt2->process('winsux3', $vars, \$out), 'winsux3' );
match( od($out), "HelloWorld", 'winsux3 out' );

$out = '';
ok( $tt2->process('winsux4', $vars, \$out), 'winsux4' );
match( od($out), 'Hello\015\012World', 'winsux4 out' );

$out = '';
ok( $tt2->process('dos_newlines', $vars, \$out), 'dos_newlines' );
match( $out, "HelloWorld", 'dos_newlines out' );

sub od{
    join(
        '', 
        map {
            my $ord = ord($_);
            ($ord > 127 || $ord < 32 )
                ? sprintf '\0%lo', $ord
                : $_
        } 
        split //, shift()
    );
}

#------------------------------------------------------------------------
# tests with the PRE_CHOMP option set
#------------------------------------------------------------------------

$tt2 = Template->new({
    PRE_CHOMP => 1,
    BLOCKS => $blocks,
});

$out = '';
ok( $tt2->process('foo', $vars, \$out), 'pre pi' );
match( $out, "3.14\n", 'pre pi match' );

$out = '';
ok( $tt2->process('bar', $vars, \$out), 'pre e' );
match( $out, "2.718", 'pre e match' );

$out = '';
ok( $tt2->process('baz', $vars, \$out), 'pre phi' );
match( $out, "\n1.618\n", 'pre phi match' );

$out = '';
ok( $tt2->process('ding', $vars, \$out), 'pre hello' );
match( $out, "!Hello!", 'pre hello match' );

$out = '';
ok( $tt2->process('dong', $vars, \$out), 'pre world' );
match( $out, "! World !", 'pre world match' );


#------------------------------------------------------------------------
# tests with the POST_CHOMP option set
#------------------------------------------------------------------------

$tt2 = Template->new({
    POST_CHOMP => 1,
    BLOCKS => $blocks,
});

$out = '';
ok( $tt2->process('foo', $vars, \$out), 'post pi' );
match( $out, "\n3.14", 'post pi match' );

$out = '';
ok( $tt2->process('bar', $vars, \$out), 'post e' );
match( $out, "2.718", 'post e match' );

$out = '';
ok( $tt2->process('baz', $vars, \$out), 'post phi' );
match( $out, "\n1.618\n", 'post phi match' );

$out = '';
ok( $tt2->process('ding', $vars, \$out), 'post hello' );
match( $out, "!Hello!", 'post hello match' );

$out = '';
ok( $tt2->process('dong', $vars, \$out), 'post world' );
match( $out, "! World !", 'post world match' );


my $tt = [
    tt_pre_none  => Template->new(PRE_CHOMP  => CHOMP_NONE),
    tt_pre_one   => Template->new(PRE_CHOMP  => CHOMP_ONE),
    tt_pre_all   => Template->new(PRE_CHOMP  => CHOMP_ALL),
    tt_pre_coll  => Template->new(PRE_CHOMP  => CHOMP_COLLAPSE),
    tt_post_none => Template->new(POST_CHOMP => CHOMP_NONE),
    tt_post_one  => Template->new(POST_CHOMP => CHOMP_ONE),
    tt_post_all  => Template->new(POST_CHOMP => CHOMP_ALL),
    tt_post_coll => Template->new(POST_CHOMP => CHOMP_COLLAPSE),
];

test_expect(\*DATA, $tt);

__DATA__
#------------------------------------------------------------------------
# tt_pre_none
#------------------------------------------------------------------------
-- test --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin
     10
     20
end

#------------------------------------------------------------------------
# tt_pre_one
#------------------------------------------------------------------------
-- test --
-- use tt_pre_one --
-- test --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin1020
end


#------------------------------------------------------------------------
# tt_pre_all
#------------------------------------------------------------------------
-- test --
-- use tt_pre_all --
-- test --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin1020
end

#------------------------------------------------------------------------
# tt_pre_coll
#------------------------------------------------------------------------
-- test --
-- use tt_pre_coll --
-- test --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin 10 20
end


#------------------------------------------------------------------------
# tt_post_none
#------------------------------------------------------------------------
-- test --
-- use tt_post_none --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin
     10
     20
end

#------------------------------------------------------------------------
# tt_post_all
#------------------------------------------------------------------------
-- test --
-- use tt_post_all --
-- test --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin     10     20end

#------------------------------------------------------------------------
# tt_post_one
#------------------------------------------------------------------------
-- test --
-- use tt_post_one --
-- test --
begin[% a = 10; b = 20 %]
     [% a %]
     [% b %]
end
-- expect --
begin     10     20end

#------------------------------------------------------------------------
# tt_post_coll
#------------------------------------------------------------------------
-- test --
-- use tt_post_coll --
-- test --
begin[% a = 10; b = 20 %]     
[% a %]     
[% b %]     
end
-- expect --
begin 10 20 end

