package t::Support;

use strict;
use Config;
use File::ExtAttr qw/listfattr/;

sub should_skip {
  # NetBSD 3.1 and earlier don't support xattrs.
  # See <http://www.netbsd.org/Changes/changes-4.0.html#ufs>.
  if ($^O eq 'netbsd') {
    my @t = split(/\./, $Config{osvers});
    return 1 if ($t[0] <= 3);
  }

  return 0;
}

sub filter_system_attrs
{
  my @attrs = @_;

  if ($^O eq 'solaris')
  {
    # Filter out container for extensible system attributes on Solaris.
    @attrs = grep { ! /^SUNWattr_r[ow]$/ } @attrs;
  }
  return @attrs;
}

# Check to see whether the file has unremovable system attributes.
sub has_system_attrs
{
  my ($h) = @_;
  my $ret = 0;

  if ($^O eq 'solaris')
  {
    my @attrs = listfattr($h);
    if (scalar(grep { /^SUNWattr_r[ow]$/ } @attrs) > 0)
    {
      $ret = 1;
    }
  }

  return $ret;
}

1;

