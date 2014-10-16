# Simple classes for testing.

sub Foo::new {
    bless { foo => $_[1] }, $_[0];
}

sub Foo::xyz {
    1;
}

sub Bar::new {
    bless { bar => $_[1] }, $_[0];
}

sub Bar::xyz {
    1;
}

{
    package Bar;
    use Scalar::Util qw(refaddr);
    use overload '""' => \&str, eq => \&eq, ne => \&ne;
    sub str { refaddr $_[0] }
    sub eq  {
              my $d0 = defined $_[0]->{bar};
	      my $d1 = defined $_[1]->{bar};
	      $d0 && $d1 ? $_[0]->{bar} eq $_[1]->{bar} :
              $d0 || $d0 ? 0 : 1;
            }
    sub ne  {
              my $d0 = defined $_[0]->{bar};
              my $d1 = defined $_[1]->{bar};
	      $d0 && $d1 ? $_[0]->{bar} ne $_[1]->{bar} :
              $d0 || $d0 ? 1 : 0;
            }
}

1;
