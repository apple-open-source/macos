package # hide from PAUSE
    DBIx::Class::CDBICompat::HasMany;

use strict;
use warnings;

sub has_many {
  my ($class, $rel, $f_class, $f_key, $args) = @_;

  my @f_method;

  if (ref $f_class eq 'ARRAY') {
    ($f_class, @f_method) = @$f_class;
  }

  if (ref $f_key eq 'HASH' && !$args) { $args = $f_key; undef $f_key; };

  $args ||= {};
  if (delete $args->{no_cascade_delete}) {
    $args->{cascade_delete} = 0;
  }

  if( !$f_key and !@f_method ) {
      my $f_source = $f_class->result_source_instance;
      ($f_key) = grep { $f_source->relationship_info($_)->{class} eq $class }
                      $f_source->relationships;
  }

  $class->next::method($rel, $f_class, $f_key, $args);

  if (@f_method) {
    no strict 'refs';
    no warnings 'redefine';
    my $post_proc = sub { my $o = shift; $o = $o->$_ for @f_method; $o; };
    *{"${class}::${rel}"} =
      sub {
        my $rs = shift->search_related($rel => @_);
        $rs->{attrs}{record_filter} = $post_proc;
        return (wantarray ? $rs->all : $rs);
      };
    return 1;
  }

}

1;
