package Class::DBI::Search::Basic;

=head1 NAME

Class::DBI::Search::Basic - Simple Class::DBI search

=head1 SYNOPSIS

	my $searcher = Class::DBI::Search::Basic->new(
		$cdbi_class, @search_args
	);

	my @results = $searcher->run_search;

	# Over in your Class::DBI subclass:
	
	__PACKAGE__->add_searcher(
		search  => "Class::DBI::Search::Basic",
	  isearch => "Class::DBI::Search::Plugin::CaseInsensitive",
	);

=head1 DESCRIPTION

This is the start of a pluggable Search infrastructure for Class::DBI.

At the minute Class::DBI::Search::Basic doubles up as both the default
search within Class::DBI as well as the search base class. We will
probably need to tease this apart more later and create an abstract base
class for search plugins.

=head1 METHODS

=head2 new 

	my $searcher = Class::DBI::Search::Basic->new(
		$cdbi_class, @search_args
	);

A Searcher is created with the class to which the results will belong,
and the arguments passed to the search call by the user.

=head2 opt 

	if (my $order = $self->opt('order_by')) { ... }

The arguments passed to search may contain an options hash. This will
return the value of a given option.

=head2 run_search

	my @results = $searcher->run_search;
	my $iterator = $searcher->run_search;

Actually run the search.

=head1 SUBCLASSING

=head2 sql / bind / fragment

The actual mechanics of generating the SQL and executing it split up
into a variety of methods for you to override.

run_search() is implemented as:

  return $cdbi->sth_to_objects($self->sql, $self->bind);

Where sql() is 

  $cdbi->sql_Retrieve($self->fragment);


There are also a variety of private methods underneath this that could
be overriden in a pinch, but if you need to do this I'd rather you let
me know so that I can make them public, or at least so that I don't
remove them from under your feet.

=cut

use strict;
use warnings;

use base 'Class::Accessor::Fast';
__PACKAGE__->mk_accessors(qw/class args opts type/);

sub new {
	my ($me, $proto, @args) = @_;
	my ($args, $opts) = $me->_unpack_args(@args);
	bless {
		class => ref $proto || $proto,
		args  => $args,
		opts  => $opts,
		type  => "=",
	} => $me;
}

sub opt {
	my ($self, $option) = @_;
	$self->{opts}->{$option};
}

sub _unpack_args {
	my ($self, @args) = @_;
	@args = %{ $args[0] } if ref $args[0] eq "HASH";
	my $opts = @args % 2 ? pop @args : {};
	return (\@args, $opts);
}

sub _search_for {
	my $self  = shift;
	my @args  = @{ $self->{args} };
	my $class = $self->{class};
	my %search_for;
	while (my ($col, $val) = splice @args, 0, 2) {
		my $column = $class->find_column($col)
			|| (List::Util::first { $_->accessor eq $col } $class->columns)
			|| $class->_croak("$col is not a column of $class");
		$search_for{$column} = $class->_deflated_column($column, $val);
	}
	return \%search_for;
}

sub _qual_bind {
	my $self = shift;
	$self->{_qual_bind} ||= do {
		my $search_for = $self->_search_for;
		my $type       = $self->type;
		my (@qual, @bind);
		for my $column (sort keys %$search_for) {    # sort for prepare_cached
			if (defined(my $value = $search_for->{$column})) {
				push @qual, "$column $type ?";
				push @bind, $value;
			} else {

				# perhaps _carp if $type ne "="
				push @qual, "$column IS NULL";
			}
		}
		[ \@qual, \@bind ];
	};
}

sub _qual {
	my $self = shift;
	$self->{_qual} ||= $self->_qual_bind->[0];
}

sub bind {
	my $self = shift;
	$self->{_bind} ||= $self->_qual_bind->[1];
}

sub fragment {
	my $self = shift;
	my $frag = join " AND ", @{ $self->_qual };
	if (my $order = $self->opt('order_by')) {
		$frag .= " ORDER BY $order";
	}
	return $frag;
}

sub sql {
	my $self = shift;
	return $self->class->sql_Retrieve($self->fragment);
}

sub run_search {
	my $self = shift;
	my $cdbi = $self->class;
	return $cdbi->sth_to_objects($self->sql, $self->bind);
}

1;
