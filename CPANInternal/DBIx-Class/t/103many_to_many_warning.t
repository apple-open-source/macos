use strict;
use warnings;
use Test::More;

use lib qw(t/lib);

plan tests => 4;
my $exp_warn = qr/The many-to-many relationship 'bars' is trying to create/;

{
  my @w; 
  local $SIG{__WARN__} = sub { $_[0] =~ $exp_warn ? push @w, $_[0] : warn $_[0] };
  my $code = gen_code ( suffix => 1 );
  eval "$code";
  ok (! $@, 'Eval code without warnings suppression')
    || diag $@;

  ok (@w, "Warning triggered without DBIC_OVERWRITE_HELPER_METHODS_OK");
}

{
  my @w; 
  local $SIG{__WARN__} = sub { $_[0] =~ $exp_warn ? push @w, $_[0] : warn $_[0] };

  my $code = gen_code ( suffix => 2 );

  local $ENV{DBIC_OVERWRITE_HELPER_METHODS_OK} = 1;
  eval "$code";
  ok (! $@, 'Eval code with warnings suppression')
    || diag $@;

  ok (! @w, "No warning triggered with DBIC_OVERWRITE_HELPER_METHODS_OK");
}

sub gen_code {

  my $args = { @_ };
  my $suffix = $args->{suffix};

  return <<EOF;
use strict;
use warnings;

{
  package #
    DBICTest::Schema::Foo${suffix};
  use base 'DBIx::Class::Core';

  __PACKAGE__->table('foo');
  __PACKAGE__->add_columns(
    'fooid' => {
      data_type => 'integer',
      is_auto_increment => 1,
    },
  );
  __PACKAGE__->set_primary_key('fooid');


  __PACKAGE__->has_many('foo_to_bar' => 'DBICTest::Schema::FooToBar${suffix}' => 'bar');
  __PACKAGE__->many_to_many( foos => foo_to_bar => 'bar' );
}
{
  package #
    DBICTest::Schema::FooToBar${suffix};

  use base 'DBIx::Class::Core';
  __PACKAGE__->table('foo_to_bar');
  __PACKAGE__->add_columns(
    'foo' => {
      data_type => 'integer',
    },
    'bar' => {
      data_type => 'integer',
    },
  );
  __PACKAGE__->belongs_to('foo' => 'DBICTest::Schema::Foo${suffix}');
  __PACKAGE__->belongs_to('bar' => 'DBICTest::Schema::Foo${suffix}');
}
{
  package #
    DBICTest::Schema::Bar${suffix};

  use base 'DBIx::Class::Core';

  __PACKAGE__->table('bar');
  __PACKAGE__->add_columns(
    'barid' => {
      data_type => 'integer',
      is_auto_increment => 1,
    },
  );

  __PACKAGE__->set_primary_key('barid');
  __PACKAGE__->has_many('foo_to_bar' => 'DBICTest::Schema::FooToBar${suffix}' => 'foo');

  __PACKAGE__->many_to_many( bars => foo_to_bar => 'foo' );

  sub add_to_bars {}
}
EOF

}
