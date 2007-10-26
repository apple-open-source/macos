#!/usr/bin/perl -w
use strict;

=pod

This example shows a simple fall through parser that tries
a few of the other formatting modules, _then_ fails.

=cut

package DateTime::Format::Fall;
use DateTime::Format::HTTP;
use DateTime::Format::Mail;
use DateTime::Format::IBeat;

use DateTime::Format::Builder (
parsers => { parse_datetime => [
    sub { eval { DateTime::Format::HTTP->parse_datetime( $_[1] ) } },
    sub { eval { DateTime::Format::Mail->parse_datetime( $_[1] ) } },
    sub { eval { DateTime::Format::IBeat->parse_datetime( $_[1] ) } },
]});

package main;

for ( '@d19.07.03 @704', '20030719T155345', 'gibberish' )
{
    print DateTime::Format::Fall->parse_datetime($_)->datetime, "\n";
}
