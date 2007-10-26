package Pod::WSDL::Method;

use strict;
use warnings;
use Pod::WSDL::Param;
use Pod::WSDL::Fault;
use Pod::WSDL::Return;
use Pod::WSDL::Doc;
use Pod::WSDL::Writer;
use Pod::WSDL::Utils qw(:writexml :namespaces :messages);
use Pod::WSDL::AUTOLOAD;

our $VERSION = "0.05";
our @ISA = qw/Pod::WSDL::AUTOLOAD/;

our $EMPTY_MESSAGE_NAME    = 'empty';
our $REQUEST_SUFFIX_NAME   = 'Request';
our $RESPONSE_SUFFIX_NAME  = 'Response';
our $RETURN_SUFFIX_NAME    = 'Return';
our $TARGET_NS_DECL        = 'tns1';

our %FORBIDDEN_METHODS = (
	name     => {get => 1, set =>  0},
	params   => {get => 1, set =>  0},
	doc      => {get => 1, set =>  1},
	return   => {get => 1, set =>  1},
	faults   => {get => 1, set =>  0},
	oneway   => {get => 1, set =>  1},
	writer   => {get => 0, set =>  0},
);

sub new {
	my ($pkg, %data) = @_;
	
	die "A method needs a name, died"   unless defined $data{name};
	die "A method needs a writer, died" unless defined $data{writer} and ref $data{writer} eq 'Pod::WSDL::Writer';
	
	bless {
		_name                => $data{name},
		_params              => $data{params} || [],
		_return              => $data{return},
		_doc                 => $data{doc} || new Pod::WSDL::Doc('_DOC'),
		_faults              => $data{faults} || [],
		_oneway              => $data{oneWay} || 0,
		_writer              => $data{writer},
		_emptyMessageWritten => 0,
	}, $pkg;
}

sub addParam {
	push @{$_[0]->{_params}}, $_[1] if defined $_[1];
}

sub addFault {
	push @{$_[0]->{_faults}}, $_[1] if defined $_[1];
}

sub requestName {
	return $_[0]->name . $REQUEST_SUFFIX_NAME;
}

sub responseName {
	return $_[0]->name . $RESPONSE_SUFFIX_NAME;
}

sub writeMessages {
	my $me      = shift;
	my $types   = shift;
	my $style   = shift;
	my $wrapped = shift;
	
	$me->_writeMessageRequestElem($types, $style, $wrapped);
	$me->writer->wrNewLine;

	unless ($me->oneway) {
		if ($me->return) {
			$me->_writeMessageResponseElem($types, $style, $wrapped);
			$me->writer->wrNewLine;
		} else {
			unless ($me->writer->emptyMessageWritten) {
				$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:message', name => $EMPTY_MESSAGE_NAME);
				$me->writer->registerWrittenEmptyMessage;
				$me->writer->wrNewLine;
			}
		}
	}
	
	for my $fault (@{$me->faults}) {
		next if $me->writer->faultMessageWritten($fault->wsdlName);
		
		$me->_writeMessageFaultElem($fault->wsdlName, $style, $wrapped);
		$me->writer->registerWrittenFaultMessage($fault->wsdlName);
		$me->writer->wrNewLine;
	}
}

sub writePortTypeOperation {
	my $me = shift;
	
	my $name = $me->name;
	my $paramOrder = '';
	
	for my $param (@{$me->params}) {
		$paramOrder .= $param->name . ' ';
	}

	$paramOrder =~ s/\s+$//;
	
	my $inputName  = $name . $REQUEST_SUFFIX_NAME;
	my $outputName = $name . $RESPONSE_SUFFIX_NAME;

	$me->writer->wrElem($START_PREFIX_NAME, 'wsdl:operation', name => $name, parameterOrder => ($paramOrder ? $paramOrder : ""));
	$me->writer->wrDoc($me->doc->descr);
	$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:input', message => "$IMPL_NS_DECL:$inputName", name => $inputName);
	
	# if method has no return, we treat it as one-way operation
	unless ($me->oneway) {
		if ($me->return) {
			$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:output', message => "$IMPL_NS_DECL:$outputName", name => $outputName);
		} else {
			$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:output', message => "$IMPL_NS_DECL:$EMPTY_MESSAGE_NAME");
		}
	}

	my $elemType;

	# write methods faults
	for my $fault (@{$me->faults}) {
		
		# if we want documentation and have some documentation ...
		if ($fault->descr and $me->writer->withDocumentation) {
			$elemType = $START_PREFIX_NAME;
		} else {
			$elemType = $EMPTY_PREFIX_NAME;
		}
		
		$me->writer->wrElem($elemType, "wsdl:fault", message => "$IMPL_NS_DECL:" . $fault->wsdlName, name => $fault->wsdlName);
		
		# only, if with documentation
		if ($elemType eq $START_PREFIX_NAME) {
			$me->writer->wrDoc($fault->descr);
			$me->writer->wrElem($END_PREFIX_NAME, "wsdl:fault");
		}
	}

	$me->writer->wrElem($END_PREFIX_NAME, 'wsdl:operation');
}

sub _writeMessageRequestElem {
	my $me      = shift;
	my $types   = shift;
	my $style   = shift;
	my $wrapped = shift;

	$me->writer->wrElem($START_PREFIX_NAME, 'wsdl:message', name => $me->requestName);
	
	if ($wrapped) {
		$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:part', name => 'parameters', element => $me->requestName);
	} else {
		for my $param (@{$me->params}) {
			$me->_writePartElem($param->name, $param->type, $param->array, $param->descr, $style, 0, $types->{$param->type}) if $param->paramType =~ /^(INOUT|OUT|IN)$/;
		}
	}
	
	$me->writer->wrElem($END_PREFIX_NAME, 'wsdl:message');
}

sub _writeMessageResponseElem {
	my $me      = shift;
	my $types   = shift;
	my $style   = shift;
	my $wrapped = shift;

	$me->writer->wrElem($START_PREFIX_NAME, 'wsdl:message', name => $me->responseName);

	if ($wrapped) {
		$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:part', name => 'parameters', element => $me->responseName);
	} else {
		for my $param (@{$me->params}) {
			$me->_writePartElem($param->name, $param->type, $param->array, $param->descr, $style, 0, $types->{$param->type}) if $param->paramType =~ /^(INOUT|OUT)?$/;
		}
	
		if (defined $me->return) {
			$me->_writePartElem($me->name . $RETURN_SUFFIX_NAME, $me->return->type, $me->return->array, $me->return->descr, $style, 1, $types->{$me->return->type});
		}
	}
	
	$me->writer->wrElem($END_PREFIX_NAME, 'wsdl:message');
}

sub _writeMessageFaultElem {
	my $me      = shift;
	my $name    = shift;
	my $style   = shift;
	my $wrapped = shift;

	my %attrs = (name => $FAULT_NAME);
	
	if ($style eq $RPC_STYLE) {
		$attrs{type} = "$TARGET_NS_DECL:$name";
	} elsif ($style eq $DOCUMENT_STYLE) {
		$attrs{element} = $name . $MESSAGE_PART;
	}

	$me->writer->wrElem($START_PREFIX_NAME, 'wsdl:message', name => $name);
	$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:part', %attrs);
	$me->writer->wrElem($END_PREFIX_NAME, 'wsdl:message');
}

sub _writePartElem {
	my $me       = shift;
	my $name     = shift;
	my $type     = shift;
	my $array    = shift;
	my $descr    = shift;
	my $style    = shift;
	my $isReturn = shift;
	my $ownType  = shift;

	my %attrs = (name => $name);
	
	if ($style eq $RPC_STYLE) {
		$attrs{type} = Pod::WSDL::Utils::getTypeDescr($type, $array, $ownType);
	} elsif ($style eq $DOCUMENT_STYLE) {
		$attrs{element} = ($isReturn ? lcfirst $RETURN_SUFFIX_NAME : $name) . $PART_IN . ucfirst $me->requestName
	}
	
	if ($descr and $me->writer->withDocumentation) {
		$me->writer->wrElem($START_PREFIX_NAME, 'wsdl:part', %attrs);
		$me->writer->wrDoc($descr);
		$me->writer->wrElem($END_PREFIX_NAME, 'wsdl:part');
		
	} else {
		$me->writer->wrElem($EMPTY_PREFIX_NAME, 'wsdl:part', %attrs);
	}
}

sub writeBindingOperation {
	my $me       = shift;
	my $location = shift;
	my $use      = shift;

	$me->writer->wrElem($START_PREFIX_NAME, "wsdl:operation", name => $me->name);
	$me->writer->wrElem($EMPTY_PREFIX_NAME, "wsdlsoap:operation", soapAction => "");
	$me->writer->wrElem($START_PREFIX_NAME, "wsdl:input", name => $me->requestName);
	$me->writer->wrElem($EMPTY_PREFIX_NAME, "wsdlsoap:body", encodingStyle => "http://schemas.xmlsoap.org/soap/encoding/", namespace => $location, use => $use);
	$me->writer->wrElem($END_PREFIX_NAME, "wsdl:input");
	
	unless ($me->oneway) {
		$me->writer->wrElem($START_PREFIX_NAME, "wsdl:output", name => $me->return ? $me->responseName : $EMPTY_MESSAGE_NAME);
		$me->writer->wrElem($EMPTY_PREFIX_NAME, "wsdlsoap:body", encodingStyle => "http://schemas.xmlsoap.org/soap/encoding/", namespace => $location, use => $use);
		$me->writer->wrElem($END_PREFIX_NAME, "wsdl:output");
	}
			
	for my $fault (@{$me->faults}) {
		$me->writer->wrElem($START_PREFIX_NAME, "wsdl:fault", name => $fault->wsdlName);
		$me->writer->wrElem($EMPTY_PREFIX_NAME, "wsdlsoap:fault", name => $fault->wsdlName, encodingStyle => "http://schemas.xmlsoap.org/soap/encoding/", namespace => $location, use => $use);
		$me->writer->wrElem($END_PREFIX_NAME, "wsdl:fault");
	}

	$me->writer->wrElem($END_PREFIX_NAME, "wsdl:operation");
}

sub writeDocumentStyleSchemaElements {
	my $me    = shift;
	my $types = shift;
	
	for my $param (@{$me->params}) {
		$me->writer->wrElem($EMPTY_PREFIX_NAME, 'element', 
			name => $param->name . $PART_IN . ucfirst $me->requestName,
			type => Pod::WSDL::Utils::getTypeDescr($param->type, $param->array, $types->{$param->type}));
	}

	for my $fault (@{$me->faults}) {
		next if $me->writer->faultMessageWritten($fault->wsdlName . $MESSAGE_PART);
		
		$me->writer->registerWrittenFaultMessage($fault->wsdlName . $MESSAGE_PART);

		$me->writer->wrElem($EMPTY_PREFIX_NAME, 'element', 
			name => $fault->wsdlName . $MESSAGE_PART,
			type => Pod::WSDL::Utils::getTypeDescr($fault->type, 0, $types->{$fault->type}));
	}

	if (!$me->oneway and $me->return) {
		$me->writer->wrElem($EMPTY_PREFIX_NAME, 'element', 
			name => lcfirst $RETURN_SUFFIX_NAME . $PART_IN . ucfirst $me->requestName,
			type => Pod::WSDL::Utils::getTypeDescr($me->return->type, $me->return->array, $types->{$me->return->type}));
	}
}
1;
__END__

=head1 NAME

Pod::WSDL::Method - Represents a method in Pod::WSDL (internal use only)

=head1 SYNOPSIS

  use Pod::WSDL::Method;
  my $m = new Pod::WSDL::Method(name => 'mySub', writer => 'myWriter', doc => new Pod::WSDL::Doc($docStr), return => new Pod::WSDL::Return($retStr));

=head1 DESCRIPTION

This module is used internally by Pod::WSDL. It is unlikely that you have to interact directly with it. If that is the case, take a look at the code, it is rather simple.

=head1 METHODS

=head2 new

Instantiates a new Pod::WSDL::Method.

=head2 Parameters

=over 4

=item

name - name of the method, mandatory

=item

doc - a Pod::WSDL::Doc object, can be ommitted, use method doc later

=item

return - a Pod::WSDL::Return object, can be ommitted, use method return later

=item

params - ref to array of Pod::WSDL::Param objects, can be ommitted, use addParam() later

=item

faults - ref to array of Pod::WSDL::Fault objects, can be ommitted, use addFault() later

=item

oneway - if true, method is a one way operation

=item

writer - XML::Writer-Object for output, mandatory

=back

=head2 addParam

Add a Pod::WSDL::Param object to Pod::WSDL::Method

=head2 addFault

Add a Pod::WSDL::Fault object to Pod::WSDL::Method

=head2 return

Get or Set the Pod::WSDL::Return object for Pod::WSDL::Method

=head2 doc

Get or Set the Pod::WSDL::Doc object for Pod::WSDL::Method

=head2 requestName

Get name for request in XML output

=head2 responseName

Get name for response in XML output

=head2 writeBindingOperation

Write operation child for binding element in XML output

=head2 writeMessages

Write message elements in XML output

=head2 writePortTypeOperation

Write operation child for porttype element in XML output

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
