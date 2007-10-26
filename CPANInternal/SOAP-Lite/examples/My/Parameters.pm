package My::Parameters;

use vars qw(@ISA);
@ISA = qw(SOAP::Server::Parameters);

sub echo {
  my $self = shift;
  pop; # last parameter is envelope (SOAP::SOM object)
       # don't want to echo it
  @_;
}

sub echodata {
  my $self = shift;
  my @parameters = pop->dataof(SOAP::SOM::paramsin);
  @parameters;
}

sub echotwo {
  SOAP::Data->type(xml => "<a>$_[1]</a><b>$_[2]</b>");
}

sub autobind {
  my $self = shift;
  my $param1 = shift;
  my $param2 = SOAP::Data->name('myparam' => shift() * 2);
  return $param1, $param2;
}

sub addheader { 
  my $self = shift;
  my $param1 = shift;
  my $header = pop->headerof(SOAP::SOM::headers);
  return $param1, $header->value($header->value x 2);
}

sub byorder {
  my $self = shift;
  my($a, $b, $c) = @_;
  return "1=$a, 2=$b, 3=$c";
}

sub byname { # input parameter(s), envelope (SOAP::SOM object)
  my $self = shift;
  my($a, $b, $c) = SOAP::Server::Parameters::byName([qw(a b c)], @_);
  return "a=$a, b=$b, c=$c";
}

sub bynameororder { # input parameter(s), envelope (SOAP::SOM object)
  my $self = shift;
  my($a, $b, $c) = SOAP::Server::Parameters::byNameOrOrder([qw(a b c)], @_);
  return "a=$a, b=$b, c=$c";
}

sub die_simply {
  die 'Something bad happened in our method';
}

sub die_with_object {
  die SOAP::Data->name(something => 'value')->uri('http://www.soaplite.com/');
}

sub die_with_fault {
  die SOAP::Fault->faultcode('Server.Custom') # will be qualified
                 ->faultstring('Died in server method')
                 ->faultdetail(bless {code => 1} => 'BadError')
                 ->faultactor('http://www.soaplite.com/custom');
}

1;