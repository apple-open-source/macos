# $Id: VR1.pm,v 1.1 2004/04/19 17:50:31 dasenbro Exp $

package Razor2::Preproc::VR1;


sub new { 
    return bless {}, shift;
}


sub doit {

    my ($self, $text) = @_;

    # Equivalent to Mail::Internet cleanup in Razor v1
    # $mail->tidy_body();
    # $mail->remove_sig (); 
    # $mail->tidy_body ();

    $$text =~ s/^[\s\n]+//sg;
    $$text =~ s/[\s\n]+$//sg;
    $$text =~ s/\n--\040?[\r\n]([^\n]*\n){0,8}[^\n]*$//g;
    $$text =~ s/[\s\n]+$//sg;
    $$text .= "\n";

}


1;

