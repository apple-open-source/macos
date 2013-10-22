#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

BEGIN { plan tests => 131 }

use SOAP::Lite;
$SIG{__WARN__} = sub { ; }; # turn off deprecation warnings

my($a, $s, $r, $serialized, $deserialized);

{ # check root, mustUnderstand
  print "root and mustUnderstand attributes with SOAP::Data test(s)...\n";

  $serialized = SOAP::Serializer->serialize(SOAP::Data->root(1 => 1)->name('rootandunderstand')->mustUnderstand(1));

  ok($serialized =~ m!<rootandunderstand( xsi:type="xsd:int"| soap:mustUnderstand="1"| soapenc:root="1"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){7}>1</rootandunderstand>!);
}

{ # check deserialization of envelope with result
  print "Deserialization of envelope with result test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	 soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
<m:doublerResponse xmlns:m="http://simon.fell.com/calc">
<nums xsi:type="soapenc:Array" soapenc:arrayType="xsd:int[5]">
<item xsi:type="xsd:int">20</item>
<item xsi:type="xsd:int">40</item>
<item xsi:type="xsd:int">60</item>
<item xsi:type="xsd:int">100</item>
<item xsi:type="xsd:int">200</item>
</nums>
</m:doublerResponse>
</soap:Body>
</soap:Envelope>
');

  ok($deserialized->result->[2] == 60);
  ok((my @array = $deserialized->paramsall) == 1);
  ok(ref $deserialized->body eq 'HASH'); # not blessed anymore since 0.51
}

{ # check deserialization of envelope with fault
  print "Deserialization of envelope with fault test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsd="http://www.w3.org/1999/XMLSchema" soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
<soap:Body>
<soap:Fault><faultcode>soap:Client</faultcode><faultstring>Application Error</faultstring><detail>Invalid Password</detail></soap:Fault></soap:Body></soap:Envelope>
');

  ok($deserialized->faultcode eq 'soap:Client');
  ok($deserialized->faultstring eq 'Application Error');
  ok($deserialized->faultdetail eq 'Invalid Password');
}

{ # check deserialization of circular references
  print "Deserialization of circular references test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<Struct prefix:id="123" xmlns:prefix="aaa" id="ref-0xb61350"><a id="ref-0xb61374"><b href="#ref-0xb61350"/></a></Struct>
');

  ok(ref $deserialized->valueof('/Struct') eq ref $deserialized->valueof('//b'));

  ok($deserialized->dataof('/Struct')->attr->{'{aaa}id'} == 123); 
  ok(exists $deserialized->dataof('/Struct')->attr->{'id'});
}

{ # check SOAP::SOM 
  print "SOM test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope  xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/1999/XMLSchema"
	 xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
	soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
<m:doublerResponse xmlns:m="http://simon.fell.com/calc">
<nums>
<item1 xsi:type="xsd:int">20</item1>
<item1 xsi:type="xsd:int">40</item1>
<item2 xsi:type="xsd:int">60</item2>
<item2 xsi:type="xsd:int">100</item2>
<item3 xsi:type="xsd:int">200</item3>
</nums>
</m:doublerResponse>
</soap:Body>
</soap:Envelope>
');

  # should return STRING '/Envelope/Body/[1]/[1]'
  my $result = SOAP::SOM::result; 
  ok($deserialized->valueof("$result/[1]") == 20);
  ok($deserialized->valueof("$result/[3]") == 60);
  ok($deserialized->valueof("$result/[5]") == 200);

  # match should return true/false in boolean context (and object ref otherwise)
  ok($deserialized->match('aaa') ? 0 : 1);

  # should return same string as above
  ok($deserialized->match(SOAP::SOM->result));

  ok($deserialized->valueof('[1]') == 20);
  ok($deserialized->valueof('[3]') == 60);
  ok($deserialized->valueof('[5]') == 200);

  $deserialized->match('//Body/[1]/[1]'); # match path and change current node on success
  ok($deserialized->valueof('[1]') == 20);
  ok($deserialized->valueof('[3]') == 60);
  ok($deserialized->valueof('[5]') == 200);
}

{ # check output parameters   
  print "Output parameters test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/1999/XMLSchema"
	 xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
	soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
  <mehodResponse>
    <res1>name1</res1>
    <res2>name2</res2>
    <res3>name3</res3>
  </mehodResponse>
</soap:Body>
</soap:Envelope>
');
  my @paramsout = $deserialized->paramsout;

  ok($paramsout[0] eq 'name2' && $paramsout[1] eq 'name3');
}

{ # check nonqualified namespace   
  print "Nonqualified namespace test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('
<soap:Envelope  xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/1999/XMLSchema"
	 xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
	soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
<doublerResponse xmlns="http://simon.fell.com/calc">
<nums xsi:type="soapenc:Array" soapenc:arrayType="xsd:int[5]">
<item xsi:type="xsd:int">20</item>
<item xsi:type="xsd:int">40</item>
<item xsi:type="xsd:int">60</item>
<item xsi:type="xsd:int">100</item>
<item xsi:type="xsd:int">200</item>
</nums>
</doublerResponse>
</soap:Body>
</soap:Envelope>
');

  ok($deserialized->namespaceuriof(SOAP::SOM::method) eq 'http://simon.fell.com/calc');
  ok($deserialized->namespaceuriof('//doublerResponse') eq 'http://simon.fell.com/calc');
}

{ # check for Array of Array serialization 
  print "Array of Array serialization test(s)...\n";

  $serialized = SOAP::Serializer
    ->readable(1)
    ->method('mymethod' => [[1, 2], [3, 4]]);
  ok($serialized =~ m!soapenc:arrayType="soapenc:Array\[2\]"!);
}

{ # check for serialization with SOAPStruct
  print "Serialization w/out SOAPStruct test(s)...\n";
  $a = { a => 1 };
  $serialized = SOAP::Serializer->namespaces({})->serialize($a);
  ok($serialized =~ m!<c-gensym(\d+)><a xsi:type="xsd:int">1</a></c-gensym\1>!);
}

{ # check header/envelope serialization/deserialization   
  print "Header/Envelope serialization/deserialization test(s)...\n";

  $serialized = SOAP::Serializer->method( # same as ->envelope(method =>
      'mymethod', 1, 2, 3, 
      SOAP::Header->name(t1 => 5)->mustUnderstand(1)->uri('http://namespaces.soaplite.com/headers'),
      SOAP::Header->name(t2 => 7)->mustUnderstand(2),
  );
  $deserialized = SOAP::Deserializer->deserialize($serialized);

  my $t1 = $deserialized->match(SOAP::SOM::header)->headerof('t1');
  my $t2 = $deserialized->dataof('t2');
  my $t3 = eval { $deserialized->headerof(SOAP::SOM::header . '/{http://namespaces.soaplite.com/headers}t3'); };

  ok(!$@ && !defined $t3);

  my @paramsin = $deserialized->paramsin;
  my @paramsall = $deserialized->paramsall;

  ok($t2->type =~ /^int$/);
  ok($t2->mustUnderstand == 1);
  ok(@paramsin == 3);
  ok(@paramsall == 3);

  eval { $deserialized->result(1) };
  ok($@ =~ /Method 'result' is readonly/);

  $serialized = SOAP::Serializer->method( # same as ->envelope(method =>
      SOAP::Data->name('mymethod')->attr({something => 'value'}), 1, 2, 3, 
  );
  ok($serialized =~ /<mymethod something="value">/);

  $serialized = SOAP::Serializer
    -> envprefix('')
    -> method('mymethod');

  ok($serialized =~ m!<Envelope(?: xmlns:namesp\d+="http://schemas.xmlsoap.org/soap/envelope/"| namesp\d+:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){5}><Body><mymethod xsi:nil="true" /></Body></Envelope>!);
  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0" encoding="UTF-8"?><soap:Envelope xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" xmlns:xsd="http://www.w3.org/2001/XMLSchema"><soap:Body><getStateName><c-gensym5 xsi:type="xsd:int">1</c-gensym5></getStateName></soap:Body></soap:Envelope>');
  ok(! defined $deserialized->namespaceuriof('//getStateName'));

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0" encoding="UTF-8"?><soap:Envelope xmlns="a" xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" xmlns:xsd="http://www.w3.org/1999/XMLSchema"><soap:Body><getStateName><c-gensym5 xsi:type="xsd:int">1</c-gensym5></getStateName></soap:Body></soap:Envelope>');
  ok($deserialized->namespaceuriof('//getStateName') eq 'a');
}

{ # Map type serialization/deserialization
  print "Map type serialization/deserialization test(s)...\n";

  my $key = "\0\1";
  $serialized = SOAP::Serializer->method(aa => SOAP::Data->type(map => {a => 123, $key => 456})->name('maaap'));

  { local $^W; # disable warning on implicit map encoding
    my $implicit = SOAP::Serializer->method(aa => SOAP::Data->name(maaap => {a => 123, $key => 456}));
    ok($implicit eq $serialized);
  }
  ok($serialized =~ /apachens:Map/);
  ok($serialized =~ m!xmlns:apachens="http://xml.apache.org/xml-soap"!);

  $deserialized = SOAP::Deserializer->deserialize($serialized);
  $a = $deserialized->valueof('//maaap');
  ok(UNIVERSAL::isa($a => 'HASH'));
  ok(ref $a && $a->{$key} == 456);
}

{ # Stringified type serialization
  print "Stringified type serialization test(s)...\n";

  $serialized = SOAP::Serializer->serialize(bless { a => 1, _current => [] } => 'SOAP::SOM');
  
  my $test = $serialized;
  ok $test =~s{
            <\?xml \s version="1.0" \s encoding="UTF-8"\?>
            <SOAP__SOM
            (?: 
                \sxsi:type="namesp(\d+):SOAP__SOM"
                | \sxmlns:namesp\d+="http://namespaces.soaplite.com/perl"
                | \sxmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
                | \sxmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
                | \sxmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                | \sxmlns:xsd="http://www.w3.org/2001/XMLSchema"){6}>
  }{}xms;

  ok $test =~s{
      </SOAP__SOM> \z
  }{}xms;

  ok $test =~s{ <a \s xsi:type="xsd:int">1</a> }{}xms;
  ok $test =~s{ <_current (:? 
        \s soapenc:arrayType="xsd:anyType\[0\]"
        | \s xsi:type="soapenc:Array" ){2}
       \s/>
    }{}xms;

  ok length $test == 0;
  
  # Replaced complex regex by several simpler (see above).
  
  # ok($serialized =~ m!<SOAP__SOM(?: xsi:type="namesp(\d+):SOAP__SOM"| xmlns:namesp\d+="http://namespaces.soaplite.com/perl"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){6}><a xsi:type="xsd:int">1</a><_current(?: soapenc:arrayType="xsd:anyType\[0\]"| xsi:type="soapenc:Array"){2} /></SOAP__SOM>!);
  # ok( ($serialized =~ m!<SOAP__SOM(?: xsi:type="namesp(\d+):SOAP__SOM"| xmlns:namesp\d+="http://namespaces.soaplite.com/perl"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){6}><a xsi:type="xsd:int">1</a><_current(?: soapenc:arrayType="xsd:anyType\[0\]"| xsi:type="soapenc:Array"){2}/></SOAP__SOM>!) 
  # ||  ($serialized =~ m!<SOAP__SOM(?: xsi:type="namesp(\d+):SOAP__SOM"| xmlns:namesp\d+="http://namespaces.soaplite.com/perl"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){6}><_current(?: soapenc:arrayType="xsd:anyType\[0\]"| xsi:type="soapenc:Array"){2}/><a xsi:type="xsd:int">1</a></SOAP__SOM>!));
  #print $serialized;
  #  exit;

  $serialized =~ s/__/./g; # check for SOAP.SOM instead of SOAP__SOM
  ok(ref SOAP::Deserializer->deserialize($serialized)->root eq 'SOAP::SOM');
}

{ # Serialization of non-allowed element
  print "Serialization of non-allowed element test(s)...\n";

  eval { $serialized = SOAP::Serializer->serialize(SOAP::Data->name('---' => 'aaa')) };

  ok($@ =~ /^Element/);
}

{ # Custom serialization of blessed reference
  print "Custom serialization of blessed reference test(s)...\n";

  eval q!
    sub SOAP::Serializer::as_My__Own__Class {
      my $self = shift;
      my($value, $name, $type, $attr) = @_;
      return [$name, {%{$attr || {}}, 'xsi:type' => 'xsd:string'}, join ', ', map {"$_ => $value->{$_}"} sort keys %$value];
    }
    1;
  ! or die;

  $serialized = SOAP::Serializer->serialize(bless {a => 1, b => 2} => 'My::Own::Class');
  ok($serialized =~ m!<My__Own__Class( xsi:type="xsd:string"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){5}>a => 1, b => 2</My__Own__Class>!);
}

{ # Multirefs serialization
  print "Multirefs serialization test(s)...\n";

  my $b = { b => 2 };
  my $a = { a => $b };
  my $c = { c1 => $a, c2 => $a };

  $serialized = SOAP::Serializer->autotype(0)->method(a => $c);
  ok($serialized =~ m!<soap:Body><a><c-gensym(\d+)><c1 href="#ref-(\d+)" /><c2 href="#ref-\2" /></c-gensym\1></a><c-gensym(\d+) id="ref-(\d+)"><b>2</b></c-gensym\3><c-gensym(\d+) id="ref-\2"><a href="#ref-\4" /></c-gensym\5></soap:Body>! ||
     $serialized =~ m!<soap:Body><a><c-gensym(\d+)><c1 href="#ref-(\d+)" /><c2 href="#ref-\2" /></c-gensym\1></a><c-gensym(\d+) id="ref-\2"><a href="#ref-(\d+)" /></c-gensym\3><c-gensym(\d+) id="ref-\4"><b>2</b></c-gensym\5></soap:Body>! ||
     $serialized =~ m!<soap:Body><a><c-gensym(\d+)><c2 href="#ref-(\d+)" /><c1 href="#ref-\2" /></c-gensym\1></a><c-gensym(\d+) id="ref-(\d+)"><b>2</b></c-gensym\3><c-gensym(\d+) id="ref-\2"><a href="#ref-\4" /></c-gensym\5></soap:Body>! ||
     $serialized =~ m!<soap:Body><a><c-gensym(\d+)><c2 href="#ref-(\d+)" /><c1 href="#ref-\2" /></c-gensym\1></a><c-gensym(\d+) id="ref-\2"><a href="#ref-(\d+)" /></c-gensym\3><c-gensym(\d+) id="ref-\4"><b>2</b></c-gensym\5></soap:Body>!);

  $serialized = SOAP::Serializer->autotype(0)->namespaces({})->serialize($c);
  ok($serialized =~ m!<c-gensym(\d+)><c1 href="#ref-(\d+)" /><c2 href="#ref-\2" /><c-gensym(\d+) id="ref-(\d+)"><b>2</b></c-gensym\3><c-gensym(\d+) id="ref-\2"><a href="#ref-\4" /></c-gensym\5></c-gensym\1>! ||
     $serialized =~ m!<c-gensym(\d+)><c1 href="#ref-(\d+)" /><c2 href="#ref-\2" /><c-gensym(\d+) id="ref-\2"><a href="#ref-(\d+)" /></c-gensym\3><c-gensym(\d+) id="ref-\4"><b>2</b></c-gensym\5></c-gensym\1>! ||
     $serialized =~ m!<c-gensym(\d+)><c2 href="#ref-(\d+)" /><c1 href="#ref-\2" /><c-gensym(\d+) id="ref-(\d+)"><b>2</b></c-gensym\3><c-gensym(\d+) id="ref-\2"><a href="#ref-\4" /></c-gensym\5></c-gensym\1>! ||
     $serialized =~ m!<c-gensym(\d+)><c2 href="#ref-(\d+)" /><c1 href="#ref-\2" /><c-gensym(\d+) id="ref-\2"><a href="#ref-(\d+)" /></c-gensym\3><c-gensym(\d+) id="ref-\4"><b>2</b></c-gensym\5></c-gensym\1>!);

  my $root = SOAP::Deserializer->deserialize($serialized)->root;

  ok($root->{c1}->{a}->{b} == 2);
  ok($root->{c2}->{a}->{b} == 2);
}

{ # Serialization of multirefs shared between Header and Body
  print "Serialization of multirefs shared between Header and Body test(s)...\n";

  $a = { b => 2 };

  print $serialized = SOAP::Serializer->autotype(0)->method(a => SOAP::Header->value($a), $a);
  print "\n";
  print '<soap:Header><c-gensym\d+ href="#ref-(\d+)" /></soap:Header><soap:Body><a><c-gensym\d+ href="#ref-\1" /></a><c-gensym(\d+) id="ref-\1"><b>2</b></c-gensym\2></soap:Body>', "\n";
  ok($serialized =~ m!<soap:Header><c-gensym\d+ href="#ref-(\d+)" /></soap:Header><soap:Body><a><c-gensym\d+ href="#ref-\1" /></a><c-gensym(\d+) id="ref-\1"><b>2</b></c-gensym\2></soap:Body>!);
}

{ # Deserialization with typecast
  print "Deserialization with typecast test(s)...\n";

  my $desc = 0;
  my $typecasts = 0;
  eval { 
    package MyDeserializer; 
    @MyDeserializer::ISA = 'SOAP::Deserializer';
    sub typecast;
    *typecast = sub { shift; 
      my($value, $name, $attrs, $children, $type) = @_;
      $desc = "$name @{[scalar @$children]}" if $name eq 'a';
      $typecasts++;
      return;
    };
    1;
  } or die;

  $deserialized = MyDeserializer->deserialize('<a><b>1</b><c>2</c></a>');
  ok($desc eq 'a 2'); #! fix "if $name eq 'a'", because $name is QName now ('{}a')
  ok($typecasts == 5);
}

{ # Deserialization with wrong encodingStyle
  print "Deserialization with wrong encodingStyle test(s)...\n";

  eval { $deserialized = SOAP::Deserializer->deserialize(
'<a 
   soap:encodingStyle="http://schemas.microsoft.com/soap/encoding/clr/1.0 http://schemas.xmlsoap.org/soap/encoding/"
   xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
>1</a>') };
  ok(!$@ && $deserialized);

  eval { $deserialized = SOAP::Deserializer->deserialize(
'<a 
   soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
   xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
>1</a>') };
  ok(!$@ && $deserialized);

  eval { $deserialized = SOAP::Deserializer->deserialize(
'<a 
   soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/something"
   xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
>1</a>') };
  ok(!$@ && $deserialized);

  eval { $deserialized = SOAP::Deserializer->deserialize(
'<a>1</a>') };
  ok(!$@ && $deserialized);

  eval { $deserialized = SOAP::Deserializer->deserialize(
'<a 
   soap:encodingStyle=""
   xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
>1</a>') };
  ok(!$@ && $deserialized);
}

{ # Deserialization with root attribute
  print "Deserialization with root attribute test(s)...\n";

  # root="0", should skip
  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/1999/XMLSchema"
	 xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
     soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
<m:doublerResponse1 soapenc:root="0" xmlns:m="http://soaplite.com/">
<nums>1</nums>
</m:doublerResponse1>
<m:doublerResponse2 xmlns:m="http://soaplite.com/">
<nums>2</nums>
</m:doublerResponse2>
</soap:Body>
</soap:Envelope>
');

  ok($deserialized->result == 2);

  # root="0", but in wrong namespace
  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/1999/XMLSchema"
	 xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
     soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
<m:doublerResponse1 root="0" xmlns:m="http://soaplite.com/">
<nums>1</nums>
</m:doublerResponse1>
<m:doublerResponse2 xmlns:m="http://soaplite.com/">
<nums>2</nums>
</m:doublerResponse2>
</soap:Body>
</soap:Envelope>
');

  ok($deserialized->result == 1);

  # root="1"
  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/1999/XMLSchema"
	 xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
     soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<soap:Body>
<m:doublerResponse1 soapenc:root="1" xmlns:m="http://soaplite.com/">
<nums>1</nums>
</m:doublerResponse1>
<m:doublerResponse2 xmlns:m="http://www.soaplite.com/2">
<nums>2</nums>
</m:doublerResponse2>
<m:doublerResponse2 xmlns:m="http://www.soaplite.com/3">
<nums>3</nums>
</m:doublerResponse2>
<doublerResponse2 xmlns="">
<nums>4</nums>
</doublerResponse2>
</soap:Body>
</soap:Envelope>
');

  ok($deserialized->result == 1);
  ok($deserialized->valueof('//{http://www.soaplite.com/2}doublerResponse2/nums') == 2);
  ok($deserialized->valueof('//{http://www.soaplite.com/3}doublerResponse2/nums') == 3);
  ok($deserialized->valueof('//{}doublerResponse2/nums') == 4);
  my @nums = $deserialized->valueof('//doublerResponse2/nums');
  ok(@nums == 3);
  ok($nums[0] == 2 && $nums[1] == 3);
  my $body = $deserialized->body;
  ok(ref $body->{doublerResponse1} && ref $body->{doublerResponse2});
}

{ 
  print "Deserialization with null elements test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soap:Envelope xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsd="http://www.w3.org/1999/XMLSchema" soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
<soap:Body>
<namesp23:object_infoResponse xmlns:namesp23="http://localhost/Test">
<soapenc:Array xsi:type="soapenc:Array" soapenc:arrayType="xsd:integer[]">
<item xsi:type="xsd:string">1</item>
<item xsi:type="xsd:string">2</item>
<item xsi:null="1"/>
<item xsi:null="1"/>
<item xsi:type="xsd:string">5</item>
<item xsi:type="xsd:string"/>
<item xsi:type="xsd:string">7</item>
</soapenc:Array>
</namesp23:object_infoResponse>
</soap:Body>
</soap:Envelope>
')->result;

  ok(scalar @$deserialized == 7);
  ok(! defined $deserialized->[2]);
  ok(! defined $deserialized->[3]);
  ok($deserialized->[5] eq '');
}

{
  print "Serialization of list with undef elements test(s)...\n";

  $serialized = SOAP::Serializer->method(a => undef, 1, undef, 2);
  my(@r) = SOAP::Deserializer->deserialize($serialized)->paramsall;

  ok(2 == grep {!defined} @r);
}

{
  print "Deserialization with xsi:type='string' test(s)...\n";

  $a = 'SOAP::Lite';
  $deserialized = SOAP::Deserializer->deserialize(qq!<inputString xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="string" xmlns="http://schemas.xmlsoap.org/soap/encoding/">$a</inputString>!)->root;

  ok($deserialized eq $a);
}

{ 
  print "Deserialization with typing inherited from Array element test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<soapenc:Array xsi:type="soapenc:Array" soapenc:arrayType="soapenc:base64[]" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsd="http://www.w3.org/1999/XMLSchema" soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
<item xsi:type="xsd:string">MTIz</item>
<item>MTIz</item>
<item xsi:type="xsd:string"/>
</soapenc:Array>')->root;

  ok(scalar @$deserialized == 3);
  ok($deserialized->[0] eq 'MTIz');
  ok($deserialized->[1] eq 123);
  ok($deserialized->[2] eq '');
}

{
  print "Serialization with explicit typing test(s)...\n";

  $serialized = SOAP::Serializer
    ->method(a => SOAP::Data->name('return')->type(int => 1));
  ok($serialized =~ /xsd:int/);

  eval {
    $serialized = SOAP::Serializer
      ->method(a => SOAP::Data->name('return')->type(noint => 1));
  };
  ok($@ =~ /for type 'noint' is not specified/);
}
{
  print "Serialization w/out explicit typing test(s)...\n";

  $a = { a => 'false' };
  $serialized = SOAP::Serializer->namespaces({})->serialize($a);

  ### 'false' evaluated as a boolean should still be false after the evaluation.
  ok($serialized =~ m!<c-gensym(\d+)><a xsi:type="xsd:boolean">false</a></c-gensym\1>!);

  $a = { a => 'true' };
  $serialized = SOAP::Serializer->namespaces({})->serialize($a);

  ### 'false' evaluated as a boolean should still be false after the evaluation.
  ok($serialized =~ m!<c-gensym(\d+)><a xsi:type="xsd:boolean">true</a></c-gensym\1>!);

}
{
  print "Serialization with explicit namespaces test(s)...\n";

  $serialized = SOAP::Serializer->serialize(SOAP::Data->name('b' => 1));
  ok($serialized =~ m!<b !);

  $serialized = SOAP::Serializer->serialize(SOAP::Data->name('c:b' => 1));
  ok($serialized =~ m!<c:b !);

  $serialized = SOAP::Serializer->serialize(SOAP::Data->name('{a}b' => 1));
  ok($serialized =~ m!<namesp\d+:b ! && $serialized =~ m!xmlns:namesp\d+="a"!);

  $serialized = SOAP::Serializer->serialize(SOAP::Data->name('{}b' => 1));
  ok($serialized =~ m!<b ! && $serialized =~ m!xmlns=""!);

  my @prefix_uri_tests = (
    # prefix,   uri,  test
    [ undef,  undef,  '<b>1</b>' ],
    [ undef,     '',  '<b xmlns="">1</b>' ],
    [ undef,    'a',  '<(namesp\d+):b xmlns:\1="a">1</\1:b>' ],
    [    '',  undef,  '<b>1</b>' ],           
    [    '',     '',  '<b xmlns="">1</b>' ],  
    [    '',    'a',  '<b xmlns="a">1</b>' ], 
    [   'c',  undef,  '<c:b>1</c:b>' ],       
    [   'c',     '',  '<b xmlns="">1</b>' ],  
    [   'c',    'a',  '<c:b xmlns:c="a">1</c:b>' ],
  );

  my $serializer = SOAP::Serializer->autotype(0)->namespaces({});
  my $deserializer = SOAP::Deserializer->new;
  my $testnum = 0;
  foreach (@prefix_uri_tests) {
    $testnum++;
    my($prefix, $uri, $test) = @$_;
    my $res = $serializer->serialize(
      SOAP::Data->name('b')->prefix($prefix)->uri($uri)->value(1)
    );
    ok($res =~ /$test/);
    next unless $testnum =~ /^([4569])$/;

    my $data = $deserializer->deserialize($res)->dataof(SOAP::SOM::root);
    ok(defined $prefix ? defined $data->prefix && $data->prefix eq $prefix
                       : !defined $data->prefix);
    ok(defined $uri ? defined $data->uri && $data->uri eq $uri
                    : !defined $data->uri);
  }
}

{
  print "Deserialization for different SOAP versions test(s)...\n";

  my $version = SOAP::Lite->soapversion;

  $a = q!<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope
  xmlns:soapenc="http://www.w3.org/2003/05/soap-encoding"
  soap:encodingStyle="http://www.w3.org/2003/05/soap-encoding"
  xmlns:soap="http://www.w3.org/2003/05/soap-envelope"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema">
<soap:Body>
<namesp9:echoIntegerArray xmlns:namesp9="http://soapinterop.org/">
<inputIntegerArray soapenc:arrayType="xsd:int[3]" xsi:type="soapenc:Array">
<item xsi:type="xsd:int">1</item>
<item xsi:type="xsd:int">3</item>
<item xsi:type="xsd:int">5</item>
</inputIntegerArray>
</namesp9:echoIntegerArray>
</soap:Body>
</soap:Envelope>!;

  SOAP::Lite->soapversion(1.1);
  $deserialized = SOAP::Deserializer->deserialize($a);
  ok(ref $deserialized->result eq 'ARRAY');

  SOAP::Lite->soapversion(1.2);
  $deserialized = SOAP::Deserializer->deserialize($a);
  ok(ref $deserialized->result eq 'ARRAY');

  SOAP::Lite->soapversion($version);
}

{
  print "Deserialization of multidimensional array of array test(s)...\n";

  $a = q!<?xml version="1.0" encoding="UTF-8"?>
<S:Envelope S:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/' 
   xmlns:S='http://schemas.xmlsoap.org/soap/envelope/'
   xmlns:E='http://schemas.xmlsoap.org/soap/encoding/'
   xmlns:a='http://foo.bar.org/'
   xmlns:b='http://www.w3.org/2001/XMLSchema'
   xmlns:c='http://www.w3.org/2001/XMLSchema-instance'>
<S:Body><a:SomeMethod>
<nums E:arrayType='b:anyType[2,2]'>
<i E:arrayType='b:anyType[3]'>
<i c:type='b:short'>1</i><i c:type='b:short'>2</i><i c:type='b:short'>3</i>
</i>
<i E:arrayType='b:anyType[3]'>
<i c:type='b:short'>4</i><i c:type='b:short'>5</i><i c:type='b:short'>6</i>
</i>
<i E:arrayType='b:anyType[3]'>
<i c:type='b:short'>7</i><i c:type='b:short'>8</i><i c:type='b:short'>9</i>
</i>
<i E:arrayType='b:anyType[3]'>
<i c:type='b:short'>10</i><i c:type='b:short'>11</i><i c:type='b:short'>12</i>
</i>
</nums></a:SomeMethod></S:Body></S:Envelope>!;

  $deserialized = SOAP::Deserializer->deserialize($a)->result;

  # [
  #   [
  #     ['1', '2', '3'],
  #     ['4', '5', '6']
  #   ],
  #   [
  #     ['7', '8', '9'],
  #     ['10', '11', '12']
  #   ]
  # ]

  ok(ref $deserialized eq 'ARRAY');
  ok(@$deserialized == 2);
  ok(@{$deserialized->[0]} == 2);
  ok(@{$deserialized->[0]->[0]} == 3);
  ok($deserialized->[0]->[0]->[2] == 3);
}

{
  print "Serialization without specified typemapping test(s)...\n";

  $serialized = SOAP::Serializer->method(a => bless {a => 1} => 'A');
  ok($serialized =~ m!<A xsi:type="namesp\d+:A">!);
  ok($serialized =~ m!^<\?xml!); # xml declaration 

  # higly questionably, but that's how it is
  $serialized = SOAP::Serializer->encoding(undef)->method(a => bless {a => 1} => 'A');
  ok($serialized =~ m!<A(?: xsi:type="namesp\d+:A"| xmlns:namesp\d+="http://namespaces.soaplite.com/perl")>!);
  ok($serialized !~ m!^<\?xml!); # no xml declaration 
}

{
  print "Deserialization with different XML Schemas on one element test(s)...\n";

  my $deserializer = SOAP::Deserializer->new;
  $deserializer->deserialize(q!<soap:Envelope
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi1="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsi0="http://www.w3.org/2000/10/XMLSchema-instance"
    xmlns:xsi9="http://www.w3.org/1999/XMLSchema-instance"
    xmlns:xsd9="http://www.w3.org/1999/XMLSchema"
    xmlns:xsd1="http://www.w3.org/2001/XMLSchema"
    xmlns:xsd0="http://www.w3.org/2000/10/XMLSchema" >
  <soap:Body>
    <ns0:echoString xmlns:ns0="http://soapinterop.org/" >
      <inputString xsi0:type="xsd0:string" xsi1:type="xsd1:string"
xsi9:type="xsd9:string">Simple Test String</inputString>
    </ns0:echoString>
  </soap:Body>
</soap:Envelope>!);

  ok($deserializer->xmlschema eq 'http://www.w3.org/1999/XMLSchema');

  $deserializer->deserialize(q!<soap:Envelope
    soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi1="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsi0="http://www.w3.org/2000/10/XMLSchema-instance"
    xmlns:xsi9="http://www.w3.org/1999/XMLSchema-instance"
    xmlns:xsd9="http://www.w3.org/1999/XMLSchema"
    xmlns:xsd1="http://www.w3.org/2001/XMLSchema"
    xmlns:xsd0="http://www.w3.org/2000/10/XMLSchema" >
  <soap:Body>
    <ns0:echoString xmlns:ns0="http://soapinterop.org/" >
      <inputString xsi0:type="xsd1:string" xsi1:type="xsd1:string"
xsi9:type="xsd1:string">Simple Test String</inputString>
    </ns0:echoString>
  </soap:Body>
</soap:Envelope>!);

  ok($deserializer->xmlschema eq 'http://www.w3.org/2001/XMLSchema');
}

{
  print "SOAP::Fault stringification test(s)...\n";

  my $f = SOAP::Fault->faultcode('Client.Authenticate')
                     ->faultstring('Bad error');
  ok($f eq 'Client.Authenticate: Bad error');
}

{
  print "Memory leaks test(s)...\n"; # also check 36-leaks.t

  my %calls;
  {
    SOAP::Lite->import(trace => [objects => sub { 
      if ((caller(2))[3] =~ /^(.+)::(.+)$/) {
        $calls{$2}{$1}++;
      }
    }]);

    my $soap = SOAP::Lite
      -> uri("Echo")
      -> proxy("http://services.soaplite.com/echo.cgi");
  }
  foreach (keys %{$calls{new}}) {
    ok(exists $calls{DESTROY}{$_});
  }

  %calls = ();
  {
    local $SOAP::Constants::DO_NOT_USE_XML_PARSER = 1;
    my $soap = SOAP::Lite
      -> uri("Echo")
      -> proxy("http://services.soaplite.com/echo.cgi");
  }
  foreach (keys %{$calls{new}}) {
    ok(exists $calls{DESTROY}{$_});
  }

  SOAP::Lite->import(trace => '-objects');
}
