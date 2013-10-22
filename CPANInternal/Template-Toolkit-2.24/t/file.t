#============================================================= -*-perl-*-
#
# t/file.t
#
# Tests the File plugin.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 2000 Andy Wardley. All Rights Reserved.
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
use Template::Plugin::File;
$^W = 1;

if ($^O eq 'MSWin32') {
    skip_all('skipping tests on MS Win 32 platform');
}

#
my $dir  = -d 't' ? 't/test' : 'test';
my $file = "$dir/src/foo";
my @stat;
(@stat = stat $file)
    || die "$file: $!\n";

my $vars = {
    dir  => $dir,
    file => $file,
};
@$vars{ @Template::Plugin::File::STAT_KEYS } = @stat;

test_expect(\*DATA, undef, $vars);

__DATA__
-- test --
[% USE f = File('/foo/bar/baz.html', nostat=1) -%]
p: [% f.path %]
r: [% f.root %]
n: [% f.name %]
d: [% f.dir %]
e: [% f.ext %]
h: [% f.home %]
a: [% f.abs %]
-- expect --
p: /foo/bar/baz.html
r: 
n: baz.html
d: /foo/bar
e: html
h: ../..
a: /foo/bar/baz.html

-- test --
[% USE f = File('foo/bar/baz.html', nostat=1) -%]
p: [% f.path %]
r: [% f.root %]
n: [% f.name %]
d: [% f.dir %]
e: [% f.ext %]
h: [% f.home %]
a: [% f.abs %]
-- expect --
p: foo/bar/baz.html
r: 
n: baz.html
d: foo/bar
e: html
h: ../..
a: foo/bar/baz.html

-- test --
[% USE f = File('baz.html', nostat=1) -%]
p: [% f.path %]
r: [% f.root %]
n: [% f.name %]
d: [% f.dir %]
e: [% f.ext %]
h: [% f.home %]
a: [% f.abs %]
-- expect --
p: baz.html
r: 
n: baz.html
d: 
e: html
h: 
a: baz.html


-- test --
[% USE f = File('bar/baz.html', root='/foo', nostat=1) -%]
p: [% f.path %]
r: [% f.root %]
n: [% f.name %]
d: [% f.dir %]
e: [% f.ext %]
h: [% f.home %]
a: [% f.abs %]
-- expect --
p: bar/baz.html
r: /foo
n: baz.html
d: bar
e: html
h: ..
a: /foo/bar/baz.html


-- test -- 
[% USE f = File('bar/baz.html', root='/foo', nostat=1) -%]
p: [% f.path %]
h: [% f.home %]
rel: [% f.rel('wiz/waz.html') %]
-- expect --
p: bar/baz.html
h: ..
rel: ../wiz/waz.html


-- test -- 
[% USE baz = File('foo/bar/baz.html', root='/tmp/tt2', nostat=1) -%]
[% USE waz = File('wiz/woz/waz.html', root='/tmp/tt2', nostat=1) -%]
[% baz.rel(waz) %]
-- expect --
../../wiz/woz/waz.html


-- test --
[% USE f = File('foo/bar/baz.html', nostat=1) -%]
[[% f.atime %]]
-- expect --
[]

-- test --
[% USE f = File(file) -%]
[% f.path %]
[% f.name %]
-- expect --
-- process --
[% dir %]/src/foo
foo

-- test --
[% USE f = File(file) -%]
[% f.path %]
[% f.mtime %]
-- expect --
-- process --
[% dir %]/src/foo
[% mtime %]

-- test --
[% USE file(file) -%]
[% file.path %]
[% file.mtime %]
-- expect --
-- process --
[% dir %]/src/foo
[% mtime %]

-- test --
[% TRY -%]
[% USE f = File('') -%]
n: [% f.name %]
[% CATCH -%]
Drat, there was a [% error.type %] error.
[% END %]
-- expect --
Drat, there was a File error.


