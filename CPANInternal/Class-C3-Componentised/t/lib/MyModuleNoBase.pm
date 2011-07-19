package # hide from pause
  MyModuleNoBase;

use base 'Class::C3::Componentised';

sub message { 
  my $msg = $_[0]->maybe::next::method() || '';
  
  return $msg . ' ' . __PACKAGE__;
}

sub new { 
  return bless {}, shift;
}

1;
