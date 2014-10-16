#============================================================= -*-perl-*-
#
# t/pod_kwalitee.t
#
# Use Test::Pod (if available) to test the POD documentation.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 2008-2012 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib );
use Test::More;

plan( skip_all => "Author tests not required for installation" )
    unless $ENV{ RELEASE_TESTING   }
        or $ENV{ AUTOMATED_TESTING };

eval "use Test::Pod 1.00";
plan skip_all => "Test::Pod 1.00 required for testing POD" if $@;
all_pod_files_ok();

