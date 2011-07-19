package # hide from PAUSE
    DBICTest::Schema::Encoded;

use base qw/DBICTest::BaseResult/;

use strict;
use warnings;

__PACKAGE__->table('encoded');
__PACKAGE__->add_columns(
    'id' => {
        data_type => 'integer',
        is_auto_increment => 1
    },
    'encoded' => {
        data_type => 'varchar',
        size      => 100,
        is_nullable => 1,
    },
);

__PACKAGE__->set_primary_key('id');

sub set_column {
  my ($self, $col, $value) = @_;
  if( $col eq 'encoded' ){
    $value = reverse split '', $value;
  }
  $self->next::method($col, $value);
}

sub new {
  my($self, $attr, @rest) = @_;
  $attr->{encoded} = reverse split '', $attr->{encoded}
    if defined $attr->{encoded};
  return $self->next::method($attr, @rest);
}

1;
