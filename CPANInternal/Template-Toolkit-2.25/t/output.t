#============================================================= -*-perl-*-
#
# t/output.t
#
# Test the OUTPUT and OUTPUT_PATH options of the Template.pm module.
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
use lib  qw( ./lib ../lib );
use Template::Test;

ntests(14);

my $dir   = -d 't' ? 't/test' : 'test';
my $f1    = 'foo.bar';
my $f2    = 'foo.baz';
my $file1 = "$dir/tmp/$f1";
my $file2 = "$dir/tmp/$f2";

#------------------------------------------------------------------------

my $tt = Template->new({
    INCLUDE_PATH => "$dir/src:$dir/lib",
    OUTPUT_PATH  => "$dir/tmp",
}) || die Template->error();

unlink($file1) if -f $file1;

ok( $tt->process('foo', &callsign, $f1) );
ok( -f $file1 );

open(FP, $file1) || die "$file1: $!\n";
local $/ = undef;
my $out = <FP>;
close(FP);

ok( 1 );

match( $out, "This is the foo file, a is alpha" );

unlink($file1);

#------------------------------------------------------------------------

$tt = Template->new({
    INCLUDE_PATH => "$dir/src:$dir/lib",
    OUTPUT_PATH  => "$dir/tmp",
    OUTPUT       => $f2,
}) || die Template->error();

unlink($file2) if -f $file2;

ok( $tt->process('foo', &callsign) );
ok( -f $file2 );

open(FP, $file2) || die "$file2: $!\n";
local $/ = undef;
$out = <FP>;
close(FP);

ok( 1 );

match( $out, "This is the foo file, a is alpha" );

unlink($file2);


#------------------------------------------------------------------------
# test passing options like 'binmode' to Template process() method to 
# ensure they get passed onto _output() subroutine.
#------------------------------------------------------------------------
package My::Template;
use Template;
use base qw( Template );
use vars qw( $MESSAGE );

sub DEBUG {
    my $self = shift;
    $MESSAGE = join('', @_);
}

package main;

$tt = My::Template->new({
    INCLUDE_PATH => "$dir/src:$dir/lib",
    OUTPUT_PATH  => "$dir/tmp",
    OUTPUT       => $f2,
}) || die Template->error();

$Template::DEBUG = 1;

ok( $tt->process('foo', &callsign, undef, { binmode => 1 }), 'processed' );
ok( -f $file2, 'output file exists' );
is( $My::Template::MESSAGE, "set binmode\n", 'set binmode via hashref' );

$My::Template::MESSAGE = 'reset';

ok( $tt->process('foo', &callsign, $f2, binmode => 1), 'processed again' );
ok( -f $file2, 'output file exists' );
is( $My::Template::MESSAGE, "set binmode\n", 'set binmode via arglist' );

unlink($file2);






