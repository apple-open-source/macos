package Module::Build::Platform::VMS;

use strict;
use Module::Build::Base;

use vars qw(@ISA);
@ISA = qw(Module::Build::Base);


=head1 NAME

Module::Build::Platform::VMS - Builder class for VMS platforms

=head1 DESCRIPTION

This module inherits from C<Module::Build::Base> and alters a few
minor details of its functionality.  Please see L<Module::Build> for
the general docs.

=head2 Overridden Methods

=over 4

=item new

Change $self->{build_script} to 'Build.com' so @Build works.

=cut

sub new {
    my $class = shift;
    my $self = $class->SUPER::new(@_);

    $self->{properties}{build_script} = 'Build.com';

    return $self;
}


=item cull_args

'@Build foo' on VMS will not preserve the case of 'foo'.  Rather than forcing
people to write '@Build "foo"' we'll dispatch case-insensitively.

=cut

sub cull_args {
    my $self = shift;
    my($action, $args) = $self->SUPER::cull_args(@_);
    my @possible_actions = grep { lc $_ eq lc $action } $self->known_actions;

    die "Ambiguous action '$action'.  Could be one of @possible_actions"
        if @possible_actions > 1;

    return ($possible_actions[0], $args);
}


=item manpage_separator

Use '__' instead of '::'.

=cut

sub manpage_separator {
    return '__';
}


=head1 AUTHOR

Michael G Schwern <schwern@pobox.com>

=head1 SEE ALSO

perl(1), Module::Build(3), ExtUtils::MakeMaker(3)

=cut

1;
__END__
