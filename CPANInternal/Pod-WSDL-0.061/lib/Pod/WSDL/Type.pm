package Pod::WSDL::Type;

use strict;
use warnings;
use Pod::WSDL::Attr;
use Pod::WSDL::Utils qw(:writexml :namespaces :types);
use Pod::WSDL::AUTOLOAD;

our $VERSION = "0.05";
our @ISA = qw/Pod::WSDL::AUTOLOAD/;

our %FORBIDDEN_METHODS = (
	name     => {get => 1, set =>  0},
	wsdlName => {get => 1, set =>  0},
	array    => {get => 1, set =>  1},
	descr    => {get => 1, set =>  0},
	attrs    => {get => 1, set =>  0},
	writer   => {get => 0, set =>  0},
);

sub new {
	my ($pkg, %data) = @_;
	
	die "A type needs a name, died" unless $data{name};

	my $wsdlName = $data{name};
	$wsdlName =~ s/(?:^|::)(.)/uc $1/eg;
	
	my $me = bless {
		_name     => $data{name},
		_wsdlName => ucfirst $wsdlName,
		_array    => $data{array} || 0,
		_attrs    => [],
		_descr    => $data{descr} || '',
		_writer   => $data{writer},
		_reftype  => 'HASH',
	}, $pkg;

	$me->_initPod($data{pod}) if $data{pod};

	return $me;	
}

sub _initPod {
	my $me  = shift;
	my $pod = shift;
	
	my @data = split "\n", $pod;
	
	# Preprocess wsdl pod: trim all lines and concatenate lines not
	# beginning with wsdl type tokens to previous line.
	# Ignore first element, if it does not begin with wsdl type token.
	for (my $i = $#data; $i >= 0; $i--) {
		
		if ($data[$i] !~ /^\s*(?:_ATTR|_REFTYPE)/i) {
			if ($i > 0) {
				$data[$i - 1] .= " $data[$i]";
				$data[$i] = '';
			}
		}
	}

	for (@data) {
		s/\s+/ /g;
		s/^ //;
		s/ $//;

		if (/^\s*_ATTR\s+/i) {
			push @{$me->{_attrs}}, new Pod::WSDL::Attr($_);
		} elsif (/^\s*_REFTYPE\s+(HASH|ARRAY)/i) {
			$me->reftype(uc $1);
		}
	}
		
}

sub writeComplexType {
	my $me = shift;
	my $ownTypes = shift;

	$me->writer->wrElem($START_PREFIX_NAME, "complexType",  name => $me->wsdlName);
	$me->writer->wrDoc($me->descr, useAnnotation => 1);
	
	if ($me->reftype eq 'HASH') {
		
		$me->writer->wrElem($START_PREFIX_NAME, "sequence");
	
		for my $attr (@{$me->attrs}) {
			my %tmpArgs = (name => $attr->name, 
				type => Pod::WSDL::Utils::getTypeDescr($attr->type, $attr->array, $ownTypes->{$attr->type}));
			
			$tmpArgs{nillable} = $attr->nillable if $attr->nillable;
			
			$me->writer->wrElem($START_PREFIX_NAME, "element", %tmpArgs);
			$me->writer->wrDoc($attr->descr, useAnnotation => 1);
			$me->writer->wrElem($END_PREFIX_NAME, "element");
		}
	
		$me->writer->wrElem($END_PREFIX_NAME, "sequence");
	} elsif ($me->reftype eq 'ARRAY') {
		$me->writer->wrElem($START_PREFIX_NAME, "complexContent");
		$me->writer->wrElem($START_PREFIX_NAME, "restriction",  base => "soapenc:Array");
		$me->writer->wrElem($EMPTY_PREFIX_NAME, "attribute",  ref => $TARGET_NS_DECL . ':' . $me->wsdlName, "wsdl:arrayType" => 'xsd:anyType[]');
		$me->writer->wrElem($END_PREFIX_NAME, "restriction");
		$me->writer->wrElem($END_PREFIX_NAME, "complexContent");
	}
	
	$me->writer->wrElem($END_PREFIX_NAME, "complexType");

	if ($me->array) {
		$me->writer->wrElem($START_PREFIX_NAME, "complexType",  name => $ARRAY_PREFIX_NAME . ucfirst $me->wsdlName);
		$me->writer->wrElem($START_PREFIX_NAME, "complexContent");
		$me->writer->wrElem($START_PREFIX_NAME, "restriction",  base => "soapenc:Array");
		$me->writer->wrElem($EMPTY_PREFIX_NAME, "attribute",  ref => "soapenc:arrayType", "wsdl:arrayType" => $TARGET_NS_DECL . ':' . $me->wsdlName . '[]');
		$me->writer->wrElem($END_PREFIX_NAME, "restriction");
		$me->writer->wrElem($END_PREFIX_NAME, "complexContent");
		$me->writer->wrElem($END_PREFIX_NAME, "complexType");
	}
}

1;
__END__

=head1 NAME

Pod::WSDL::Type - Represents a type in Pod::WSDL (internal use only)

=head1 SYNOPSIS

  use Pod::WSDL::Type;
  my $type = new Pod::WSDL::Param(name => 'My::Foo', array => 0, descr => 'My foo bars');

=head1 DESCRIPTION

This module is used internally by Pod::WSDL. It is unlikely that you have to interact directly with it. If that is the case, take a look at the code, it is rather simple.

=head1 METHODS

=head2 new

Instantiates a new Pod::WSDL::Type.

=head3 Parameters

=over 4

=item

name - name of the type, something like 'string', 'boolean', 'My::Foo' etc.

=item

array - if true, an array of the type is used (defaults to 0)

=item

descr - description of the type

=item

pod - the wsdl pod of the type. Please see the section "Pod Syntax" in the description of Pod::WSDL.

=back

=head2 writeComplexType

Write complex type element for XML output. Takes one parameter: ownTypes, reference to hash with own type information

=head1 EXTERNAL DEPENDENCIES

  [none]

=head1 EXAMPLES

see Pod::WSDL

=head1 BUGS

see Pod::WSDL

=head1 TODO

see Pod::WSDL

=head1 SEE ALSO

  Pod::WSDL :-)
 
=head1 AUTHOR

Tarek Ahmed, E<lt>bloerch -the character every email address contains- oelbsk.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by Tarek Ahmed

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

=cut
