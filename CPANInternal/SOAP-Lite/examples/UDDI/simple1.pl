#!perl -w
#!d:\perl\bin\perl.exe 

# -- UDDI::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use UDDI::Lite +autodispatch =>
  proxy => 'http://uddi.microsoft.com/inquire',
;

my $list = find_business(name => 'microsoft');
my $bis = $list->businessInfos;
for ($bis->businessInfo) {
  my $s = $_->serviceInfos->serviceInfo or next; # skip if no service information
  print $s->name, ' ', $s->businessKey, "\n";
}
