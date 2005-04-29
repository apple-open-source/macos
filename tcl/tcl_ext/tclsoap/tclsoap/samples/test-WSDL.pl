# test-WSDL.pl Copyright (C) 2002 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Check out TclSOAP's interop WSDL file using SOAP::Lite.
#
# If you are using an HTTP proxy you may need to set
# HTTP_proxy, HTTP_proxy_user, HTTP_proxy_pass and no_proxy
# in your environment.
#
# $Id: test-WSDL.pl,v 1.3 2003/09/06 17:08:46 patthoyts Exp $

use strict;
use SOAP::Lite;

interopBase();
interopB();
#interopC();

# -------------------------------------------------------------------------
# Base
# -------------------------------------------------------------------------

sub interopBase {
  my $service = SOAP::Lite
    ->service('http://localhost/tclsoap/WSDL/interop.wsdl');

  my $voidResponse = $service->echoVoid();
  $voidResponse = '' unless defined $voidResponse;
  print "echoVoid: \"$voidResponse\"\n";
  print "echoString: " . $service->echoString('Hello, TclSOAP') . "\n";
  print "echoInteger: " . $service->echoInteger(3) . "\n";
  print "echoFloat: " . $service->echoFloat(2.1) . "\n";
  
  my $strings = $service->echoStringArray(['Hello','Tcl','SOAP']);
  print "echoStringArray: \"" . join("\", \"", @$strings) . "\"\n";
  
  my  $ints = $service->echoIntegerArray([45, 2, -18, 0]);
  print "echoIntegerArray: " . join(', ', @$ints) . "\n";
  
  my $floats = $service->echoFloatArray([4.5, -2.0, 0, 3e-2]);
  print "echoFloatArray: " . join(', ', @$floats) . "\n";
  
  my $struct = $service->echoStruct({varInt=>120,
                                     varFloat=>3.1415,
                                     varString=>"TclSOAP"});
  PrintStruct("echoStruct", $struct);
}

# -------------------------------------------------------------------------
# Round 2 B
# -------------------------------------------------------------------------

sub interopB {
  my $service = SOAP::Lite
    ->service('http://localhost/tclsoap/WSDL/interopB.wsdl');

  my @simple = $service->echoStructAsSimpleTypes({varInt=>120,
                                                  varFloat=>3.1415,
                                                  varString=>"TclSOAP"});
  print "echoStructAsSimpleTypes: ( ";
  foreach my $item (@simple) {
    print "$item ";
  }
  print ")\n";

  my $struct = $service->echoSimpleTypesAsStruct(
           SOAP::Data->type('string')->name('inputString')->value('TclSOAP'),
           SOAP::Data->type('int')->name('inputInteger')->value(240),
           SOAP::Data->type('float')->name('inputFloat')->value(2.5));

  PrintStruct('echoSimpleTypesAsStruct', $struct);

  my $nested = $service->echoNestedStruct({
                                           varString=>'TclSOAP',
                                           varFloat=>3.1415,
                                           varInt=>120,
                                           varStruct=>{
                                                       varString=>'SOAP',
                                                       varFloat=>2.5,
                                                       varInt=>240
                                                      }
                                          });
  PrintStruct('echoNestedStruct', $nested);

  my $nested2 = $service->echoNestedArray({
                                           varString=>'TclSOAP',
                                           varFloat=>3.1415,
                                           varInt=>120,
                                           varArray=>['red', 'green', 'blue']
                                          });
  PrintStruct('echoNestedArray', $nested2);
}

# -------------------------------------------------------------------------
# Round 2 C
# -------------------------------------------------------------------------

sub interopC {
  my $service = SOAP::Lite
    ->service('http://localhost/tclsoap/WSDL/interopC.wsdl');
}

# -------------------------------------------------------------------------
# Utility functions
# -------------------------------------------------------------------------

sub PrintStruct {
  my ($name, $struct, $indent) = @_;
  $indent = 0 unless defined($indent);
  print ' ' x $indent . "$name: {\n";
  foreach my $membr (keys %$struct) {
    if (ref($struct->{$membr}) eq 'HASH') {
      PrintStruct($membr, $struct->{$membr}, $indent + 2);
    } elsif (ref($struct->{$membr}) eq 'ARRAY') {
      PrintArray($membr, $struct->{$membr}, $indent + 2);
    }
    else {
      print ' ' x $indent . "  $membr => $struct->{$membr}\n";
    }
  }
  print ' ' x $indent . "};\n";
}

sub PrintArray {
  my ($name, $array, $indent) = @_;
  $indent = 0 unless defined($indent);
  print ' ' x $indent . "$name: ( ";
  foreach my $item (@$array) {
    if (ref($item) eq 'HASH') {
      PrintStruct(' ', $item, $indent + 2);
    } elsif (ref($item) eq 'ARRAY') {
      PrintArray(' ', $item, $indent + 2);
    }
    else {
      print "$item ";
    }
  }
  print ")\n";
}

exit;

