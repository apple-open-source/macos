package # hide from pause
  MyModule;

use base 'Class::C3::Componentised';

sub component_base_class { "MyModule::Plugin" }

sub message { 
  my $msg = $_[0]->maybe::next::method() || '';
  
  return $msg . " MyModule";
}

sub new { 
  return bless {}, shift;
}

1;
