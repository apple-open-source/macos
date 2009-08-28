##################################################
# String dispatcher for testing
##################################################

package Log::Dispatch::String;

use Log::Dispatch::Output;
use base qw( Log::Dispatch::Output );
use fields qw( stderr );

sub new
{
    my $proto = shift;
    my $class = ref $proto || $proto;
    my %params = @_;

    my $self = bless {}, $class;

    $self->_basic_init(%params);
    $self->{stderr} = exists $params{stderr} ? $params{stderr} : 1;
    $self->{buffer} = "";

    return $self;
}

sub log_message
{   
    my $self = shift;
    my %params = @_;

    $self->{buffer} .= $params{message};
}

sub buffer
{   
    my($self, $new) = @_;

    if(defined $new) {
        $self->{buffer} = $new;
    }

    return $self->{buffer};
}

sub reset
{   
    my($self) = @_;

    $self->{buffer} = "";
}

1;
