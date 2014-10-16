#============================================================= -*-perl-*-
#
# t/service.t
#
# Test the Template::Service module.
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
use lib qw( ./lib ../lib );
use Template::Test;
use Template::Service;
use Template::Document;
use Template::Constants qw( :debug );

my $DEBUG = grep(/^--?d(debug)?$/, @ARGV);

my $dir    = -d 't' ? 't/test' : 'test';
my $config = {
    INCLUDE_PATH => "$dir/src:$dir/lib",
    PRE_PROCESS  => [ 'config', 'header' ],
    POST_PROCESS => 'footer',
    BLOCKS       => { 
	demo     => sub { return 'This is a demo' },
	astext   => "Another template block, a is '[% a %]'",
    },
    ERROR        => {
	barf     => 'barfed',
	default  => 'error',
    },
    DEBUG => $DEBUG ? DEBUG_SERVICE : 0,
};
my $tt1 = Template->new($config);

$config->{ AUTO_RESET } = 0;
my $tt2 = Template->new($config);

$config->{ ERROR } = 'barfed';
my $tt3 = Template->new($config);

$config->{ PRE_PROCESS  } = 'before';
$config->{ POST_PROCESS } = 'after';
$config->{ PROCESS } = 'process';
$config->{ WRAPPER } = 'outer';
my $tt4 = Template->new($config);

$config->{ WRAPPER } = [ 'outer', 'inner' ];
my $tt5 = Template->new($config);

my $replace = {
    title => 'Joe Random Title',
};


test_expect(\*DATA, [ 
    tt1 => $tt1, 
    tt2 => $tt2, 
    tt3 => $tt3,
    wrapper => $tt4,
    nested  => $tt5,
], $replace);

__END__
# test that headers and footers get added
-- test --
This is some text
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
This is some text
footer

# test that the 'demo' block (template sub) is defined
-- test --
[% INCLUDE demo %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
This is a demo
footer

# and also the 'astext' block (template text)
-- test --
[% INCLUDE astext a = 'artifact' %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
Another template block, a is 'artifact'
footer

# test that 'barf' exception gets redirected to the correct error template
-- test --
[% THROW barf 'Not feeling too good' %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
barfed: [barf] [Not feeling too good]
footer

# test all other errors get redirected correctly
-- test --
[% INCLUDE no_such_file %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
error: [file] [no_such_file: not found]
footer

# import some block definitions from 'blockdef'...
-- test --
[% PROCESS blockdef -%]
[% INCLUDE block1
   a = 'alpha'
%]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
start of blockdef

end of blockdef
This is block 1, defined in blockdef, a is alpha

footer

# ...and make sure they go away for the next service
-- test --
[% INCLUDE block1 %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
error: [file] [block1: not found]
footer

# now try it again with AUTO_RESET turned off...
-- test --
-- use tt2 --
[% PROCESS blockdef -%]
[% INCLUDE block1
   a = 'alpha'
%]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
start of blockdef

end of blockdef
This is block 1, defined in blockdef, a is alpha

footer

# ...and the block definitions should persist
-- test --
[% INCLUDE block1 a = 'alpha' %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
This is block 1, defined in blockdef, a is alpha

footer

# test that the 'demo' block is still defined
-- test --
[% INCLUDE demo %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
This is a demo
footer

# and also the 'astext' block
-- test --
[% INCLUDE astext a = 'artifact' %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
Another template block, a is 'artifact'
footer

# test that a single ERROR template can be specified
-- test --
-- use tt3 --
[% THROW food 'cabbages' %]
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
barfed: [food] [cabbages]
footer

-- test --
-- use wrapper --
[% title = 'The Foo Page' -%]
begin page content
title is "[% title %]"
end page content
-- expect --
This comes before
<outer title="The Foo Page">
begin process
begin page content
title is "The Foo Page"
end page content
end process
</outer>
This comes after

-- test --
-- use nested --
[% title = 'The Bar Page' -%]
begin page content
title is "[% title %]"
end page content
-- expect --
This comes before
<outer title="inner The Bar Page">
<inner title="The Bar Page">
begin process
begin page content
title is "The Bar Page"
end page content
end process
</inner>

</outer>
This comes after
