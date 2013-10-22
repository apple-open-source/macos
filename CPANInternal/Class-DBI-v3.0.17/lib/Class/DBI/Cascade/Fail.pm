package Class::DBI::Cascade::Fail;

=head1 NAME

Class::DBI::Cascade::Fail - Do not cascade if foreign objects exist

=head1 DESCRIPTION

This is a Cascading Delete strategy that will throw an error if any
object about to be deleted still has any other objects pointing at it.

=cut

use strict;
use warnings;

use base 'Class::DBI::Cascade::None';

sub cascade {
	my ($self, $obj) = @_;
	my $refs = $self->foreign_for($obj)->count or return;
	$self->{_rel}->foreign_class->_croak(
		"$refs objects still refer to $obj. Deletion failed");
}

1;
