#============================================================= -*-perl-*-
#
# t/compile5.t
#
# Test that the compiled template files written by compile4.t can be 
# loaded and used.  Similar to compile2.t but using COMPILE_DIR as well
# as COMPILE_EXT.
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
use warnings;
use lib qw( ./lib ../lib );
use Template::Test;
use Cwd qw( abs_path );
use File::Path;

my @dir   = -d 't' ? qw(t test) : qw(test);
my $dir   = abs_path( File::Spec->catfile(@dir) );
my $tdir  = abs_path( File::Spec->catfile(@dir, 'tmp'));
my $cdir  = File::Spec->catfile($tdir, 'cache');
my $zero  = File::Spec->catfile($dir, qw(src divisionbyzero));
print "zero: $zero\n";

#my $dir   = abs_path( -d 't' ? 't/test' : 'test' );
#my $cdir  = abs_path("$dir/tmp") . "/cache";
#my $zero  = "$cdir/src/divisionbyzero";
my $ttcfg = {
    POST_CHOMP   => 1,
    INCLUDE_PATH => "$dir/src",
    COMPILE_DIR  => "$cdir/",    # note trailing slash - should be handled OK
    COMPILE_EXT  => '.ttc',
    ABSOLUTE     => 1,
    CONSTANTS    => {
      dir  => $dir,
      zero => $zero,
    },
};

#print "

# check compiled template files exist
my $fixdir = $dir;
$fixdir =~ s[:][]g if $^O eq 'MSWin32';
my ($foo, $bar, $blam) = map { "$cdir/$fixdir/src/$_.ttc" } 
                           qw( foo complex blam );
$blam =~ s[/+][/]g;

ok( -f $foo, 'cached foo' );
ok( -f $bar, 'cached bar' );
ok( -f $blam, 'cached blam' );

# we're going to hack on the compiled 'foo' file to change some key text.
# this way we can tell that the template was loaded from the compiled
# version and not the source.

my @foo_times = (stat $foo)[8,9];

open(FOO, $foo) || die "$foo: $!\n";
local $/ = undef;
my $content = <FOO>;
close(FOO);

$content =~ s/the foo file/the newly hacked foo file/;
open(FOO, "> $foo") || die "$foo: $!\n";
print FOO $content;
close(FOO);

# and set back
utime( @foo_times, $foo );


# same again for 'blam'

my @blam_times = (stat $blam)[8,9];

open(BLAM, $blam) || die "$blam: $!\n";
local $/ = undef;
$content = <BLAM>;
close(BLAM);
$content =~ s/blam/wam-bam/g;
open(BLAM, "> $blam") || die "$blam: $!\n";
print BLAM $content;
close(BLAM);

# and set back
utime( @blam_times, $blam );

test_expect(\*DATA, $ttcfg, { root => abs_path($dir) } );

exit;

# cleanup cache directory
rmtree($cdir) if -d $cdir;


__DATA__
-- test --
[% INCLUDE foo a = 'any value' %]
-- expect --
This is the newly hacked foo file, a is any value

-- test --
[% META author => 'billg' version => 6.66  %]
[% INCLUDE complex %]
-- expect --
This is the header, title: Yet Another Template Test
This is a more complex file which includes some BLOCK definitions
This is the footer, author: billg, version: 6.66
- 3 - 2 - 1 

-- test --
[% INCLUDE "$root/src/blam" %]
-- expect --
This is the wam-bam file
-- test --
[%- # second pass, reads the compiled code from cache -%]
[% INCLUDE divisionbyzero -%]
-- expect --
-- process --
undef error - Illegal division by zero at [% constants.zero %] line 1, <DATA> chunk 1.
