#!/usr/bin/perl -w
package Pod::WSDL;
use Test::More tests => 54;
BEGIN {use_ok('Pod::WSDL');}
use lib length $0 > 10 ? substr $0, 0, length($0) - 16 : '.';
use strict;
use warnings;
use XML::XPath;

my $tmpXP;

# test standardtypes
my @xsdTypes = qw(
	anyType
	anySimpleType
	string
	normalizedString
	token
	anyUri  
	language
	Name
	QName
	NCName
	boolean
	float
	double
	decimal
	int
	positiveInteger
	nonPositiveInteger
	negativeInteger
	nonNegativeInteger
	long
	short
	byte
	unsignedInt
	unsignedLong
	unsignedShort
	unsignedByte
	duration
	dateTime
	time
	date
	gYearMonth
	gYear
	gMonthDay
	gDay
	gMonth
	hexBinary
	base64Binary
);

my $p = new Pod::WSDL(source => 'My::TypeTest',
	               location => 'http://localhost/My/TypeTest',
	               pretty => 1,
	               withDocumentation => 1);

my $xmlOutput = $p->WSDL;
my $xp = XML::XPath->new(xml => $xmlOutput);

my $foundMethod = 0;

for my $m (@{$p->methods}) {
	if ($m->name eq 'testXSDTypes') {
		$foundMethod = 1;
		for (0 .. @xsdTypes - 1) {
			ok($m->params->[$_]->type eq $xsdTypes[$_], "Recognized xsd type '$xsdTypes[$_]' on method 'testXSDTypes' correctly")
		}
	}
}

fail('Did not find method testXSDTypes in package My::TypeTest') unless $foundMethod;

# test own complex types
$foundMethod = 0;
for my $m (@{$p->methods}) {
	if ($m->name eq 'testComplexTypes') {
		$foundMethod = 1;
		ok($m->params->[0]->type eq 'My::Foo', "Recognized own complex type 'My::Foo' on method 'testComplexTypes' correctly")
	}
}

fail('Did not find method testComplexTypes in package My::TypeTest') unless $foundMethod;

ok($xp->exists('/wsdl:definitions/wsdl:types/schema[@targetNamespace = "http://localhost/My/TypeTest"]'), 'Found schema with targetNamespace "http://localhost/My/TypeTest" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/import[@namespace = "http://schemas.xmlsoap.org/soap/encoding/"]'), 'Found import of namespace "http://schemas.xmlsoap.org/soap/encoding/" in schema in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "MyFoo"]'), 'Found complex type MyFoo in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType/sequence/element[@name = "_bar" and @type = "xsd:negativeInteger"]'), 'Found element with name "_bar" and type xsd:negativeInteger in complex type MyFoo in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType/sequence/element/annotation/documentation[text() = "a bar"]'), 'Found documentation for element with name "_bar" in complex type MyFoo in xml output.');

# test array types
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfString"]'), 'Found array type ArrayOfString in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfString"]/complexContent'), 'ArrayOfString has complexContent child.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfString"]/complexContent/restriction[@base = "soapenc:Array"]'), 'complexContent has restriction child with base="soapenc:Array".');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfString"]/complexContent/restriction[@base = "soapenc:Array"]/attribute[@ref="soapenc:arrayType" and @wsdl:arrayType="soapenc:string[]"]'), 'restriction has attribute child with ref="soapenc:arrayType" and wsdl:arrayType="soapenc:string[].');

ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfMyFoo"]'), 'Found array type ArrayOfMyFoo in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfMyFoo"]/complexContent'), 'ArrayOfMyFoo has complexContent child.');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfMyFoo"]/complexContent/restriction[@base = "soapenc:Array"]'), 'complexContent has restriction child with base="soapenc:Array".');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name = "ArrayOfMyFoo"]/complexContent/restriction[@base = "soapenc:Array"]/attribute[@ref="soapenc:arrayType" and @wsdl:arrayType="tns1:MyFoo[]"]'), 'restriction has attribute child with ref="soapenc:arrayType" and wsdl:arrayType="tns1:MyFoo[].');

# test nillable attributes
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType/sequence/element[@name = "_boerk" and @type = "xsd:boolean" and @nillable = "true"]'), 'Found nillable element with name "_boerk" in complex type MyFoo in xml output.');

# test non existing types
eval {
$p = new Pod::WSDL(source => 'My::WrongTypeTest',
	               location => 'http://localhost/My/WrongTypeTest',
	               pretty => 1,
	               withDocumentation => 1);
};

ok($@ =~ /Can't find any file 'Non::Existent::Type' and can't locate it as a module in \@INC either \(\@INC contains/, 'Pod::WSDL dies on encountering unknown type');

__END__
This is just to help making tests ...

<wsdl:types>
    <schema targetNamespace="http://localhost/My/TypeTest" xmlns="http://www.w3.org/2001/XMLSchema">
        <import namespace="http://schemas.xmlsoap.org/soap/encoding/" />
        <complexType name="ArrayOfString">
                <complexContent>
                        <restriction base="soapenc:Array">
                                <attribute ref="soapenc:arrayType" wsdl:arrayType="soapenc:string[]" />
                        </restriction>
                </complexContent>
        </complexType>
        <complexType name="MyFoo">
                <sequence>
                        <element name="_bar" type="xsd:negativeInteger">
                                <wsdl:documentation>a bar</wsdl:documentation>
                        </element>
                        <element name="_boerk" nillable="true" type="xsd:boolean">
                                <wsdl:documentation>a nillable _boerk</wsdl:documentation>
                        </element>
                </sequence>
        </complexType>
        <complexType name="ArrayOfMyFoo">
                <complexContent>
                        <restriction base="soapenc:Array">
                                <attribute ref="soapenc:arrayType" wsdl:arrayType="tns1:MyFoo[]" />
                        </restriction>
                </complexContent>
        </complexType>
    </schema>
</wsdl:types>
