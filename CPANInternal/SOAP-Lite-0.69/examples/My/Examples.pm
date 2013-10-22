package My::Examples;

my @states = (undef, # we want to start from one :)
  qw/Alabama Alaska Arizona Arkansas California Colorado Connecticut
  Delaware Florida Georgia Hawaii Idaho Illinois Indiana Iowa Kansas
  Kentucky Louisiana Maine Maryland Massachusetts Michigan Minnesota
  Mississippi Missouri Montana Nebraska Nevada/, 'New Hampshire',
  'New Jersey', 'New Mexico', 'New York', 'North Carolina',
  'North Dakota', qw/Ohio Oklahoma Oregon Pennsylvania/, 'Rhode Island',
  'South Carolina', 'South Dakota', qw/Tennessee Texas Utah Vermont
  Virginia Washington/, 'West Virginia', 'Wisconsin', 'Wyoming'
);

sub getStateName {
  my $self = shift;
  $states[shift];
}

sub getStateNames {
  my $self = shift;
  join "\n", map {$states[$_]} @_;
}

sub getStateList {
  my $self = shift;
  [map {$states[$_]} @{shift()}];
}

sub getStateStruct {
  my $self = shift;
  my %states = %{shift()};
  # be careful to distinguish block from hash. Just {} won't work
  +{map {$_ => $states[$states{$_}]} keys %states}; 
}

1;
