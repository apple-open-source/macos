package Razor2::Preproc::Manager;
use Razor2::Preproc::deBase64;
use Razor2::Preproc::deQP;
use Razor2::Preproc::deHTMLxs;
use Razor2::Preproc::deHTML;
use Razor2::Preproc::deNewline;
use Data::Dumper;
use strict;

sub new  {

    my ($class, %args) = @_;

    my %self = ();
    
    $self{deBase64}         = new Razor2::Preproc::deBase64  unless exists $args{no_deBase64};
    $self{deQP}             = new Razor2::Preproc::deQP      unless exists $args{no_deQP};
    $self{deHTML}           = new Razor2::Preproc::deHTMLxs  unless exists $args{no_deHTML};
    $self{deNewline}        = new Razor2::Preproc::deNewline unless exists $args{no_deNewline}; 
    $self{rm}               = $args{RM};

    return bless \%self, $class;

}

#
# $bodyref must be Headers\n\r*\nBody
# for this to work.   Cleaned ref to Body returned.
# 
sub preproc {

    my ($self, $bodyref, $dolength) = @_;
    my $lengths = { '1_orig' => length($$bodyref) } if $dolength;

    #-- $self->{rm}->{log}->log(12, "before_deBase64:");
    if (exists $$self{deBase64} && $self->{deBase64}->isit($bodyref)) { 
        $self->{deBase64}->doit($bodyref);
    }
    #-- $self->{rm}->{log}->log(12, "after_deBase64:");

    #$self->log2file($bodyref, "preproc.afta.debase64");
    $lengths->{'2_after_deBase64'} = length($$bodyref) if $dolength;

    #-- $self->{rm}->{log}->log(12, "before_deQP:");
    my $isQP;
    if (exists $$self{deQP} && ($isQP = $self->{deQP}->isit($bodyref))) { 
        $self->{deQP}->doit($bodyref);
    }
    #-- $self->{rm}->{log}->log(12, "after_deQP:");

    #$self->log2file($bodyref, "preproc.afta.deQP.$isQP");
    $lengths->{'3_after_deQP'} = length($$bodyref) if $dolength;

    #-- $self->{rm}->{log}->log(12, "before_deHTML:");
    if (exists $$self{deHTML} && $self->{deHTML}->isit($bodyref)) { 
        $self->{deHTML}->doit($bodyref);
    }
    #-- $self->{rm}->{log}->log(12, "after_deHTML:");

    #-- $self->{rm}->{log}->log(12, "before_deNewline:");
    if (exists $$self{deNewline}) { 
        $self->{deNewline}->doit($bodyref);
    }
    #-- $self->{rm}->{log}->log(12, "after_deNewline:");
    #$self->log2file($bodyref, "preproc.afta.deHTML");
    $lengths->{'4_after_deHTML'} = length($$bodyref) if $dolength;

    my ($hdr, $body) = split /\n\r*\n/, $$bodyref, 2;

    $$bodyref = $body;
    $lengths->{'5_after_header_removal'} = length($$bodyref) if $dolength;

    return $lengths;

}


sub log2file {
    my ($self, $msgref, $mailid) = @_;
    my $len = length($$msgref);
    my $fn = "/tmp/.razor.debug.msg.$$.$mailid";
    if (open OUT, ">$fn") {
        print OUT $$msgref;
        close OUT;
    } else {
    }
}


1;


