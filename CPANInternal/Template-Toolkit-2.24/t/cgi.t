#============================================================= -*-perl-*-
#
# t/cgi.t
#
# Test the CGI plugin.
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
use Template;
use Template::Test;
$^W = 1;

#$Template::Parser::DEBUG = 1;
#$Template::Parser::PRETTY = 1;
#$Template::Stash::DEBUG = 1;

eval "use CGI";
if ($@) {
    skip_all("no CGI module");
}


my $cgi = CGI->new('');
$cgi = join("\n", $cgi->checkbox_group(
		-name     => 'words',
                -values   => [ 'eenie', 'meenie', 'minie', 'moe' ],
	        -defaults => [ 'eenie', 'meenie' ],
)); 


test_expect(\*DATA, undef, { cgicheck => $cgi, barf => \&barf });

sub barf {
    carp('failed');
}


__END__
-- test --
[% USE cgi = CGI('id=abw&name=Andy+Wardley'); global.cgi = cgi -%]
name: [% global.cgi.param('name') %]
-- expect --
name: Andy Wardley

-- test --
name: [% global.cgi.param('name') %]

-- expect --
name: Andy Wardley

-- test --
[% FOREACH key = global.cgi.param.sort -%]
   * [% key %] : [% global.cgi.param(key) %]
[% END %]
-- expect --
   * id : abw
   * name : Andy Wardley

-- test --
[% FOREACH key = global.cgi.param().sort -%]
   * [% key %] : [% global.cgi.param(key) %]
[% END %]
-- expect --
   * id : abw
   * name : Andy Wardley

-- test --
[% FOREACH x = global.cgi.checkbox_group(
		name     => 'words'
                values   => [ 'eenie', 'meenie', 'minie', 'moe' ]
	        defaults => [ 'eenie', 'meenie' ] )   -%]
[% x %]
[% END %]

-- expect --
-- process --
[% cgicheck %]

-- test --
[% USE cgi('item=foo&items=one&items=two') -%]
item: [% cgi.params.item %]
item: [% cgi.params.item.join(', ') %]
items: [% cgi.params.items.join(', ') %]

-- expect --
item: foo
item: foo
items: one, two

