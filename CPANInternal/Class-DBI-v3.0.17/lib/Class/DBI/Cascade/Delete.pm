package Class::DBI::Cascade::Delete;

=head1 NAME

Class::DBI::Cascade::Delete - Delete related objects

=head1 DESCRIPTION

This is a Cascading Delete strategy that will delete any related
objects.

=cut

use strict;
use warnings;

use base 'Class::DBI::Cascade::None';

sub cascade {
	my ($self, $obj) = @_;
	$self->foreign_for($obj)->delete_all;
}

1;
