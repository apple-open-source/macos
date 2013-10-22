#!/usr/bin/perl

use strict;
use warnings;
use DateTime::Format::W3CDTF;

my $dt		= DateTime->now;
my $format	= DateTime::Format::W3CDTF->new;
print $format->format_datetime($dt) . "\n";
