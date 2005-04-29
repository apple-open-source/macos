package Razor2::Client::Engine;

use strict;
use Digest::SHA1 qw(sha1_hex);
use Data::Dumper;
use Razor2::Signature::Ephemeral;
use Razor2::String qw(hextobase64 makesis debugobj);

# meant to be inherited
#
sub new {
    return {};
}


sub supported_engines {

    my @a = qw( 4 );

    my $hr = {};
    foreach (@a) { $hr->{$_} = 1; }

    return wantarray ? @a : $hr;
}


sub compute_engine {
    my ($self, $engine, @params) = @_;

    return $self->vr1_signature(@params) if $engine == 1;
    return $self->vr2_signature(@params) if $engine == 2;
    # return $self->vr3_signature(@params) if $engine == 3;
    return $self->vr4_signature(@params) if $engine == 4;

    $self->log (1,"engine $engine not supported");
    return;
}

#
# The following *_signature subroutines should be
# the same as the ones on the server 
# 


#
# VR1 Engine - Razor 1.0 SHA1 signatures
#
# fixme - how is this different from VR2 ?
#
sub vr1_signature { 
    my ($self, $text) = @_;

    my $sig = hextobase64(sha1_hex($$text));
    $self->log (11,"engine 1 computing on ". length($$text) .", sig=$sig");
    return $sig;
}



#
# VR2 Engine - SHA1 signatures of decoded body content
#
sub vr2_signature { 
    my ($self, $text) = @_;

    my $sha1 = sha1_hex($$text);
    my $h2b = hextobase64($sha1);
    $self->log (11,"engine 2 computing on ". length($$text) .", sig=$h2b");
    return $h2b;
}


#
# VR3 Engine - Nilsimsa signatures of decoded body content
#
#sub vr3_signature { 
#
#    my ($self, $text) = @_;
#
#    my $nilsimsa = new Digest::Nilsimsa;
#    $self->log(1,"couldn't load Digest::Nilsimsa") unless $nilsimsa;
#
#    my $digest = $nilsimsa->text2digest($$text);
#    my $h2b = hextobase64($digest);
#
#    #my $line = debugobj($text);
#    #$self->log(8,"Nilsimsa digest: $digest; b64: $h2b;\n$line");
#
#    $self->log (11,"engine 3 computing on ". length($$text) .", sig=$h2b");
#
#    return $h2b;
#}



#
# VR4 Engine - Ephemereal signatures of decoded body content
#
sub vr4_signature { 

    my ($self, $text, $ep4) = @_;

    my ($seed, $separator) = split /-/, $ep4, 2;

    return $self->log(1,"vr4_signature: Bad ep4: $ep4") unless ($seed && $separator);

    my $ehash = new Razor2::Signature::Ephemeral (seed => $seed, separator => $separator);
    my $digest = $ehash->hexdigest($$text);

    my $sig = hextobase64($digest);
    $self->log (11,"engine 4 computing on ". length($$text) .", sig=$sig");
    return $sig;
}




1;

