package My::Chat;

my @messages;
my %users;

sub join { 
  my $self = shift;
  my $class = ref($self) || $self;
  my $nick = shift      or die "User cannot join chat anonymously\n"; 
  !exists $users{$nick} or die "User '$nick' is already in chatroom. Choose another nick\n";
  $users{$nick} = time;
  my $messages = shift || 10; 
  bless {
    _nick        => $nick,
    _users       => \%users,
    _messages    => \@messages,
    _lastmessage => ($#messages > $messages ? $#messages - $messages : -1),
  } => $class;
}

sub get {
  my $self = shift;
  my $nick = $self->{_nick};
  my @mess = grep {exists $users{$_->[0]} && $_->[0] ne $nick} @messages[($self->{_lastmessage}+1)..$#messages];
  $self->{_lastmessage} = $#messages;
  [@mess];
}

sub send { 
  push @messages, [shift->{_nick} => shift, time]; 
  splice(@messages, 0, -12); # we'll keep only last 12 messages
}

sub whois { shift->{_users} }

sub quit { my $self = shift; delete $self->{_users}->{$self->{_nick}} }

sub DESTROY { shift->quit }

1;
