#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# example that shows how to handle types specified in other schemas

package EncodedTypes;

sub as_TickDirection { $_[1] }
sub as_Exchanges { $_[1] }

package main;

use SOAP::Lite;

$d = SOAP::Deserializer->new;
$d->xmlschemas->{'http://marketdata.earthconnect.net/encodedTypes'} = 'EncodedTypes';

$r = $d->deserialize(q!<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope 
xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" 
xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" 
xmlns:tns="http://marketdata.earthconnect.net/" 
xmlns:types="http://marketdata.earthconnect.net/encodedTypes" 
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
xmlns:xsd="http://www.w3.org/2001/XMLSchema">

<soap:Body soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<tns:GetProfessionalQuoteResponse>
  <GetProfessionalQuoteResult href="#id1" />
</tns:GetProfessionalQuoteResponse>

<soapenc:Array id="id1" soapenc:arrayType="types:ProfessionalQuote[1]">
  <Item href="#id2" />
</soapenc:Array>

<types:ProfessionalQuote id="id2" xsi:type="types:ProfessionalQuote">
  <CompanyName xsi:type="xsd:string">EarthConnect Corporation</CompanyName>
  <LastPrice xsi:type="xsd:decimal">66.7</LastPrice>
  <LastPriceTime xsi:type="xsd:dateTime">2001-07-17T14:19:45.3310450-07:00</LastPriceTime>
  <Change xsi:type="xsd:decimal">0.34</Change>
  <Volume xsi:type="xsd:long">23456778</Volume>
  <Tick xsi:type="types:TickDirection">Down</Tick>
  <Bid xsi:type="xsd:decimal">88.21</Bid>
  <Ask xsi:type="xsd:decimal">88.22</Ask>
  <BidSize xsi:type="xsd:int">300</BidSize>
  <AskSize xsi:type="xsd:int">5800</AskSize>
  <DayLow xsi:type="xsd:decimal">64.8987</DayLow>
  <DayHigh xsi:type="xsd:decimal">68.4356</DayHigh>
  <Open xsi:type="xsd:decimal">87.43</Open>
  <PreviousClose xsi:type="xsd:decimal">86.34</PreviousClose>
  <LastTradeVolume xsi:type="xsd:int">640</LastTradeVolume>
  <Exchange xsi:type="types:Exchanges"> one of NASDAQ or NYSE or AMEX or INDEX</Exchange>
  <Valid href="#id3" />
</types:ProfessionalQuote>

<types:ProfessionalQuoteValues id="id3" xsi:type="types:ProfessionalQuoteValues">
  <CompanyName xsi:type="xsd:boolean">false</CompanyName>
  <LastPrice xsi:type="xsd:boolean">false</LastPrice>
  <LastPriceTime xsi:type="xsd:boolean">false</LastPriceTime>
  <Change xsi:type="xsd:boolean">false</Change>
  <Volume xsi:type="xsd:boolean">false</Volume>
  <Tick xsi:type="xsd:boolean">false</Tick>
  <Bid xsi:type="xsd:boolean">false</Bid>
  <Ask xsi:type="xsd:boolean">false</Ask>
  <BidSize xsi:type="xsd:boolean">false</BidSize>
  <AskSize xsi:type="xsd:boolean">false</AskSize>
  <DayLow xsi:type="xsd:boolean">false</DayLow>
  <DayHigh xsi:type="xsd:boolean">false</DayHigh>
  <Open xsi:type="xsd:boolean">false</Open>
  <PreviousClose xsi:type="xsd:boolean">false</PreviousClose>
  <LastTradeVolume xsi:type="xsd:boolean">false</LastTradeVolume>
</types:ProfessionalQuoteValues>
</soap:Body>

</soap:Envelope>!)->result;

print "Tick (types:TickDirection): ", $r->[0]->{Tick}, "\n";
print "Exchange (types:Exchanges): ", $r->[0]->{Exchange}, "\n";
