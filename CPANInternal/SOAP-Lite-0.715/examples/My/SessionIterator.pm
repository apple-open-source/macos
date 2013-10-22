package My::SessionIterator;

sub new { 
  my $self = shift;
  my $class = ref($self) || $self;
  bless {_num=>shift} => $class;
}

sub next {
  my $self = shift;
  $self->{_num}++;
}

1;