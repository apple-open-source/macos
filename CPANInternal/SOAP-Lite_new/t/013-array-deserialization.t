#!/usr/bin/perl
use strict;
use Test;
plan tests => 5;

local $/ = undef;
my $xml = <DATA>;
use SOAP::Lite;
my $som = SOAP::Deserializer->new->deserialize($xml);
my $result = $som->result();

ok (@$result == 2);
ok $result->[0]->isa('outer');
ok $result->[1]->isa('outer');
ok $result->[1]->{ kids }->[0]->isa('inner');
ok $result->[1]->{ kids }->[1]->isa('inner');

__DATA__
<?xml version="1.0" encoding="UTF-8"?>
<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
                  xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
                  xmlns:apachesoap="http://xml.apache.org/xml-soap"
                  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
                  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                  xmlns:my="urn:MyNamespace"
                  soapenv:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
 <soapenv:Body>
  <my:response>
   <my:return href="#id0"/>
  </my:response>
  <multiRef id="id0" soapenc:root="0" xsi:type="apachesoap:Vector">
   <item href="#id1"/>
   <item href="#id5"/>
  </multiRef>
  <multiRef id="id1" soapenc:root="0" xsi:type="my:outer">
   <name xsi:type="xsd:string">a</name>
   <kids href="#id2"/>
  </multiRef>
  <multiRef id="id2" soapenc:root="0" xsi:type="apachesoap:Vector">
   <item href="#id3"/>
   <item href="#id4"/>
  </multiRef>
  <multiRef id="id3" soapenc:root="0" xsi:type="my:inner">
   <name xsi:type="xsd:string">aa</name>
  </multiRef>
  <multiRef id="id4" soapenc:root="0" xsi:type="my:inner">
   <name xsi:type="xsd:string">ab</name>
  </multiRef>
  <multiRef id="id5" soapenc:root="0" xsi:type="my:outer">
   <name xsi:type="xsd:string">b</name>
   <kids href="#id6"/>
  </multiRef>
  <multiRef id="id6" soapenc:root="0" xsi:type="apachesoap:Vector">
   <item href="#id7"/>
   <item href="#id8"/>
  </multiRef>
  <multiRef id="id7" soapenc:root="0" xsi:type="my:inner">
   <name xsi:type="xsd:string">ba</name>
  </multiRef>
  <multiRef id="id8" soapenc:root="0" xsi:type="my:inner">
   <name xsi:type="xsd:string">bb</name>
  </multiRef>
 </soapenv:Body>
</soapenv:Envelope>
