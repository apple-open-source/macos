#Lifted this code from Data::Compare by Fabien Tassin fta@sofaraway.org .
#Using it in the XML tests

use Carp;

sub Compare {
  croak "Usage: Data::Compare::Compare(x, y)\n" unless $#_ == 1;
  my $x = shift;
  my $y = shift;

  my $refx = ref $x;
  my $refy = ref $y;

  unless ($refx || $refy) { # both are scalars
    return $x eq $y if defined $x && defined $y; # both are defined
    !(defined $x || defined $y);
  }
  elsif ($refx ne $refy) { # not the same type
    0;
  }
  elsif ($x == $y) { # exactly the same reference
    1;
  }
  elsif ($refx eq 'SCALAR') {
    Compare($$x, $$y);
  }
  elsif ($refx eq 'ARRAY') {
    if ($#$x == $#$y) { # same length
      my $i = -1;
      for (@$x) {
	$i++;
	return 0 unless Compare($$x[$i], $$y[$i]);
      }
      1;
    }
    else {
      0;
    }
  }
  elsif ($refx eq 'HASH') {
    return 0 unless scalar keys %$x == scalar keys %$y;
    for (keys %$x) {
      next unless defined $$x{$_} || defined $$y{$_};
      return 0 unless defined $$y{$_} && Compare($$x{$_}, $$y{$_});
    }
    1;
  }
  elsif ($refx eq 'REF') {
    0;
  }
  elsif ($refx eq 'CODE') {
    1; #changed for log4perl, let's just accept coderefs
  }
  elsif ($refx eq 'GLOB') {
    0;
  }
  else { # a package name (object blessed)
    my ($type) = "$x" =~ m/^$refx=(\S+)\(/o;
    if ($type eq 'HASH') {
      my %x = %$x;
      my %y = %$y;
      Compare(\%x, \%y);
    }
    elsif ($type eq 'ARRAY') {
      my @x = @$x;
      my @y = @$y;
      Compare(\@x, \@y);
    }
    elsif ($type eq 'SCALAR') {
      my $x = $$x;
      my $y = $$y;
      Compare($x, $y);
    }
    elsif ($type eq 'GLOB') {
      0;
    }
    elsif ($type eq 'CODE') {
      1; #changed for log4perl, let's just accept coderefs
    }
    else {
      croak "Can't handle $type type.";
    }
  }
}

1;
