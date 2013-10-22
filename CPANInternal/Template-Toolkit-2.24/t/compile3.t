#============================================================= -*-perl-*-
#
# t/compile3.t
#
# Third test in the compile<n>.t trilogy.  Checks that modifications
# to a source template result in a re-compilation of the template.
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
use File::Copy;
use File::Spec;

#ntests(13);

# declare extra test to follow test_expect();
$Template::Test::EXTRA = 1;
#$Template::Parser::DEBUG = 1;

# script may be being run in distribution root or 't' directory
my @dir   = -d 't' ? qw(t test src) : qw(test src);
my $dir   = File::Spec->catfile(@dir);
my $ttcfg = {
    POST_CHOMP   => 1,
    INCLUDE_PATH => $dir,
    COMPILE_EXT  => '.ttc',
};

# test process fails when EVAL_PERL not set
my $tt = Template->new($ttcfg);
my $out;
ok( ! $tt->process("evalperl", { }, \$out) );
match( $tt->error->type, 'perl' );
match( $tt->error->info, 'EVAL_PERL not set' );

# ensure we can run compiled templates without loading parser
# (fix for "Can't locate object method "TIEHANDLE" via package 
# Template::String..." bug)
$ttcfg->{ EVAL_PERL } = 1;
$tt = Template->new($ttcfg);
ok( $tt->process("evalperl", { }, \$out) )
    || match( $tt->error(), "" );

my $file = "$dir/complex";

# check compiled template file exists and grab modification time
ok( -f "$file.ttc" );
my $mod = (stat(_))[9];

# save copy of the source file because we're going to try to break it
copy($file, "$file.org") || die "failed to copy $file to $file.org\n";

# sleep for a couple of seconds to ensure clock has ticked
sleep(2);

# append a harmless newline to the end of the source file to change
# its modification time
append_file("\n");

# define 'bust_it' to append a lone "[% TRY %]" onto the end of the 
# source file to cause re-compilation to fail
my $replace = {
    bust_it   => sub { append_file('[% TRY %]') },
    near_line => sub {
        my ($warning, $n) = @_;
        if ($warning =~ s/line (\d+)/line ${n}ish/) {
            my $diff = abs($1 - $n);
            if ($diff < 4) {
                # That's close enough for rock'n'roll.  The line
                # number reported appears to vary from one version of
                # Perl to another
                return $warning;
            }
            else {
                return $warning . " (where 'ish' means $diff!)";
            }
        }
        else {
            return "no idea what line number that is\n";
        }
    }
};

test_expect(\*DATA, $ttcfg, $replace );

ok( (stat($file))[9] > $mod );

# restore original source file
copy("$file.org", $file) || die "failed to copy $file.org to $file\n";

#------------------------------------------------------------------------

sub append_file {
    local *FP;
    sleep(2);     # ensure file time stamps are different
    open(FP, ">>$file") || die "$file: $!\n";
    print FP @_;
    close(FP);
}

#------------------------------------------------------------------------

__DATA__
-- test --
[% META author => 'albert' version => 'emc2'  %]
[% INCLUDE complex %]
-- expect --
This is the header, title: Yet Another Template Test
This is a more complex file which includes some BLOCK definitions
This is the footer, author: albert, version: emc2
- 3 - 2 - 1 

-- test --
[%# we want to break 'compile' to check that errors get reported -%]
[% CALL bust_it -%]
[% TRY; INCLUDE complex; CATCH; near_line("$error", 18); END %]
-- expect --
file error - parse error - complex line 18ish: unexpected end of input
