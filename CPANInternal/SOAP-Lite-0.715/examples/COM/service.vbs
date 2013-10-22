' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

MsgBox CreateObject("SOAP.Lite").new.service("http://www.xmethods.net/sd/StockQuoteService.wsdl").getQuote("MSFT")
