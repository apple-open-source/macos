#============================================================= -*-perl-*-
#
# t/template.t
#
# Test the Template.pm module.  Does nothing of any great importance
# at the moment, but all of its options are tested in the various other
# test scripts.
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
use Template;
use Template::Test;

my $out;
my $dir = -d 't' ? 't/test' : 'test';
my $tt  = Template->new({
    INCLUDE_PATH => "$dir/src:$dir/lib",	
    OUTPUT       => \$out,
});

ok( $tt );
ok( $tt->process('header') );
ok( $out );

$out = '';
ok( ! $tt->process('this_file_does_not_exist') );
my $error = $tt->error();
ok( $error->type() eq 'file' );
ok( $error->info() eq 'this_file_does_not_exist: not found' );

my @output;
$tt->process('header', undef, \@output);
ok(length($output[-1]));

sub myout {
  my $output = shift;
  ok($output)
}

ok($tt->process('header', undef, \&myout));

$out = Myout->new();

ok($tt->process('header', undef, $out));

package Myout;
use Template::Test;

sub new {
  my $proto = shift;
  my $class = ref($proto) || $proto;
  my $self = {};
  bless($self, $class);
  return $self;
}
sub print {
  my $output = shift;
  ok($output);
}
