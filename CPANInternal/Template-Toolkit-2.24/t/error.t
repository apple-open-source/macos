#============================================================= -*-perl-*-
#
# t/error.t
#
# Test that errors are propagated back to the caller as a 
# Template::Exception object.
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
use Template::Constants qw( :status );
use Template::Test;
$^W = 1;


my $template = Template->new({
    BLOCKS => {
	badinc => "[% INCLUDE nosuchfile %]",
    },
});


ok( ! $template->process('badinc') );
my $error = $template->error();
ok( $error );
ok( ref $error eq 'Template::Exception' );
ok( $error->type eq 'file' );
ok( $error->info eq 'nosuchfile: not found' );




