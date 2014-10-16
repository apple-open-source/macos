#============================================================= -*-perl-*-
#
# t/html.t
#
# Tests the 'HTML' plugin.
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
use warnings;
use lib qw( ./lib ../lib );
use Template;
use Template::Test;
use Template::Plugin::HTML;

my $DEBUG = grep(/-d/, @ARGV);
$Template::Test::DEBUG =  $DEBUG;
$Template::Test::PRESERVE = $DEBUG;

#------------------------------------------------------------------------
# behaviour of html filter depends on these being available
#------------------------------------------------------------------------

use constant HAS_HTML_Entities => eval { 
    require HTML::Entities;
    1;
};
use constant HAS_Apache_Util   => eval { 
    require Apache::Util;
    Apache::Utils::escape_html('');
    1;
};

#print "Has HTML::Entities: ", HAS_HTML_Entities ? 'yes' : 'no', "\n";
#print "Has Apache::Util: ", HAS_Apache_Util ? 'yes' : 'no', "\n";

my $h = Template::Plugin::HTML->new('foo');
ok( $h, 'created HTML plugin' );

my $cfg  = { };
my $vars = {
    entities => HAS_HTML_Entities || HAS_Apache_Util || 0,
};

test_expect(\*DATA, $cfg, $vars); 

__DATA__
-- test --
-- name html plugin --
[% USE HTML -%]
OK
-- expect --
OK

-- test --
-- name html filter --
[% FILTER html -%]
< &amp; >
[%- END %]
-- expect --
&lt; &amp;amp; &gt;

-- test --
-- name html entity --
[%  TRY; 
        text = "Léon Brocard" | html_entity;

        IF text == "L&eacute;on Brocard";
            'passed';
        ELSIF text == "L&#233;on Brocard";
            'passed';
        ELSE;
            "failed: $text";
        END;
    CATCH;
        error;
    END;
%]
-- expect --
-- process --
[%  IF entities -%]
passed
[%- ELSE -%]
html_entity error - cannot locate Apache::Util or HTML::Entities
[%- END %]

-- test --
[% USE html; html.url('my file.html') -%]
-- expect --
my%20file.html

-- test --
-- name escape --
[% USE HTML -%]
[% HTML.escape("if (a < b && c > d) ...") %]
-- expect --
if (a &lt; b &amp;&amp; c &gt; d) ...

-- test --
-- name sorted --
[% USE HTML(sorted=1) -%]
[% HTML.element(table => { border => 1, cellpadding => 2 }) %]
-- expect --
<table border="1" cellpadding="2">

-- test --
-- name attributes --
[% USE HTML -%]
[% HTML.attributes(border => 1, cellpadding => 2).split.sort.join %]
-- expect --
border="1" cellpadding="2"

