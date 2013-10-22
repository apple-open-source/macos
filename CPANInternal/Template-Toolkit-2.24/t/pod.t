#============================================================= -*-perl-*-
#
# t/pod.t
#
# Tests the 'Pod' plugin.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 2001 Andy Wardley. All Rights Reserved.
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
use Carp qw( confess );
$^W = 1;

$Template::Test::DEBUG = 0;
$Template::Test::PRESERVE = 1;
#$Template::View::DEBUG = 1;

eval "use Pod::POM";
if ($@) {
    skip_all('Pod::POM not installed');
}

my $config = {
    INCLUDE_PATH => 'templates:../templates',
#    RELATIVE     => 1,
#    POST_CHOMP   => 1,
};

my $vars = {
    podloc => -d 't' ? 't/test/pod' : 'test/pod',
};

test_expect(\*DATA, $config, $vars);

__DATA__
-- test --
[%  USE pod;
    pom = pod.parse("$podloc/no_such_file.pod");
    pom ? 'not ok' : 'ok'; ' - file does not exist';
%]
-- expect --
ok - file does not exist

-- test --
[%  USE pod;
    pom = pod.parse("$podloc/test1.pod");
    pom ? 'ok' : 'not ok'; ' - file parsed';
    global.pom = pom;
    global.warnings = pod.warnings;
%]
-- expect --
ok - file parsed

-- test --
[%  global.warnings.join("\n") %]
-- expect --
-- process --
spurious '>' at [% podloc %]/test1.pod line 17
spurious '>' at [% podloc %]/test1.pod line 21

-- test --
[% FOREACH h1 = global.pom.head1 -%]
* [% h1.title %]
[% END %]
-- expect --
* NAME
* SYNOPSIS
* DESCRIPTION
* THE END

-- test --
[% FOREACH h2 = global.pom.head1.2.head2 -%]
+ [% h2.title %]
[% END %]
-- expect --
+ First Subsection
+ Second Subsection

-- test --
[% PROCESS $item.type FOREACH item=global.pom.head1.2.content %]

[% BLOCK head2 -%]
<h2>[% item.title | trim %]</h2>
[% END %]

[% BLOCK text -%]
<p>[% item | trim %]</p>
[% END %]

[% BLOCK verbatim -%]
<pre>[% item | trim %]</pre>
[% END %]
-- expect --
<p>This is the description for My::Module.</p>
<pre>This is verbatim</pre>
<h2>First Subsection</h2>
<h2>Second Subsection</h2>

