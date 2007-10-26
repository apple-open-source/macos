package Pod::WSDL::Attr;
use strict;
use warnings;
use Pod::WSDL::AUTOLOAD;

our $VERSION = "0.05";
our @ISA = qw/Pod::WSDL::AUTOLOAD/;

our %FORBIDDEN_METHODS = (
	name      => {get => 1, set =>  0},
	type      => {get => 1, set =>  0},
	nillable  => {get => 1, set =>  0},
	descr     => {get => 1, set =>  0},
	array     => {get => 1, set =>  0},
);

sub new {
	my ($pkg, $str) = @_;

	defined $str or $str = ''; # avoids warnings
	$str =~ s/\s*_ATTR\s*//i or die "Input string '$str' does not begin with '_ATTR'";
	my ($name, $type, $needed, $descr) = split /\s+/, $str, 4;

	$descr ||= '';
	
	if ((uc $needed) ne '_NEEDED') {
		$descr  = "$needed $descr";
		$needed = 0;
	} else {
		$needed = 1;
	}
	
	$type =~ /([\$\@])(.*)/;
	die "Type '$type' must be prefixed with either '\$' or '\@', died" unless $1;
	
	bless {
		_name     => $name,
		_type     => $2,
		_nillable => $needed ? undef : 'true',
		_descr    => $descr,
		_array    => $1 eq '@' ? 1 : 0,
	}, $pkg;
}

1;
__END__

=head1 NAME

Pod::WSDL::Attr - Represents the WSDL pod for an attribute of a class (internal use only)

=head1 SYNOPSIS

  use Pod::WSDL::Attr;
  my $attr = new Pod::WSDL::Attr('_ATTR $string _NEEDED This attribute is for blah ...');

=head1 DESCRIPTION

This module is used internally by Pod::WSDL. It is unlikely that you have to interact directly with it. If that is the case, take a look at the code, it is rather simple.

=head1 METHODS

=head2 new

Instantiates a new Pod::WSDL::Attr. The method needs one parameter, the attribute string from the pod. Please see SYNOPSIS or the section "Pod Syntax" in the description of Pod::WSDL.

=head1 EXTERNAL DEPENDENCIES

  [none]

=head1 EXAMPLES

see Pod::WSDL

=head1 BUGS

see Pod::WSDL

=head1 TODO

see Pod::WSDL

=head1 SEE ALSO

  Pod::WSDL
 
=head1 AUTHOR

Tarek Ahmed, E<lt>bloerch -the character every email address contains- oelbsk.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 by Tarek Ahmed

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

=cut
