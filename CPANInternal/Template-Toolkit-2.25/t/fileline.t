#============================================================= -*-perl-*-
#
# t/fileline.t
#
# Test the reporting of template file and line number in errors.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1996-2007 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
#========================================================================

#BEGIN {
#    if ( $^O eq 'MSWin32' ) {
#        print "1..0 # Skip Temporarily skipping on Win32\n";
#        exit(0);
#    }
#}

use strict;
use warnings;
use lib qw( ./lib ../lib ./blib/lib ../blib/lib ./blib/arch ../blib/arch );
use Template::Test;
use Template::Parser;
use Template::Directive;

#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;

my $dir = -d 't' ? 't/test/lib' : 'test/lib';

my $warning;
local $SIG{__WARN__} = sub {
    $warning = shift;
};

my $vars = {
    warning => sub { return $warning },
    file => sub {
        $warning =~ /at (.*?) line/;
        my $file = $1;
        # The error returned includes a reference to the eval string
        # e.g. ' ...at (eval 1) line 1'.  On some platforms (notably
        # FreeBSD and variants like OSX), the (eval $n) part contains
        # a different number, presumably because it has previously 
        # performed additional string evals.  It's not important to 
        # the success or failure of the test, so we delete it.
        # Thanks to Andreas Koenig for identifying the problem.
        # http://rt.cpan.org/Public/Bug/Display.html?id=20807
        $file =~ s/eval\s+\d+/eval/;

        # handle backslashes on Win32 by converting them to forward slashes
        $file =~ s!\\!/!g if $^O eq 'MSWin32';
        return $file;
    },
    line => sub {
        $warning =~ /line (\d*)/;
        return $1;
    },
    warn => sub {
        $warning =~ /(.*?) at /;
        return $1;
    },
};

my $tt2err = Template->new({ INCLUDE_PATH => $dir })
    || die Template->error();
my $tt2not = Template->new({ INCLUDE_PATH => $dir, FILE_INFO => 0 })
    || die Template->error();

test_expect(\*DATA, [ err => $tt2err, not => $tt2not ], $vars);

__DATA__
-- test --
[% place = 'World' -%]
Hello [% place %]
[% a = a + 1 -%]
file: [% file %]
line: [% line %]
warn: [% warn %]
-- expect --
-- process --
Hello World
file: input text
line: 3
warn: Argument "" isn't numeric in addition (+)

-- test --
[% INCLUDE warning -%]
file: [% file.chunk(-16).last %]
line: [% line %]
warn: [% warn %]
-- expect --
-- process --
Hello
World
file: test/lib/warning
line: 2
warn: Argument "" isn't numeric in addition (+)

-- test --
-- use not --
[% INCLUDE warning -%]
file: [% file.chunk(-16).last %]
line: [% line %]
warn: [% warn %]
-- expect --
Hello
World
file: (eval)
line: 10
warn: Argument "" isn't numeric in addition (+)

-- test --
[% TRY; 
     INCLUDE chomp; 
   CATCH; 
     error; 
   END 
%]
-- expect --
file error - parse error - chomp line 6: unexpected token (END)
  [% END %]
