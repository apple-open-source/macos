#!/usr/bin/perl -sw
##
## Razor2::Errorhandler -- Base class that provides error 
##                         handling functionality.
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Errorhandler.pm,v 1.1 2004/04/19 17:50:31 dasenbro Exp $

package Razor2::Errorhandler; 
use strict;

sub new { 
    bless {}, shift
}


sub error { 
    no strict;
    my ($self, $errstr, $construction_error) = @_;
    if ($construction_error) { 
        my ($package, @undef) = caller();
        my $location = "$package\::errstr";
        my $spot = *{$location}{SCALAR};
        $$spot = "$errstr\n";
    } else {
        $$self{errstr} = "$errstr\n";
    }
    $self->log($self->{logerrors},"Error: $errstr\n") if $self->{logerrors};
    use strict;
    return;
} 


sub errstr { 
    my $self = shift;
    return $$self{errstr};
}

sub errprefix { 
    my ($self, $prefix) = @_;
    $$self{errstr} = $prefix .": ". $$self{errstr};
    return;
}

sub errstrrst { 
    my $self = shift;
    $$self{errstr} = "";
}

1;


=head1 NAME

Razor::Errorhandler - Error handling mechanism for Razor.

=head1 SYNOPSIS

    package Foo;

    use Razor::Errorhandler;
    @ISA = qw(Razor::Errorhandler);
    
    sub alive { 
        ..
        ..
        return 
        $self->error ("Awake, awake! Ring the alarum bell. \
                       Murther and treason!", $dagger) 
            if $self->murdered($king);
    }


    package main; 

    use Foo;
    my $foo = new Foo;
    $foo->alive($king) or print $foo->errstr(); 
    # prints "Awake, awake! ... "

=head1 DESCRIPTION 

Razor::Errorhandler encapsulates the error handling mechanism used by the
modules in Razor bundle. Razor::Errorhandler doesn't have a constructor
and is meant to be inherited. The derived modules use its two methods,
error() and errstr(), to communicate error messages to the caller.

When a method of the derived module fails, it calls $self->error() and
returns  to the caller. The error message passed to error() is made
available to the caller through the errstr() accessor. error() also
accepts a list of sensitive data that it wipes out (undef'es) before
returning.

The caller should B<never> call errstr() to check for errors. errstr()
should be called only when a method indicates (usually through an undef
return value) that an error has occured. This is because errstr() is
never overwritten and will always contain a value after the occurance of
first error.

=head1 METHODS

=over 4

=item B<error($mesage, ($wipeme, $wipemetoo))>

The first argument to error() is $message which is placed in
$self->{errstr} and the remaining arguments are interpretted as variables
containing sensitive data that are wiped out from the memory. error()
always returns undef.

=item B<errstr()> 

errstr() is an accessor method for $self->{errstr}.

=back

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Razor::Client(3)

=cut


