
=head1 NAME

Sample - Foo foo sample foo

=head1 AUTHOR

Sample Man <sample@example.com>

=head1 OTHER

version lines in pod should be ignored:
$VERSION = 'not ok: got version from pod!';

=cut

package Sample;

# version lines in comments should be ignored:
# $VERSION = 'not ok: got version from comment!';

# this is the version line we're looking for:
$VERSION = ('0.01')[0];  # should be eval'd

{
  # we should only take the first version line found:
  local
    $VERSION = 'not ok: got second version from code!';
}

1;
