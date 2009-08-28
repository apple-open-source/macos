#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# example for Microsoft's TerraServer http://terraserver.microsoft.net/
# thanks to Ivan R. Judson <judson@mcs.anl.gov> for his help

use SOAP::Lite;

# terraserver doesn't like charset in content-type
$SOAP::Constants::DO_NOT_USE_CHARSET = 1;

my $terra = SOAP::Lite
  ->proxy('http://terraserver.microsoft.net/TerraService.asmx')
  ->on_action(sub { join '/', 'http://terraservice.net/terraserver', $_[1] })
  ->uri('http://tempuri.org/')
;

my $response = $terra->GetTheme(SOAP::Data->name(theme => 'Photo'));

if ($response->fault) {
  die $response->faultstring;
} else {
  my %result = %{$response->result};
  print map("$_: @{[$result{$_} || '']}\n", keys %result), "\n";
}

my $method = SOAP::Data->name('GetPlaceList')
  ->attr({xmlns => 'http://tempuri.org/'});

my @params = (
  SOAP::Data->name(placeName => 'Chicago'),
  SOAP::Data->name(MaxItems => 10),
  SOAP::Data->name(imagePresence => 'true')
);

$response = $terra->call($method => @params);

if ($response->fault) {
  print $response->faultcode, " ", $response->faultstring, "\n";
} else {
  foreach ($response->valueof('//PlaceFacts')) {
    my %result = %{$_->{Place}};
    print map("$_: $result{$_}\n", keys %result), "\n";
  }
}
