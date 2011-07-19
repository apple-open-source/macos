
package Class::MOP::Method::Generated;

use strict;
use warnings;

use Carp 'confess';

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use base 'Class::MOP::Method';

use constant _PRINT_SOURCE => $ENV{MOP_PRINT_SOURCE} ? 1 : 0;

## accessors

sub new {
    confess __PACKAGE__ . " is an abstract base class, you must provide a constructor.";
}

sub is_inline { $_[0]{is_inline} }

sub definition_context { $_[0]{definition_context} }

sub _initialize_body {
    confess "No body to initialize, " . __PACKAGE__ . " is an abstract base class";
}

sub _eval_closure {
    # my ($self, $captures, $sub_body) = @_;
    my $__captures = $_[1];

    my $code;

    my $e = do {
        local $@;
        local $SIG{__DIE__};
        my $source = join
            "\n", (
            map {
                /^([\@\%\$])/
                    or die "capture key should start with \@, \% or \$: $_";
                q[my ] 
                    . $_ . q[ = ] 
                    . $1
                    . q[{$__captures->{']
                    . $_ . q['}};];
                } keys %$__captures
            ),
            $_[2];
        print STDERR "\n", $_[0]->name, ":\n", $source, "\n" if _PRINT_SOURCE;
        $code = eval $source;
        $@;
    };

    return ( $code, $e );
}

sub _add_line_directive {
    my ( $self, %args ) = @_;

    my ( $line, $file );

    if ( my $ctx = ( $args{context} || $self->definition_context ) ) {
        $line = $ctx->{line};
        if ( my $desc = $ctx->{description} ) {
            $file = "$desc defined at $ctx->{file}";
        } else {
            $file = $ctx->{file};
        }
    } else {
        ( $line, $file ) = ( 0, "generated method (unknown origin)" );
    }

    my $code = $args{code};

    # if it's an array of lines, join it up
    # don't use newlines so that the definition context is more meaningful
    $code = join(@$code, ' ') if ref $code;

    return qq{#line $line "$file"\n} . $code;
}

sub _compile_code {
    my ( $self, %args ) = @_;

    my $code = $self->_add_line_directive(%args);

    return $self->_eval_closure($args{environment}, $code);
}

1;

__END__

=pod

=head1 NAME 

Class::MOP::Method::Generated - Abstract base class for generated methods

=head1 DESCRIPTION

This is a C<Class::MOP::Method> subclass which is subclassed by
C<Class::MOP::Method::Accessor> and
C<Class::MOP::Method::Constructor>.

It is not intended to be used directly.

=head1 AUTHORS

Stevan Little E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

