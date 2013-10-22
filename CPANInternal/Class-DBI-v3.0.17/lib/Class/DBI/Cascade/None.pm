package Class::DBI::Cascade::None;

=head1 NAME

Class::DBI::Cascade::None - Do nothing upon deletion

=head1 DESCRIPTION

This is a Cascading Delete strategy that will do nothing, leaving
orphaned records behind.

It is the base class for most ofther Cascade strategies, and so provides
several important methods:

=head1 CONSTRUCTOR

=head2 new

	my $strategy = Cascade::Class->new($Relationship);

This must be instantiated with a Class::DBI::Relationship object.

=head1 METHODS

=head2 foreign_for

	my $iterator = $strategy->foreign_for($obj);

This will return all the objects which are foreign to $obj across the
relationship. It's a normal Class::DBI search you can get the results
either as a list or as an iterator.

=head2 cascade

	$strategy->cascade($obj);

Cascade across the related objects to $obj.

=head1 WRITING NEW STRATEGIES

Creating a Cascade strategy should be fairly simple. You usually just
need to inherit from here, and then supply a cascade() method that does
the required thing with the results from foreign_for().

So, for example, Cascade::Delete is implemented simply as:

	package Class::DBI::Cascade::Delete;

	use base 'Class::DBI::Cascade::None';

	sub cascade {
		my ($self, $obj) = @_;
		$self->foreign_for($obj)->delete_all;
	}

=cut

use strict;
use warnings;

sub new {
	my ($class, $rel) = @_;
	bless { _rel => $rel } => $class;
}

sub foreign_for {
	my ($self, $obj) = @_;
	return $self->{_rel}
		->foreign_class->search($self->{_rel}->args->{foreign_key} => $obj->id);
}

sub cascade { return; }

1;
