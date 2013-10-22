package Pod::WSDL::Return;
use strict;
use warnings;
use Pod::WSDL::AUTOLOAD;

our $VERSION = "0.05";
our @ISA = qw/Pod::WSDL::AUTOLOAD/;

our %FORBIDDEN_METHODS = (
	type  => {get => 1, set =>  0},
	array => {get => 1, set =>  0},
	descr => {get => 1, set =>  0},
);

sub new {
	my ($pkg, $str) = @_;

	defined $str or $str = ''; # avoids warnings, dies soon
	$str =~ s/\s*_RETURN\s*//i;
	my ($type, $descr) = split /\s+/, $str, 2;

	$type ||= ''; # avoids warnings, dies soon

	$type =~ /([\$\@])(.+)/;
	die "Type '$type' must have structure (\$|\@)<typename>, e.g. '\$boolean' or '\@string', died" unless $1 and $2;
	
	bless {
		_type   => $2,
		_descr  => $descr || '',
		_array  => $1 eq '@' ? 1 : 0,
	}, $pkg;
}

1;
__END__

=head1 NAME

Pod::WSDL::Return - Represents the WSDL pod for the return value of a method (internal use only)

=head1 SYNOPSIS

  use Pod::WSDL::Return;
  my $return = new Pod::WSDL::Return('_RETURN $string This returns blah ...');

=head1 DESCRIPTION

This module is used internally by Pod::WSDL. It is unlikely that you have to interact directly with it. If that is the case, take a look at the code, it is rather simple.

=head1 METHODS

=head2 new

Instantiates a new Pod::WSDL::Param. The method needs one parameter, the _RETURN string from the pod. Please see SYNOPSIS or the section "Pod Syntax" in the description of Pod::WSDL.

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
