# $Id: 00compile.t,v 1.1 2005/12/29 19:49:25 nicolaw Exp $

use Test::More tests => 2;

require_ok('Sys::Filesystem');
use_ok('Sys::Filesystem');

diag("Testing Sys::Filesystem $Sys::Filesystem::VERSION, Perl $], $^X");
