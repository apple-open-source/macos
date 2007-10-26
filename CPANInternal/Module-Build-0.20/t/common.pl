use strict;

sub have_module {
  my $module = shift;
  return eval "use $module; 1";
}

sub need_module {
  my $module = shift;
  skip_test("$module not installed") unless have_module($module);
}

sub skip_test {
  my $msg = @_ ? shift() : '';
  print "1..0 # Skipped: $msg\n";
  exit;
}

sub stdout_of {
  my $subr = shift;
  my $outfile = 'save_out';

  local *SAVEOUT;
  open SAVEOUT, ">&STDOUT" or die "Can't save STDOUT handle: $!";
  open STDOUT, "> $outfile" or die "Can't create $outfile: $!";

  eval {$subr->()};
  open STDOUT, ">&SAVEOUT" or die "Can't restore STDOUT: $!";

  return slurp($outfile);
}

sub slurp {
  my $fh = IO::File->new($_[0]) or die "Can't open $_[0]: $!";
  local $/;
  return <$fh>;
}

1;
