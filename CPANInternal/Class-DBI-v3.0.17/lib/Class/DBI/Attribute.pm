package Class::DBI::Attribute;

=head1 NAME

Class::DBI::Attribute - A value in a column.

=head1 SYNOPSIS

	my $column = Class::DBI::Attribute->new($column => $value);

=head1 DESCRIPTION

This stores the row-value of a certain column in an object.
You probably shouldn't be dealing with this directly, and its interface
is liable to change without notice.

=head1 METHODS

=cut

use strict;
use base 'Class::Accessor::Fast';

__PACKAGE__->mk_accessors(qw/column current_value known/);

use overload
	'""' => sub { shift->current_value },
	fallback => 1;

sub new {
	my ($class, $col, $val) = @_;
	$class->SUPER::new({ 
		column => $col, current_value => $val, known => 1,
	});
}

1;
