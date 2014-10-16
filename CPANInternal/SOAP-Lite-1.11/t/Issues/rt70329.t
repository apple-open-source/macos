#!/usr/bin/perl
#
# Tests the error described in rt.cpan.org #70329
# 
# The error came from handling multiref ids, and unconditionally
# overwriting the id attribute
#
#
use Test::More tests => 1;
use 5.006;
use strict;
use warnings;
use SOAP::Lite +trace => [ 'debug' ];

my $req1 = SOAP::Lite->new(
    readable => 1,
    autotype => 0,
    proxy    => 'LOOPBACK://',
);

# req1 does not generate the XML attribute <item id="0"> it just generates
# <item>

my $content = SOAP::Data->new(
        name => 'item',
        attr => { "id" => 1 },
        value => \SOAP::Data->new(
            name => 'foo',
            value => 1,
        ),
     );

my $response = $req1->requestMessage(\$content);
my $response_item = $response->dataof("//item");

is($content->attr->{ id }, $response_item->attr->{ id });

