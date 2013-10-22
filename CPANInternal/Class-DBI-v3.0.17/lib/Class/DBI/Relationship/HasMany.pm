package Class::DBI::Relationship::HasMany;

use strict;
use warnings;

use base 'Class::DBI::Relationship';

sub remap_arguments {
	my ($proto, $class, $accessor, $f_class, $f_key, $args) = @_;

	return $class->_croak($class->name . " needs an accessor name")
		unless $accessor;
	return $class->_croak($class->name . " needs a foreign class")
		unless $f_class;

	{
		no strict 'refs';
		defined &{"$class\::$accessor"}
			and return $class->_carp("$accessor method already exists in $class\n");
	}

	my @f_method = ();
	if (ref $f_class eq "ARRAY") {
		($f_class, @f_method) = @$f_class;
	}
	$class->_require_class($f_class);

	if (ref $f_key eq "HASH") {    # didn't supply f_key, this is really $args
		$args  = $f_key;
		$f_key = "";
	}

	$f_key ||= do {
		my $meta = $f_class->meta_info('has_a');
		my ($col) = grep $meta->{$_}->foreign_class eq $class, keys %$meta;
		$col || $class->table_alias;
	};

	if (ref $f_key eq "ARRAY") {
		return $class->_croak("Multi-column foreign keys not supported")
			if @$f_key > 1;
		$f_key = $f_key->[0];
	}

	$args ||= {};
	$args->{mapping}     = \@f_method;
	$args->{foreign_key} = $f_key;
	$args->{order_by} ||= $args->{sort};    # deprecated 0.96
	warn "sort argument to has_many deprecated in favour of order_by"
		if $args->{sort};                     # deprecated 0.96

	return ($class, $accessor, $f_class, $args);
}

sub _set_up_class_data {
	my $self = shift;
	$self->class->_extend_class_data(
		__hasa_list => $self->foreign_class => $self->args->{foreign_key});
	$self->SUPER::_set_up_class_data;
}

sub triggers {
	my $self = shift;
	if ($self->args->{no_cascade_delete}) {    # old undocumented way
		warn "no_cascade_delete deprecated in favour of cascade => None";
		return;
	}
	my $strategy = $self->args->{cascade} || "Delete";
	$strategy = "Class::DBI::Cascade::$strategy" unless $strategy =~ /::/;

	$self->foreign_class->_require_class($strategy);
	$strategy->can('cascade')
		or return $self->_croak("$strategy is not a valid Cascade Strategy");
	my $strat_obj = $strategy->new($self);
	return (before_delete => sub { $strat_obj->cascade(@_) });
}

sub methods {
	my $self     = shift;
	my $accessor = $self->accessor;
	return (
		$accessor          => $self->_has_many_method,
		"add_to_$accessor" => $self->_method_add_to,
	);
}

sub _method_add_to {
	my $rel      = shift;
	my $accessor = $rel->accessor;
	return sub {
		my ($self, $data) = @_;
		my $class = ref $self
			or return $self->_croak("add_to_$accessor called as class method");
		return $self->_croak("add_to_$accessor needs data")
			unless ref $data eq "HASH";

		my $meta = $class->meta_info($rel->name => $accessor);
		my ($f_class, $f_key, $args) =
			($meta->foreign_class, $meta->args->{foreign_key}, $meta->args);
		$data->{$f_key} = $self->id;

		# See if has_many constraints were defined and auto fill them
		if (defined $args->{constraint} && ref $args->{constraint} eq 'HASH') {
			while (my ($k, $v) = each %{ $args->{constraint} }) {
				$self->_croak(
					"Can't add_to_$accessor with $k = $data->{$k} (must be $v)")
					if defined($data->{$k}) && $data->{$k} ne $v;
				$data->{$k} = $v;
			}
		}

		$f_class->insert($data);
	};
}

sub _has_many_method {
	my $self       = shift;
	my $run_search = $self->_hm_run_search;
	my @mapping    = @{ $self->args->{mapping} } or return $run_search;
	return sub {
		return $run_search->(@_)->set_mapping_method(@mapping)
			unless wantarray;
		my @ret = $run_search->(@_);
		foreach my $meth (@mapping) { @ret = map $_->$meth(), @ret }
		return @ret;
		}
}

sub _hm_run_search {
	my $rel = shift;
	my ($class, $accessor) = ($rel->class, $rel->accessor);
	return sub {
		my ($self, @search_args) = @_;
		@search_args = %{ $search_args[0] } if ref $search_args[0] eq "HASH";
		my $meta = $class->meta_info($rel->name => $accessor);
		my ($f_class, $f_key, $args) =
			($meta->foreign_class, $meta->args->{foreign_key}, $meta->args);
		if (ref $self) {    # For $artist->cds
			unshift @search_args, %{ $args->{constraint} }
				if defined($args->{constraint}) && ref $args->{constraint} eq 'HASH';
			unshift @search_args, ($f_key => $self->id);
			push @search_args, { order_by => $args->{order_by} }
				if defined $args->{order_by};
			return $f_class->search(@search_args);
		} else {            # For Artist->cds
			    # Cross-table join as class method
			    # This stuff is highly experimental and will probably change beyond
			    # recognition. Use at your own risk...
			my %kv = @search_args;
			my $query = Class::DBI::Query->new({ owner => $f_class });
			$query->kings($class, $f_class);
			$query->add_restriction(sprintf "%s.%s = %s.%s",
				$f_class->table_alias, $f_key, $class->table_alias,
				$class->primary_column);
			$query->add_restriction("$_ = ?") for keys %kv;
			my $sth = $query->run(values %kv);
			return $f_class->sth_to_objects($sth);
		}
	};
}

1;
