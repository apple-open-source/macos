package # hide from PAUSE
    DBIx::Class::Relationship::ManyToMany;

use strict;
use warnings;

sub many_to_many {
  my ($class, $meth, $rel, $f_rel, $rel_attrs) = @_;

  $class->throw_exception(
    "missing relation in many-to-many"
  ) unless $rel;

  $class->throw_exception(
    "missing foreign relation in many-to-many"
  ) unless $f_rel;

  {
    no strict 'refs';
    no warnings 'redefine';

    my $add_meth = "add_to_${meth}";
    my $remove_meth = "remove_from_${meth}";
    my $set_meth = "set_${meth}";
    my $rs_meth = "${meth}_rs";

    for ($add_meth, $remove_meth, $set_meth, $rs_meth) {
      warn "***************************************************************************\n".
           "The many-to-many relationship $meth is trying to create a utility method called $_. This will overwrite the existing method on $class. You almost certainly want to rename your method or the many-to-many relationship, as your method will not be callable (it will use the one from the relationship instead.) YOU HAVE BEEN WARNED\n".
           "***************************************************************************\n"
        if $class->can($_);
    }

    $rel_attrs->{alias} ||= $f_rel;

    *{"${class}::${meth}_rs"} = sub {
      my $self = shift;
      my $attrs = @_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {};
      my @args = ($f_rel, @_ > 0 ? @_ : undef, { %{$rel_attrs||{}}, %$attrs });
      my $rs = $self->search_related($rel)->search_related(
        $f_rel, @_ > 0 ? @_ : undef, { %{$rel_attrs||{}}, %$attrs }
      );
	  return $rs;
    };

	*{"${class}::${meth}"} = sub {
		my $self = shift;
		my $rs = $self->$rs_meth( @_ );
  		return (wantarray ? $rs->all : $rs);
	};

    *{"${class}::${add_meth}"} = sub {
      my $self = shift;
      @_ > 0 or $self->throw_exception(
        "${add_meth} needs an object or hashref"
      );
      my $source = $self->result_source;
      my $schema = $source->schema;
      my $rel_source_name = $source->relationship_info($rel)->{source};
      my $rel_source = $schema->resultset($rel_source_name)->result_source;
      my $f_rel_source_name = $rel_source->relationship_info($f_rel)->{source};
      my $f_rel_rs = $schema->resultset($f_rel_source_name)->search({}, $rel_attrs||{});

      my $obj;
      if (ref $_[0]) {
        if (ref $_[0] eq 'HASH') {
          $obj = $f_rel_rs->create($_[0]);
        } else {
          $obj = $_[0];
        }
      } else {
        $obj = $f_rel_rs->create({@_});
      }

      my $link_vals = @_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {};
      my $link = $self->search_related($rel)->new_result($link_vals);
      $link->set_from_related($f_rel, $obj);
      $link->insert();
	  return $obj;
    };

    *{"${class}::${set_meth}"} = sub {
      my $self = shift;
      @_ > 0 or $self->throw_exception(
        "{$set_meth} needs a list of objects or hashrefs"
      );
      my @to_set = (ref($_[0]) eq 'ARRAY' ? @{ $_[0] } : @_);
      $self->search_related($rel, {})->delete;
      $self->$add_meth($_) for (@to_set);
    };

    *{"${class}::${remove_meth}"} = sub {
      my $self = shift;
      @_ > 0 && ref $_[0] ne 'HASH'
        or $self->throw_exception("${remove_meth} needs an object");
      my $obj = shift;
      my $rel_source = $self->search_related($rel)->result_source;
      my $cond = $rel_source->relationship_info($f_rel)->{cond};
      my $link_cond = $rel_source->resolve_condition(
        $cond, $obj, $f_rel
      );
      $self->search_related($rel, $link_cond)->delete;
    };

  }
}

1;
