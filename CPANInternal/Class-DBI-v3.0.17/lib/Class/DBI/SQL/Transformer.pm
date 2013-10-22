package Class::DBI::SQL::Transformer;

use strict;
use warnings;

=head1 NAME

Class::DBI::SQL::Transformer - Transform SQL

=head1 SYNOPSIS

  my $trans = $tclass->new($self, $sql, @args);
  return $self->SUPER::transform_sql($trans->sql => $trans->args);

=head1 DESCRIPTION

Class::DBI hooks into the transform_sql() method in Ima::DBI to provide
its own SQL extensions. Class::DBI::SQL::Transformer does the heavy
lifting of these transformations.

=head1 CONSTRUCTOR

=head2 new

  my $trans = $tclass->new($self, $sql, @args);

Create a new transformer for the SQL and arguments that will be used
with the given object (or class).

=cut

sub new {
	my ($me, $caller, $sql, @args) = @_;
	bless {
		_caller      => $caller,
		_sql         => $sql,
		_args        => [@args],
		_transformed => 0,
	} => $me;
}

=head2 sql / args

  my $sql = $trans->sql;
  my @args = $trans->args;

The transformed SQL and args.

=cut

# TODO Document what the different transformations are
# and factor out how they're called so that people can pick and mix the
# ones they want and add new ones.

sub sql {
	my $self = shift;
	$self->_do_transformation if !$self->{_transformed};
	return $self->{_transformed_sql};
}

sub args {
	my $self = shift;
	$self->_do_transformation if !$self->{_transformed};
	return @{ $self->{_transformed_args} };
}

sub _expand_table {
	my $self = shift;
	my ($class, $alias) = split /=/, shift, 2;
	my $caller = $self->{_caller};
	my $table = $class ? $class->table : $caller->table;
	$self->{cmap}{ $alias || $table } = $class || ref $caller || $caller;
	($alias ||= "") &&= " $alias";
	return $table . $alias;
}

sub _expand_join {
	my $self  = shift;
	my $joins = shift;
	my @table = split /\s+/, $joins;

	my $caller = $self->{_caller};
	my %tojoin = map { $table[$_] => $table[ $_ + 1 ] } 0 .. $#table - 1;
	my @sql;
	while (my ($t1, $t2) = each %tojoin) {
		my ($c1, $c2) = map $self->{cmap}{$_}
			|| $caller->_croak("Don't understand table '$_' in JOIN"), ($t1, $t2);

		my $join_col = sub {
			my ($c1, $c2) = @_;
			my $meta = $c1->meta_info('has_a');
			my ($col) = grep $meta->{$_}->foreign_class eq $c2, keys %$meta;
			$col;
		};

		my $col = $join_col->($c1 => $c2) || do {
			($c1, $c2) = ($c2, $c1);
			($t1, $t2) = ($t2, $t1);
			$join_col->($c1 => $c2);
		};

		$caller->_croak("Don't know how to join $c1 to $c2") unless $col;
		push @sql, sprintf " %s.%s = %s.%s ", $t1, $col, $t2, $c2->primary_column;
	}
	return join " AND ", @sql;
}

sub _do_transformation {
	my $me     = shift;
	my $sql    = $me->{_sql};
	my @args   = @{ $me->{_args} };
	my $caller = $me->{_caller};

	$sql =~ s/__TABLE\(?(.*?)\)?__/$me->_expand_table($1)/eg;
	$sql =~ s/__JOIN\((.*?)\)__/$me->_expand_join($1)/eg;
	$sql =~ s/__ESSENTIAL__/join ", ", $caller->_essential/eg;
	$sql =~
		s/__ESSENTIAL\((.*?)\)__/join ", ", map "$1.$_", $caller->_essential/eg;
	if ($sql =~ /__IDENTIFIER__/) {
		my $key_sql = join " AND ", map "$_=?", $caller->primary_columns;
		$sql =~ s/__IDENTIFIER__/$key_sql/g;
	}

	$me->{_transformed_sql}  = $sql;
	$me->{_transformed_args} = [@args];
	$me->{_transformed}      = 1;
	return 1;
}

1;

