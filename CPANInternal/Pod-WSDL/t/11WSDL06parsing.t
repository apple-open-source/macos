#!/usr/bin/perl -w
package Pod::WSDL;
use Test::More tests => 1;
BEGIN {use_ok('Pod::WSDL');}
use lib length $0 > 10 ? substr $0, 0, length($0) - 16 : '.';
use strict;
use warnings;
