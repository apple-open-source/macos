package TestSOAP;
use strict;
use warnings;

use base qw(Exporter);
our (@EXPORT_OK);
@EXPORT_OK = qw(make_soap);

sub make_soap {
  my ($soap_uri, $soap_proxy) = @_;
  unless (eval { require SOAP::Lite }) {
    print STDERR "SOAP::Lite is unavailable to make remote call\n"; 
    return;
  }

  return SOAP::Lite
    ->uri($soap_uri)
      ->proxy($soap_proxy,
              options => {compress_threshold => 10000})
        ->on_fault(sub { my($soap, $res) = @_; 
                         print STDERR "SOAP Fault: ", 
                           (ref $res ? $res->faultstring 
                                     : $soap->transport->status),
                           "\n";
                         return undef;
                       });
}

1;
