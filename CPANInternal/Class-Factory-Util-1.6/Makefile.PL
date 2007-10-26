use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'		=> 'Class::Factory::Util',
    'VERSION_FROM'	=> 'lib/Class/Factory/Util.pm',
);

package MY;

sub libscan
{
    my $self = shift;
    my $file = shift;

    return $file =~ /svn/ ? 0 : $self->SUPER::libscan($file);
}
