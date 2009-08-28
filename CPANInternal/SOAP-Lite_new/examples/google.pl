#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# Google Web API: http://www.google.com/apis/
# NB: Register and get your own key first
# see also:
#   http://interconnected.org/home/more/GoogleSearch.pl.txt
#   http://aaronland.net/weblog/archive/4205
#   http://www.oreillynet.com/cs/weblog/view/wlg/1283

use SOAP::Lite;

my $key = '0'; # <<< put your key here
my $query = shift || 'soap';

# use GoogleSearch.wsdl file from Google developer's kit
# update path to file to make it work
# GoogleSearch.wsdl is NOT included
my $google = SOAP::Lite->service('file://GoogleSearch.wsdl');
my $result = $google->doGoogleSearch(
  $key, $query, 0, 10, 'false', '', 'false', '', 'latin1', 'latin1');

die $google->call->faultstring if $google->call->fault;
print "About $result->{'estimatedTotalResultsCount'} results.\n";
