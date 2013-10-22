#============================================================= -*-perl-*-
#
# t/directory.t
#
# Tests the Directory plugin.
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
use Cwd;
$^W = 1;

if ($^O eq 'MSWin32') {
    skip_all('skipping tests on MS Win 32 platform');
}

#$Template::Test::PRESERVE = 1;
my $cwd = getcwd();
my $dir = -d 't' ? 't/test/dir' : 'test/dir';

my $dot = $dir;
$dot =~ s/[^\/]+/../g;

my $vars = {
    cwd  => $cwd,
    dir  => $dir,
    dot  => $dot,
};
test_expect(\*DATA, undef, $vars);

__DATA__
-- test --
[% TRY ;
     USE Directory ;
   CATCH ;
     error ;
   END
-%]
-- expect --
Directory error - no directory specified

-- test --
[% TRY ;
     USE Directory('/no/such/place') ;
   CATCH ;
     error.type ; ' error on ' ; error.info.split(':').0 ;
   END
-%]
-- expect --
Directory error on /no/such/place

-- test --
[% USE d = Directory(dir, nostat=1) -%]
[% d.path %]
-- expect --
-- process --
[% dir %]

-- test --
[% USE d = Directory(dir) -%]
[% d.path %]
-- expect --
-- process --
[% dir %]

-- test --
[% USE directory(dir) -%]
[% directory.path %]
-- expect --
-- process --
[% dir %]

-- test --
[% USE d = Directory(dir) -%]
[% FOREACH f = d.files -%]
   - [% f.name %]
[% END -%]
[% FOREACH f = d.dirs; NEXT IF f.name == 'CVS';  -%]
   * [% f.name %]
[% END %]
-- expect --
   - file1
   - file2
   - xyzfile
   * sub_one
   * sub_two

-- test --
[% USE dir = Directory(dir) -%]
[% INCLUDE dir %]
[% BLOCK dir -%]
* [% dir.name %]
[% FOREACH f = dir.files -%]
    - [% f.name %]
[% END -%]
[% FOREACH f = dir.dirs; NEXT IF f.name == 'CVS';  -%]
[% f.scan -%]
[% INCLUDE dir dir=f FILTER indent(4) -%]
[% END -%]
[% END -%]
-- expect --
* dir
    - file1
    - file2
    - xyzfile
    * sub_one
        - bar
        - foo
    * sub_two
        - waz.html
        - wiz.html

-- test --
[% USE dir = Directory(dir) -%]
* [% dir.path %]
[% INCLUDE dir %]
[% BLOCK dir;
     FOREACH f = dir.list ;
     NEXT IF f.name == 'CVS'; 
       IF f.isdir ; -%]
    * [% f.name %]
[%       f.scan ;
	 INCLUDE dir dir=f FILTER indent(4) ;
       ELSE -%]
    - [% f.name %]
[%     END ;
    END ;
   END -%]
-- expect --
-- process --
* [% dir %]
    - file1
    - file2
    * sub_one
        - bar
        - foo
    * sub_two
        - waz.html
        - wiz.html
    - xyzfile

-- test --
[% USE d = Directory(dir, recurse=1) -%]
[% FOREACH f = d.files -%]
   - [% f.name %]
[% END -%]
[% FOREACH f = d.dirs; NEXT IF f.name == 'CVS';  -%]
   * [% f.name %]
[% END %]
-- expect --
   - file1
   - file2
   - xyzfile
   * sub_one
   * sub_two

-- test --
[% USE dir = Directory(dir, recurse=1, root=cwd) -%]
* [% dir.path %]
[% INCLUDE dir %]
[% BLOCK dir;
     FOREACH f = dir.list ;
     NEXT IF f.name == 'CVS'; 
       IF f.isdir ; -%]
    * [% f.name %] => [% f.path %] => [% f.abs %]
[%       INCLUDE dir dir=f FILTER indent(4) ;
       ELSE -%]
    - [% f.name %] => [% f.path %] => [% f.abs %]
[%     END ;
    END ;
   END -%]
-- expect --
-- process --
* [% dir %]
    - file1 => [% dir %]/file1 => [% cwd %]/[% dir %]/file1
    - file2 => [% dir %]/file2 => [% cwd %]/[% dir %]/file2
    * sub_one => [% dir %]/sub_one => [% cwd %]/[% dir %]/sub_one
        - bar => [% dir %]/sub_one/bar => [% cwd %]/[% dir %]/sub_one/bar
        - foo => [% dir %]/sub_one/foo => [% cwd %]/[% dir %]/sub_one/foo
    * sub_two => [% dir %]/sub_two => [% cwd %]/[% dir %]/sub_two
        - waz.html => [% dir %]/sub_two/waz.html => [% cwd %]/[% dir %]/sub_two/waz.html
        - wiz.html => [% dir %]/sub_two/wiz.html => [% cwd %]/[% dir %]/sub_two/wiz.html
    - xyzfile => [% dir %]/xyzfile => [% cwd %]/[% dir %]/xyzfile

-- test --
[% USE dir = Directory(dir, recurse=1, root=cwd) -%]
* [% dir.path %]
[% INCLUDE dir %]
[% BLOCK dir;
     FOREACH f = dir.list ;
	NEXT IF f.name == 'CVS'; 
	IF f.isdir ; -%]
    * [% f.name %] => [% f.home %]
[%       INCLUDE dir dir=f FILTER indent(4) ;
       ELSE -%]
    - [% f.name %] => [% f.home %]
[%     END ;
    END ;
   END -%]
-- expect --
-- process --
* [% dir %]
    - file1 => [% dot %]
    - file2 => [% dot %]
    * sub_one => [% dot %]
        - bar => [% dot %]/..
        - foo => [% dot %]/..
    * sub_two => [% dot %]
        - waz.html => [% dot %]/..
        - wiz.html => [% dot %]/..
    - xyzfile => [% dot %]


-- test --
[% USE dir = Directory(dir) -%]
[% file = dir.file('xyzfile') -%]
[% file.name %]
-- expect --
xyzfile

-- test --
[% USE dir = Directory('.', root=dir) -%]
[% dir.name %]
[% FOREACH f = dir.files -%]
- [% f.name %]
[% END -%]
-- expect --
.
- file1
- file2
- xyzfile


-- test --
[% VIEW filelist -%]

[% BLOCK file -%]
f [% item.name %] => [% item.path %]
[% END -%]

[% BLOCK directory; NEXT IF item.name == 'CVS';  -%]
d [% item.name %] => [% item.path %]
[% item.content(view) | indent -%]
[% END -%]

[% END -%]
[% USE dir = Directory(dir, recurse=1) -%]
[% filelist.print(dir) %]
-- expect --
-- process --
d dir => [% dir %]
    f file1 => [% dir %]/file1
    f file2 => [% dir %]/file2
    d sub_one => [% dir %]/sub_one
        f bar => [% dir %]/sub_one/bar
        f foo => [% dir %]/sub_one/foo
    d sub_two => [% dir %]/sub_two
        f waz.html => [% dir %]/sub_two/waz.html
        f wiz.html => [% dir %]/sub_two/wiz.html
    f xyzfile => [% dir %]/xyzfile




