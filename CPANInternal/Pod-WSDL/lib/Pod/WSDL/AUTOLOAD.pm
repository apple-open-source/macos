package Pod::WSDL::AUTOLOAD;

use Carp;
use strict;
use warnings;

our $AUTOLOAD;
our $VERSION = "0.05";

sub AUTOLOAD {
    my $me     = shift;
    my $param  = shift;
    
    my $fbd = ref($me) . '::FORBIDDEN_METHODS';
    
    my $attr   = $AUTOLOAD;
    $attr =~ s/.*:://;

    if (@_) {
		croak ref $me . " received call to '$attr' with too many params (max 1). Call was '$attr($param, " . join (", ",  @_) .  ")'!"; 
    }
    
    if ($attr eq "DESTROY"){
		return;
    } elsif (exists $me->{'_' . $attr}) {
    	no strict 'refs';
		if (defined $param) {
			croak ref ($me) . " does not allow setting of '$attr', died" if (caller)[0] ne ref($me) and %$fbd and $fbd->{$attr} and !$fbd->{$attr}->{set};
		    $me->{'_' . $attr} = $param; 
	    	return $me;
		} else {
			croak ref ($me) . " does not allow getting of '$attr', died" if (caller)[0] ne ref($me) and %$fbd and $fbd->{$attr} and !$fbd->{$attr}->{get};
		    #if (ref $me->{'_' . $attr} eq 'ARRAY') {
			#    return @{$me->{'_' . $attr}};
		    #} elsif (ref $me->{'_' . $attr} eq 'HASH') {
			#    return %{$me->{'_' . $attr}};
		    #} elsif (ref $me->{'_' . $attr} eq 'SCALAR') {
			#    return ${$me->{'_' . $attr}};
		    #} else {
		    	return $me->{'_' . $attr};
		    #}
		}
    } else {
		croak "I have no method called '$attr()'!";
    }
}

1;
__END__

=head1 NAME

Pod::WSDL::AUTOLOAD - Base class for autoloading (internal use only)

=head1 SYNOPSIS

  package Foo;
  our @ISA = qw/Pod::WSDL::AUTOLOAD/;

  sub new {
    my $pgk = shift;
    bless {
  	  _bar => 'blah',
    }, $pgk
  }

  package main;
  use Foo;
  
  my $foo = new Foo();
  print $foo->bar;  # prints 'blah'
  
  $foo->bar('bloerch');  # sets _bar to 'bloerch'
  
  
=head1 DESCRIPTION

This module is used internally by Pod::WSDL. It is unlikely that you have to interact directly with it. The Pod::WSDL::AUTOLOADER is used as a base class and handels autoloading of accessor methods. If there is a property called _foo in a hash based blessed reference, it will allow the use of the method 'foo' as a getter and setter. As a getter is returns the value of _foo, as a setter it sets _foo with the argument and returns the object. You can exclude the accessor by using a hash %FORBIDDEN_METHODS in the derived class like that:

  our %FORBIDDEN_METHODS = (
	foo => {get => 1, set => 0},
	bar => {get => 0, set => 0}
  );

In this example it will not be allowed to set _foo and to set or get _bar. If the user of the object tries to do so, it croaks. From within the objects package every accessor is allowed.

=head1 METHODS

[none]

=head1 EXTERNAL DEPENDENCIES

  Carp;

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
