#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# stub interface (created with stubmaker.pl)
# perl stubmaker.pl http://www.xmethods.net/sd/StockQuoteService.wsdl

use StockQuoteService;

my $service = StockQuoteService->new;
print $service->getQuote('MSFT'), "\n";
