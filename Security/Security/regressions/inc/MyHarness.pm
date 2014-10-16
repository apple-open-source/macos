use warnings;
use strict;

package MyStraps;
use base qw( Test::Harness::Straps );
 
sub _command_line {
    my $self = shift;
    my $file = shift;

    $file = qq["$file"] if ($file =~ /\s/) && ($file !~ /^".*"$/);
    my $line = "$file";

    return $line;
}

sub _default_inc {
    return @INC;
}
 
package MyHarness;
use base qw( Test::Harness );

#my $Strap = MyStraps->new();
$Test::Harness::Strap = MyStraps->new();
 
sub strap { return $Test::Harness::Strap }
 
1;
