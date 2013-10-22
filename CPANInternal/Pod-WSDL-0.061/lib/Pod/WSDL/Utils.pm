package Pod::WSDL::Utils;

use strict;
use warnings;
require Exporter;

our @ISA = qw(Exporter);
our %EXPORT_TAGS = (
	writexml   => [qw($END_PREFIX_NAME $START_PREFIX_NAME $EMPTY_PREFIX_NAME)],
	namespaces => [qw(%BASIC_NAMESPACES $DEFAULT_NS_DECL $TARGET_NS_DECL $IMPL_NS_DECL)],
	messages   => [qw($REQUEST_SUFFIX_NAME $RESPONSE_SUFFIX_NAME $RETURN_SUFFIX_NAME $EMPTY_MESSAGE_NAME $DOCUMENT_STYLE $RPC_STYLE $LITERAL_USE $ENCODED_USE $PART_IN $FAULT_NAME $MESSAGE_PART)],
	types      => [qw($ARRAY_PREFIX_NAME %XSD_STANDARD_TYPE_MAP)],
);

our @EXPORT_OK = (@{$EXPORT_TAGS{writexml}}, @{$EXPORT_TAGS{namespaces}}, @{$EXPORT_TAGS{messages}}, @{$EXPORT_TAGS{types}});
our $VERSION = "0.05";

# writexml
our $END_PREFIX_NAME       = 'end';
our $START_PREFIX_NAME     = 'start';
our $EMPTY_PREFIX_NAME     = 'empty';

# namespaces
our %BASIC_NAMESPACES = qw (
	soapenc    http://schemas.xmlsoap.org/soap/encoding/
	wsdl       http://schemas.xmlsoap.org/wsdl/
	wsdlsoap   http://schemas.xmlsoap.org/wsdl/soap/
	xsd        http://www.w3.org/2001/XMLSchema
);

our $DEFAULT_NS_DECL       = 'podwsdl';
our $TARGET_NS_DECL        = 'tns1';
our $IMPL_NS_DECL          = 'impl';

# messages
our $REQUEST_SUFFIX_NAME   = 'Request';
our $RESPONSE_SUFFIX_NAME  = 'Response';
our $RETURN_SUFFIX_NAME    = 'Return';
our $EMPTY_MESSAGE_NAME    = 'empty';
our $FAULT_NAME            = 'fault';
our $DOCUMENT_STYLE        = 'document';
our $RPC_STYLE             = 'rpc';
our $LITERAL_USE           = 'literal';
our $ENCODED_USE           = 'encoded';
our $PART_IN               = 'PartIn';
our $MESSAGE_PART          = 'MessagePart';

# types
our $ARRAY_PREFIX_NAME     = 'ArrayOf';
our %XSD_STANDARD_TYPE_MAP;

# see http://www.w3.org/TR/2004/REC-xmlschema-2-20041028/
$XSD_STANDARD_TYPE_MAP{$_} = 1 for qw(
	anyType
	anySimpleType

	string
	normalizedString
	token
	anyURI  
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

sub getTypeDescr {
	my $typeName = shift;
	my $array    = shift;
	my $ownType  = shift;
	
	if ((defined $typeName) and (exists $XSD_STANDARD_TYPE_MAP{$typeName})) {
		if ($array) {
			return $TARGET_NS_DECL . ':' . $ARRAY_PREFIX_NAME . ucfirst $typeName;
		} else {
			return 'xsd:' . $typeName;
		}
	} elsif (defined $ownType) {
		return $TARGET_NS_DECL . ':' . ($array ? $ARRAY_PREFIX_NAME . ucfirst $ownType->wsdlName : $ownType->wsdlName);
	} else {
    return undef;
  }
}

1;
__END__

=head1 NAME

Pod::WSDL::Utils - Utilities and constants for Pod::WSDL (internal use only)

=head1 DESCRIPTION

This module is used internally by Pod::WSDL. It is unlikely that you have to interact directly with it. If that is the case, take a look at the code, it is rather simple.

=head1 METHODS

=head2 getTypeDescr

Returns a type description for type attributes in the wsdl document

=head1 EXTERNAL DEPENDENCIES

  [none]

=head1 EXAMPLES

see Pod::WSDL

=head1 BUGS

see Pod::WSDL

=head1 TODO

see Pod::WSDL

=head1 SEE ALSO

  Pod::WSDL ;-)
 
=head1 AUTHOR

Tarek Ahmed, E<lt>bloerch -the character every email address contains- oelbsk.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by Tarek Ahmed

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

=cut
