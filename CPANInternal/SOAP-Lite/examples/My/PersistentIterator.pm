package My::PersistentIterator;

my $iterator;

sub new { 
  my $self = shift;
  my $class = ref($self) || $self;
  $iterator ||= (bless {_num=>shift} => $class);
}

sub next {
  my $self = shift;
  $self->{_num}++;
}

1;