#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use strict;
use SOAP::Lite;
use Text::Wrap;

my $res = SOAP::Lite
  -> uri('urn:vgx-joke')
  -> proxy('http://services.xmltoday.com/vx_engine/soap-trigger.pperl')
  -> JokeOfTheDay
;

die $res->faultstring if $res->fault;

printf "%s [%s]\n", $res->result->{title}, $res->result->{score};
print wrap("\t", '', split( /\n/, $res->result->{text})), "\n";
